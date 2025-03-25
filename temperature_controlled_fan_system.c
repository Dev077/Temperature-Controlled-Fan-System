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
#define LEDR_BASE 0x00000000
#define SW_BASE 0x00000040
#define KEY_BASE 0x00000050
#define HEX3_HEX0_BASE 0x00000020
#define HEX5_HEX4_BASE 0x00000030
#define ADC_BASE 0x00000100
#define PWM_BASE 0x00000200  // Hypothetical PWM controller address(need to discuss if we still want this)

// Global variables for memory-mapped I/O
volatile int *LEDR_ptr;
volatile int *SW_ptr;
volatile int *KEY_ptr;
volatile int *HEX3_HEX0_ptr;
volatile int *HEX5_HEX4_ptr;
volatile int *ADC_ptr;
volatile int *PWM_ptr;

// Application state variables
float current_temperature = 0.0; // variable to monitor the current temperature
float threshold_temperature = 25.0;  // Default threshold in Celsius(we can change this upon our requirements)
bool fan_state = false; 
int fan_speed = 0;  // 0-100 percentage

