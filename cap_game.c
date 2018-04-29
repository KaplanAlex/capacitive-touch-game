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
#define NUM_LEDS 120

// LED colors
#define OFF   0
#define RED   1
#define GREEN 2
#define BLUE  3
#define GREEN 4
#define GREEN 5
#define GREEN 6
#define GREEN 7


// Game states
#define START 0
#define PLAY 1
#define WIN 2
#define LOSE 3



// Prototypes.
void start_spi(uint8_t *transmit, unsigned int count);
void leds_from_press();
void refresh_board();
void led_to_spi(uint8_t led);
void game_fsm();

// SPI
unsigned int spi_led_idx = 0;
unsigned int data_len = 24;               // The number of bytes to be transmitted
unsigned int frame_idx = 0;                // Track transmission count in interrupts
uint8_t *dataptr;                         // Pointer which traverses the tx information
uint8_t spi_start_frame[] = {0x00, 0x00, 0x00, 0x00};
uint8_t general_frame[] = {0xE1, 0x00, 0x00, 0x00};
uint8_t spi_end_frame[] = {0xFF, 0xFF, 0xFF, 0xFF};
// Capacitive Sensing
/* Pressed Buttons: 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle */
uint8_t button_state = 0x00;


// Represent every LED with one byte.
uint8_t led_board[NUM_LEDS];

// Game state
current_state = START;


int
main(void)
{
    // Initialize the led_board.
    int i;
    for (i = 0; i < NUM_LEDS; i++) {
        led_board[i] = 0x00;
    }
    
    // Initialize clocks, timers, and SPI protocol.
    setup();
    
    while (1) {
        leds_from_press();
        refresh_board();
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

/* Convert led color from a single byte representation
 * to the 4 byte format expected by the APA102 LEDs.
 *
 * Led is a byte in led_board.
 */
void
led_to_spi(uint8_t led)
{
    // Clear the general frame, leaving brightness.
    int i;
    for (i = 3; i > 0; i--) {
        general_frame[i] = 0x00;
    }
    
    // Update the general frame to contain the color encoded in the byte.
    switch (led) {
        // Off
        case RED:
            general_frame[RED] = 0xFF;
            break;
        // Green
        case GREEN:
            general_frame[GREEN] = 0xFF;
            break;
        // Blue
        case BLUE:
            general_frame[BLUE] = 0xFF;
            break;
        default:
            break;
    }
    
}


void
refresh_board()
{
    spi_led_idx = 0;
    start_spi(spi_start_frame, 4);
}


/* Interrupt driven SPI transfer */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCIA0TX_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCIAB0TX_VECTOR))) USCIA0TX_ISR (void)
#else
#error Compiler not supported!
#endif
{
    
    
    //There are more bytes in the current frame to transmit.
    if (frame_idx > 0) {
        UCA0TXBUF = *dataptr;   // Transmit the next byte.
        dataptr++;              // Move the pointer to the next byte.
        frame_idx--;
    } else {
        // Send the next LED.
        if (spi_led_idx < NUM_LEDS) {
            // Update the general frame to transmit the next led.
            led_to_spi(led_board[spi_led_idx]);
            start_spi(general_frame, 4);
            
            spi_led_idx++;
        } else if (spi_led_idx == NUM_LEDS) {
            // Start endframe tx.
            start_spi(spi_end_frame, 4);
            spi_led_idx++;
        } else {
            /* There is no more data to transmit. Clear the interrupt flag
             * to exit.
             */
            IFG2 &= ~UCA0TXIFG;
        }
    }
   
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

