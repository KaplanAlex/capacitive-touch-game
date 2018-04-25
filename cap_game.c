/********************************************************************
 * Capacitive Touch Sliding Block Game
 * ELEC 327 - Final Project
 * Alex Kaplan (adk6)   Alfonso Morera (am108)  Neil Seoni (nas10)
 *
 * Main File
 * Contains all global variables which are passed by pointers to other
 * files. This includes:
 *
 ********************************************************************/
#include "cap_setup.h"
#include <msp430.h>
#include <stdint.h>

#define INTERRUPT_INTERVAL 100            // Interrupt every .1ms for timing.

// SPI
uint8_t *dataptr;                         // Pointer which traverses the tx information
unsigned int data_len = 24;               // The number of bytes to be transmitted
unsigned int tx_count = 0;                // Track transmission count in interrupts

// Capacitive Sensing
unsigned int pulse_time = 0;
uint8_t pulse_rx = 0;

// Random initial colors - TODO initialize...
uint8_t colors[] = {0x00, 0x00, 0x00, 0x00, \
    0xE1, 0x00, 0x00, 0xFF, \
    0xE1, 0x00, 0xFF, 0x00, \
    0xE1, 0x09, 0x9F, 0xFF, \
    0xE1, 0xFF, 0x00, 0x00, \
    0xFF, 0xFF, 0xFF, 0xFF};

int main(void)
{
    
    setup();                            // Initialize clocks, timers, and SPI protocol.
    start_spi(colors, data_len);
    
}


/* Writes the first bit of data via spi, triggering the spi
 * interrupt. Sets a global pointer and count which are accessed by the
 * spi driven interrupt.
 *
 * transmit - a pointer to a byte (array of bytes to be transmitted).
 * count    - the number of bytes to be transmitted.
 */
void
start_spi(unsigned int *transmit, unsigned int count)
{
    //Transmit the first byte and move the pointer to the next byte
    dataptr = transmit;            // Set the data pointer for the spi interrupt.
    tx_count = count - 1;          // Set the global count for the spi interrupt.
    UCA0TXBUF = *dataptr;          // Write the first byte to the buffer and send.
    dataptr++;
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
        /* There are no remaining bytes to transmit.
         * Clear the interrupt flag to exit. */
        IFG2 &= ~UCA0TXIFG;
    }
}

/* Timer A0 interrupt service routine for timing. */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) Timer_A1 (void)
#else
#error Compiler not supported!
#endif
{
    
    TA0CCR0 += INTERRUPT_INTERVAL;           // Re-trigger the interrupt in another interavl.
    // A new pulse has been sent.
    if (!pulse_time) {
        // debounce();
        pulse_time++;
    }
    
    
    /* Why is this difficult?
     * 1. Make sure that pulse dont overlap - i.e. that we don't match a received pulse with a really old sent pulse. this is solved by making sure the wait time is long enough (small duty cycle?) for full pulse to progagate (issue for very large capacitance)
     
        Also needs to be long enough to be detected!
        
        Its probably ok but... we could do something where we dont send another pulse until the last one is received? More robust.
     
     * 2. Are we debouncing? If we are then we have to do it for every button. This is annoying because we have to do it based on received time, which will be different for all buttons even when some are pressed.
                - *Potential solution*:
                    Have a byte for every button and increment the count in the byte.
                    Dependent on if that button has received yet (another byte).
                    When pulse time is reset to 0, pass the received byte to debounce and 
                    use that to update debounced state (gotta come up with threshold also).
     
     * 3. Do we need to debounce this receive also? I guess it couldn't hurt. Its just a lot of 
          steps. I think we have time for it though.
     *
     *
     */
    
    __bic_SR_register_on_exit(LPM0_bits);    // Exit low power mode 0.
}


/* Timer A1 interrupt service routine - called on pulse send. */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER0_A1_VECTOR
__interrupt void Timer_A1 (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) Timer_A1 (void)
#else
#error Compiler not supported!
#endif
{
    // Reset time from pulse send to receive to zero and pulse receive flag.
    pulse_time = 0;
}

