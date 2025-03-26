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
#define PWM_BASE 0x00000200  // Hypothetical PWM controller address(need to discuss if we still want this)

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
//bool auto_mode = true; 

// 7-segment display patterns for digits 0-9
const unsigned char seven_seg_digits_decode[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67
};

// Function prototypes
void initialize_hardware(void); //function to initialize the hardware
void cleanup_hardware(void); //function to cleanup the memory mapping
int read_temperature(void);
void set_fan_status(bool status);
void update_fan_speed(int temp, int threshold);
void update_displays(void);
void process_user_input(void);

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
