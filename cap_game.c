/********************************************************************
 * Capacitive Touch Sliding Block Game
 * ELEC 327 - Final Project
 * Alex Kaplan (adk6)   Alfonso Morera (am108)  Neil Seoni (nas10)
 *
 * Main File
 * Contains all global variables which are passed by pointers to other
 * files. This includes:
 *
 * button_state: the pressed state of the each capacitive button.
 *
 ********************************************************************/
#include <msp430.h>
#include <stdint.h>

#include "cap_sense.h"
#include "cap_setup.h"

//#define INTERRUPT_INTERVAL 500            // Interrupt every .1ms for timing.
#define NUM_LEDS 64

// Transmit codes to send long pulse (1) and short pulse (0).
#define HIGH_CODE   (0xF0)      // b11110000
#define LOW_CODE    (0xC0)      // b11000000

// LED colors
#define OFF   0
#define RED   1
#define GREEN 2
#define BLUE  3


// Game states
#define START 0
#define PLAY 1
#define WIN 2
#define LOSE 3

// WS2812 takes GRB format
typedef struct {
    u_char green;
    u_char red;
    u_char blue;
} LED;


// Prototypes.
void start_spi(uint8_t *transmit, unsigned int count);
void setLEDColor(u_int p, u_char r, u_char g, u_char b);
void clearStrip();
void fillStrip(u_char r, u_char g, u_char b);
void gradualFill(u_int n, u_char r, u_char g, u_char b);


void leds_from_press();
void refresh_board();

void game_fsm();

// SPI
//unsigned int spi_led_idx = 0;
unsigned int data_len = 24;               // The number of bytes to be transmitted
unsigned int frame_idx = 0;                // Track transmission count in interrupts
uint8_t *dataptr;                         // Pointer which traverses the tx information



// Capacitive Sensing
/* Pressed Buttons: 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle */
uint8_t button_state = 0x00;


// Represent every LED with one byte.
static LED led_board[NUM_LEDS];

// Game state
current_state = START;


int
main(void)
{
    // Initialize the led_board.
    int i;
    for (i = 0; i < NUM_LEDS; i++) {
        led_board[i].red = 0x00;
        led_board[i].green = 0x00;
        led_board[i].blue = 0x00;
    }
    
    // Initialize clocks, timers, and SPI protocol.
    setup();
    
    //    while (1) {
    //        leds_from_press();
    //        refresh_board();
    //    }
    
    // set strip color red
    fillStrip(0xFF, 0x00, 0x00);
    
    // show the strip
    refresh_board();
    
    // gradually fill for ever and ever
    while (1) {
        gradualFill(NUM_LEDS, 0x00, 0xFF, 0x00);  // green
        gradualFill(NUM_LEDS, 0x00, 0x00, 0xFF);  // blue
        gradualFill(NUM_LEDS, 0xFF, 0x00, 0xFF);  // magenta
        gradualFill(NUM_LEDS, 0xFF, 0xFF, 0x00);  // yellow
        gradualFill(NUM_LEDS, 0x00, 0xFF, 0xFF);  // cyan
        gradualFill(NUM_LEDS, 0xFF, 0x00, 0x00);  // red
    }
}


/* Iterates over the game FSM.
 *
 */
void
game_fsm()
{
    switch(current_state) {
            
    }
}


/* Writes the first bit of data via spi, triggering the spi
 * interrupt. Sets a global pointer and count which are accessed by the
 * spi driven interrupt.
 *
 * transmit - a pointer to a byte (array of bytes to be transmitted).
 * count    - the number of bytes to be transmitted.
 */
void
start_spi(uint8_t *transmit, unsigned int count)
{
    //Transmit the first byte and move the pointer to the next byte
    dataptr = transmit;            // Set the data pointer for the spi interrupt.
    frame_idx = count - 1;          // Set the global count for the spi interrupt.
    UCA0TXBUF = *dataptr;          // Write the first byte to the buffer and send.
    dataptr++;
}

/* Turns on LEDs on the board corrsponding to pressed buttons to illustrate
 * capacitive sense detected pressed.
 */
void
leds_from_press()
{
    uint8_t local_pressed = button_state;
    
    // Shift down 2 bits to match the output pins.
    P3OUT &= 0x00;
    local_pressed <<= 1;
    local_pressed <<= 1;
    P3OUT |= local_pressed;
}



// Sets the color of a certain LED (0 indexed)
void setLEDColor(u_int p, u_char r, u_char g, u_char b) {
    led_board[p].green = g;
    led_board[p].red = r;
    led_board[p].blue = b;
}

// Clear the color of all LEDs (make them black/off)
void clearStrip() {
    fillStrip(0x00, 0x00, 0x00);  // black
}

// Fill the strip with a solid color. This will update the strip.
void fillStrip(u_char r, u_char g, u_char b) {
    int i;
    for (i = 0; i < NUM_LEDS; i++) {
        setLEDColor(i, r, g, b);  // set all LEDs to specified color
    }
    refresh_board();  // refresh strip
}


void gradualFill(u_int n, u_char r, u_char g, u_char b) {
    int i;
    for (i = 0; i < n; i++){        // n is number of LEDs
        setLEDColor(i, r, g, b);
        refresh_board();
        __delay_cycles(1000000);       // lazy delay
    }
}


void
refresh_board()
{
    __bic_SR_register(GIE);  // disable interrupts
    
    // send RGB color for every LED
    unsigned int i, j;
    for (i = 0; i < NUM_LEDS; i++) {
        u_char *rgb = (u_char *)&led_board[i]; // get GRB color for this LED
        
        // send green, then red, then blue
        for (j = 0; j < 3; j++) {
            u_char mask = 0x80;    // b1000000
            
            // check each of the 8 bits
            while (mask != 0) {
                while (!(IFG2 & UCA0TXIFG))
                    ;    // wait to transmit
                if (rgb[j] & mask) {        // most significant bit first
                    UCA0TXBUF = HIGH_CODE;  // send 1
                } else {
                    UCA0TXBUF = LOW_CODE;   // send 0
                }
                
                mask >>= 1;  // check next bit
            }
        }
    }
    
    // send RES code for at least 50 us (800 cycles at 16 MHz)
    __delay_cycles(800);
    
    __bis_SR_register(GIE);    // enable interrupts
}





/* Timer A0 interrupt service routine for timing. */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) Timer_A0 (void)
#else
#error Compiler not supported!
#endif
{
    
    /* Track pulse rx time and update button pressed state upon new pulse transmission. */
    check_pulse(&button_state);
    __bic_SR_register_on_exit(LPM0_bits);    // Exit low power mode 0.
}
