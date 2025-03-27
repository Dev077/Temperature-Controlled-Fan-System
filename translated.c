#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Memory-mapped I/O physical addresses for DE10-Standard
#define FPGA_PERIPHERAL_BASE 0xFF200000

// Offsets for peripherals on DE10-Standard
#define LED_OFFSET 0x00000000         // LED base offset
#define SW_OFFSET 0x00000040          // Switch base offset
#define KEY_OFFSET 0x00000050         // Push button base offset
#define HEX3_HEX0_OFFSET 0x00000020   // 7-segment display (HEX3..HEX0) base offset
#define HEX5_HEX4_OFFSET 0x00000030   // 7-segment display (HEX5..HEX4) base offset
#define ADC_OFFSET 0x00004000         // ADC base offset (updated to match Lab 3 documentation)
#define JP1_OFFSET 0x00000060         // GPIO expansion port for green LEDs

// Define register pointers
volatile int* led_ptr;
volatile int* sw_ptr;
volatile int* key_ptr;
volatile int* hex3_hex0_ptr;
volatile int* hex5_hex4_ptr;
volatile int* adc_ptr;
volatile int* gpio_ptr;

// System parameters
int current_temperature = 0;     // Current temperature (will be read from ADC)
int threshold_temperature = 25;  // Default threshold in celsius
bool fan_status = false;         // Fan status (on/off)
bool auto_mode = true;           // Auto mode status
int fan_speed = 0;               // Fan speed (0-100%)
int selected_potentiometer = 0;  // Selected potentiometer (0 or 1)

// 7-segment display patterns for digits 0-9
const unsigned char seven_seg_digits_decode[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67
};

// Function to initialize memory-mapped I/O
void initialize_hardware(void) {
    // Set up pointers to memory-mapped I/O
    led_ptr = (volatile int*)(FPGA_PERIPHERAL_BASE + LED_OFFSET);
    sw_ptr = (volatile int*)(FPGA_PERIPHERAL_BASE + SW_OFFSET);
    key_ptr = (volatile int*)(FPGA_PERIPHERAL_BASE + KEY_OFFSET);
    hex3_hex0_ptr = (volatile int*)(FPGA_PERIPHERAL_BASE + HEX3_HEX0_OFFSET);
    hex5_hex4_ptr = (volatile int*)(FPGA_PERIPHERAL_BASE + HEX5_HEX4_OFFSET);
    adc_ptr = (volatile int*)(FPGA_PERIPHERAL_BASE + ADC_OFFSET);
    gpio_ptr = (volatile int*)(FPGA_PERIPHERAL_BASE + JP1_OFFSET);
    
    // Initialize peripherals
    *led_ptr = 0;            // Turn off all LEDs
    *gpio_ptr = 0;           // Configure GPIO port for green LEDs
    *(gpio_ptr + 1) = 0x3FF; // Set bits 0-9 as output (for 10 green LEDs)
    
    // Setup ADC for auto-update mode
    *(adc_ptr + 1) = 1;      // Write any value to Channel 1 register to enable auto-update
    
    printf("Hardware initialized\n");
}

// Read temperature from ADC
int read_temperature(void) {
    // Get potentiometer selection from SW0
    selected_potentiometer = *sw_ptr & 0x1;
    
    // Read ADC channel based on selected potentiometer (0 or 1)
    int adc_value = *(adc_ptr + selected_potentiometer) & 0xFFF; // 12-bit ADC value (0-4095)
    
    // Convert ADC value to temperature (0-4095 to 0-100°C)
    int temperature = (adc_value * 100) / 4095;
    
    return temperature;
}

// Set fan status (on/off)
void set_fan_status(bool status) {
    fan_status = status;
    
    // Use LED0 to indicate fan status
    if (status) {
        *led_ptr |= 0x1;  // Set bit 0
    } else {
        *led_ptr &= ~0x1; // Clear bit 0
        fan_speed = 0;    // Reset fan speed when turning off
    }
}

// Update fan speed based on temperature and threshold
void update_fan_speed(int temp, int threshold) {
    if (!auto_mode) return; // Manual mode - do nothing
    
    if (temp >= threshold) {
        // Temperature above threshold - turn fan on
        set_fan_status(true);
        
        // Calculate fan speed based on temperature difference
        int temp_diff = temp - threshold;
        fan_speed = (temp_diff > 20) ? 100 : (temp_diff * 5); // Max 100%
        
        // Update LEDs 1-9 to show fan speed
        unsigned int led_pattern = 0;
        for (int i = 0; i < (fan_speed / 10); i++) {
            led_pattern |= (1 << (i + 1));
        }
        *led_ptr = (*led_ptr & 0x1) | led_pattern; // Keep LED0 status
        
        // Update green LEDs on GPIO port to show temperature as bar graph
        int green_led_pattern = 0;
        int num_leds = (temp > 100) ? 10 : (temp / 10);
        for (int i = 0; i < num_leds; i++) {
            green_led_pattern |= (1 << i);
        }
        *gpio_ptr = green_led_pattern;
    } else if (temp < (threshold - 2)) {
        // Temperature below threshold-2 - turn fan off
        set_fan_status(false);
        fan_speed = 0;
        *led_ptr &= 0x1; // Clear all LEDs except LED0 status
        
        // Update green LEDs on GPIO port to show temperature as bar graph
        int green_led_pattern = 0;
        int num_leds = (temp > 100) ? 10 : (temp / 10);
        for (int i = 0; i < num_leds; i++) {
            green_led_pattern |= (1 << i);
        }
        *gpio_ptr = green_led_pattern;
    }
    // Otherwise maintain current status
}

// Update 7-segment displays
void update_displays(void) {
    // Display current temperature on HEX1-HEX0
    int temp_ones = current_temperature % 10;
    int temp_tens = (current_temperature / 10) % 10;
    unsigned int temp_display = (seven_seg_digits_decode[temp_tens] << 8) | seven_seg_digits_decode[temp_ones];
    
    // Display threshold temperature on HEX3-HEX2
    int thresh_ones = threshold_temperature % 10;
    int thresh_tens = (threshold_temperature / 10) % 10;
    unsigned int thresh_display = (seven_seg_digits_decode[thresh_tens] << 24) | (seven_seg_digits_decode[thresh_ones] << 16);
    
    // Combine and update HEX3-HEX0
    *hex3_hex0_ptr = temp_display | thresh_display;
    
    // Display fan status on HEX5-HEX4
    if (!fan_status) {
        // "OF" for Off
        *hex5_hex4_ptr = 0x3F5C; // 'O' and 'F'
    } else {
        // Fan speed percentage
        int fan_tens = fan_speed / 10;
        int fan_ones = fan_speed % 10;
        *hex5_hex4_ptr = (seven_seg_digits_decode[fan_tens] << 8) | seven_seg_digits_decode[fan_ones];
    }
}

// Process user input from switches and buttons
void process_user_input(void) {
    // Read switch values
    unsigned int sw_value = *sw_ptr;
    
    // SW0 selects potentiometer (already handled in read_temperature)
    
    // Use SW4-SW0 to set temperature threshold (1-31°C)
    int new_threshold = (sw_value >> 1) & 0x1F; // Shift right to skip SW0, then mask to get 5 bits
    if (new_threshold != 0) { // Avoid threshold of 0
        threshold_temperature = new_threshold;
    }
    
    // SW9 toggles auto/manual mode
    auto_mode = !(sw_value & 0x200); // SW9=1: manual, SW9=0: auto
    
    // Read push button values (active low, so we invert)
    unsigned int key_value = ~(*key_ptr) & 0xF;
    
    if (!auto_mode) {
        // In manual mode, use push buttons
        // KEY0: Turn fan on
        if (key_value & 0x1) {
            set_fan_status(true);
        }
        
        // KEY1: Turn fan off
        if (key_value & 0x2) {
            set_fan_status(false);
        }
        
        // KEY2: Increase fan speed
        if (key_value & 0x4) {
            fan_speed = (fan_speed >= 100) ? 100 : fan_speed + 10;
            // Update LEDs for fan speed
            unsigned int led_pattern = 0;
            for (int i = 0; i < (fan_speed / 10); i++) {
                led_pattern |= (1 << (i + 1));
            }
            *led_ptr = (*led_ptr & 0x1) | led_pattern;
        }
        
        // KEY3: Decrease fan speed
        if (key_value & 0x8) {
            fan_speed = (fan_speed <= 0) ? 0 : fan_speed - 10;
            // Update LEDs for fan speed
            unsigned int led_pattern = 0;
            for (int i = 0; i < (fan_speed / 10); i++) {
                led_pattern |= (1 << (i + 1));
            }
            *led_ptr = (*led_ptr & 0x1) | led_pattern;
        }
    }
}

// Simple delay function using a loop
void delay(int milliseconds) {
    volatile int i, j;
    for (i = 0; i < milliseconds; i++) {
        for (j = 0; j < 10000; j++) {
            // Empty loop to create delay
        }
    }
}

int main(void) {
    // Initialize hardware
    initialize_hardware();
    
    printf("Temperature-Controlled Fan System\n");
    printf("SW0: Select potentiometer (0 or 1)\n");
    printf("SW5-SW1: Set temperature threshold (1-31°C)\n");
    printf("SW9: ON=Manual mode, OFF=Auto mode\n");
    printf("In manual mode:\n");
    printf("  KEY0: Turn fan ON\n");
    printf("  KEY1: Turn fan OFF\n");
    printf("  KEY2: Increase fan speed\n");
    printf("  KEY3: Decrease fan speed\n");
    
    // Main control loop
    while (1) {
        // Read temperature from ADC
        current_temperature = read_temperature();
        
        // Process user input
        process_user_input();
        
        // Update fan status and speed
        update_fan_speed(current_temperature, threshold_temperature);
        
        // Update displays
        update_displays();
        
        // Short delay
        delay(200); // ~200ms delay
    }
    
    return 0; // Never reached
}