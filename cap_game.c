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

// Prototypes.
void start_spi(uint8_t *transmit, unsigned int count);
void leds_from_press();

// SPI
uint8_t *dataptr;                         // Pointer which traverses the tx information
unsigned int data_len = 24;               // The number of bytes to be transmitted
unsigned int tx_count = 0;                // Track transmission count in interrupts

// Capacitive Sensing

/* Pressed Buttons: 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle */
uint8_t button_state = 0x00;


// Random initial colors - TODO initialize...
uint8_t colors[] = {0x00, 0x00, 0x00, 0x00, \
    0xE1, 0x00, 0x00, 0xFF, \
    0xE1, 0x00, 0xFF, 0x00, \
    0xE1, 0x09, 0x9F, 0xFF, \
    0xE1, 0xFF, 0x00, 0x00, \
    0xFF, 0xFF, 0xFF, 0xFF};

int main(void)
{
    setup();                              // Initialize clocks, timers, and SPI protocol.
    //start_spi(colors, data_len);
    while (1) {
        leds_from_press();
        __bis_SR_register(LPM0_bits);     //Enters low power mode for 1ms for fun.
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
    tx_count = count - 1;          // Set the global count for the spi interrupt.
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
    //There are more bytes to transmit
    if (tx_count > 0) {
        UCA0TXBUF = *dataptr;   // Transmit the next byte.
        dataptr++;              // Move the pointer to the next byte.
        tx_count--;
    } else {
        /*o transmit.
         * Clear the interrupt flag to exit. */
        IFG2 &= ~UCA0TXIFG;
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

