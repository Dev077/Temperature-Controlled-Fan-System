//3375 Design Project
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

// Memory mapping addresses for DE10-Standard
#define LW_BRIDGE_BASE 0xFF200000
#define LW_BRIDGE_SPAN 0x00200000

// Hardware component addresses (offsets from base)(also need to double check if they're correct)
#define LED_BASE 0x00000000
#define SW_BASE 0x00000040
#define KEY_BASE 0x00000050
#define HEX3_HEX0_BASE 0x00000020
#define HEX5_HEX4_BASE 0x00000030
#define ADC_BASE 0x00000100
//#define PWM_BASE 0x00000200  // Hypothetical PWM controller address(for full hardware implementation)

// Global pointers for memory-mapped I/O
void *virtual_base; //base pointer for our memory-mapped region.
volatile int *led_ptr;
volatile int *sw_ptr;
volatile int *key_ptr;
volatile int *HEX3_HEX0_ptr;
volatile int *HEX5_HEX4_ptr;
volatile int *ADC_ptr;
volatile int *PWM_ptr;

// Application state variables
float current_temperature = 0.0; // variable to monitor the current temperature
float threshold_temperature = 25.0;  // Default threshold in Celsius(we can change this upon our requirements)
bool fan_state = false; 
int fan_speed = 0;  // 0-100 percentage
bool auto_mode = true; 

// 7-segment display patterns for digits 0-9
const unsigned char seven_seg_digits_decode[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67
};

// Function prototypes
void initialize_hardware(void); //function to initialize the hardware
void cleanup_hardware(void); //function to cleanup the memory mapping
int read_temperature(void); //function for temperature sensing
void set_fan_status(bool status); //function for turning on/off the fan
void update_fan_speed(int temp, int threshold); // function for fan speed control
void update_displays(void); //function to update the 7-segment displays
void process_user_input(void); //function to process user input

void initialize_hardware(void) {
    int fd;
    
    // Open /dev/mem device
    if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1) {
        printf("ERROR: could not open /dev/mem\n");
        exit(1);
    }
    
    // Map physical memory into virtual address space
    virtual_base = mmap(NULL, LW_BRIDGE_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, LW_BRIDGE_BASE);
    if (virtual_base == MAP_FAILED) {
        printf("ERROR: mmap() failed\n");
        close(fd);
        exit(1);
    }
    
    // Get pointers to memory-mapped I/O peripherals
    led_ptr = (unsigned int *)(virtual_base + LED_BASE);
    sw_ptr = (unsigned int *)(virtual_base + SW_BASE);
    key_ptr = (unsigned int *)(virtual_base + KEY_BASE);
    HEX3_HEX0_ptr = (unsigned int *)(virtual_base + HEX3_HEX0_BASE);
    HEX5_HEX4_ptr = (unsigned int *)(virtual_base + HEX5_HEX4_BASE);
    ADC_ptr = (unsigned int *)(virtual_base + ADC_BASE);
    
    // Initialize peripherals as needed
    *led_ptr = 0; // Turn off all LEDs
    
    close(fd); // Can close file descriptor after mapping
}

void cleanup_hardware(void) {
    // Clean up memory mapping when done
    if (munmap(virtual_base, LW_BRIDGE_SPAN) != 0) {
        printf("ERROR: munmap() failed\n");
    }
}

int read_temperature(void) {
    // ADC channel for temperature reading (using channel 0)
    int channel = 0;
    
    // Start ADC conversion
    *(ADC_ptr + 0) = 0x1;    // Write 1 to start ADC conversion
    
    // Wait for conversion to complete
    while ((*(ADC_ptr + 0) & 0x80) == 0); // Wait for ADC to complete
    
    // Read ADC result
    int adc_value = *(ADC_ptr + channel + 1) & 0xFFF; // 12-bit ADC
    
    // Convert ADC value to temperature (simple linear mapping)
    // Assuming ADC range 0-4095 maps to temperature range 0-100°C
    int temperature = (adc_value * 100) / 4095;
    
    return temperature;
}

void set_fan_status(bool status) {
    fan_state = status;
    
    // Use LED0 to indicate fan status (ON/OFF)
    if (status) {
        *led_ptr |= 0x1;  // Turn on LED0
    } else {
        *led_ptr &= ~0x1; // Turn off LED0
    }
}

void update_fan_speed(int temp, int threshold) {
    if (!auto_mode) return; // Do nothing if not in auto mode
    
    if (temp >= threshold) {
        // Turn fan on if temperature is above threshold
        set_fan_status(true);
        
        // Calculate fan speed based on how much temperature exceeds threshold
        int temp_diff = temp - threshold;
        // Map excess temperature to fan speed (0-100%)
        fan_speed = (temp_diff > 20) ? 100 : (temp_diff * 5);
        
        // Represent fan speed using LEDs 1-9 (LED0 is for fan status)
        unsigned int led_pattern = 0;
        for (int i = 0; i < (fan_speed / 10); i++) {
            led_pattern |= (1 << (i + 1));
        }
        *led_ptr = (*led_ptr & 0x1) | led_pattern; // Keep LED0 status
    } else {
        // If temperature falls 2 degrees below threshold, turn fan off
        if (temp < (threshold - 2)) {
            set_fan_status(false);
            fan_speed = 0;
            *led_ptr &= 0x1; // Keep only LED0 status
        }
        // Otherwise keep current status
    }
}

void update_displays(void) {
    // Display current temperature on HEX1-HEX0
    int temp_ones = ((int)current_temperature) % 10;
    int temp_tens = ((int)(current_temperature / 10)) % 10;
    unsigned int temp_display = (seven_seg_digits_decode[temp_tens] << 8) | seven_seg_digits_decode[temp_ones];
    
    // Display threshold temperature on HEX3-HEX2
    int thresh_ones = ((int)threshold_temperature) % 10;
    int thresh_tens = ((int)(threshold_temperature / 10)) % 10;
    unsigned int thresh_display = (seven_seg_digits_decode[thresh_tens] << 24) | (seven_seg_digits_decode[thresh_ones] << 16);
    
    // Update the displays
    *HEX3_HEX0_ptr = temp_display | thresh_display;
    
    // Display fan status and speed on HEX5-HEX4
    int fan_tens = fan_speed / 10;
    int fan_ones = fan_speed % 10;
    
    // If fan is off, show "OF"
    if (!fan_state) {
        // "OF" for Off (HEX5-HEX4)
        *HEX5_HEX4_ptr = 0x3F5C; // 'O' and 'F'
    } else {
        // Show fan speed percentage
        *HEX5_HEX4_ptr = (seven_seg_digits_decode[fan_tens] << 8) | seven_seg_digits_decode[fan_ones];
    }
}

void process_user_input(void) {
    // Read switch values
    unsigned int sw_value = *sw_ptr;
    
    // Use switches to set temperature threshold (SW4-SW0)
    int new_threshold = sw_value & 0x1F; // Use lower 5 bits for values 0-31
    if (new_threshold != 0) { // Avoid threshold of 0
        threshold_temperature = new_threshold;
    }
    
    // Use SW9 to toggle auto/manual mode
    auto_mode = !(sw_value & 0x200); // SW9 = 1: manual mode, SW9 = 0: auto mode
    
    // Use push buttons for manual control
    unsigned int key_value = *key_ptr;
    if (!auto_mode) {
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
            // Update LEDs to show fan speed
            unsigned int led_pattern = 0;
            for (int i = 0; i < (fan_speed / 10); i++) {
                led_pattern |= (1 << (i + 1));
            }
            *led_ptr = (*led_ptr & 0x1) | led_pattern; // Keep LED0 status
        }
        
        // KEY3: Decrease fan speed
        if (key_value & 0x8) {
            fan_speed = (fan_speed <= 0) ? 0 : fan_speed - 10;
            // Update LEDs to show fan speed
            unsigned int led_pattern = 0;
            for (int i = 0; i < (fan_speed / 10); i++) {
                led_pattern |= (1 << (i + 1));
            }
            *led_ptr = (*led_ptr & 0x1) | led_pattern; // Keep LED0 status
        }
    }
}