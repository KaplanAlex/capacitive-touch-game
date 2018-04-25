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
#define PRESS_THRESHOLD    20             // Minimum of 20 .1ms delay to register press.

// Prototypes.
void raw_button_state();
void check_pulse();
void start_spi(uint8_t *transmit, unsigned int count);
void leds_from_press();

// SPI
uint8_t *dataptr;                         // Pointer which traverses the tx information
unsigned int data_len = 24;               // The number of bytes to be transmitted
unsigned int tx_count = 0;                // Track transmission count in interrupts

// Capacitive Sensing
unsigned int pulse_time = 0;

/* Button state variables */
/* 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle */
uint8_t pulse_rx = 0x00;                  // Flags representing propagated pulse rx.
uint8_t button_state = 0x00;              // Flags representing current button press state.

/* Pulse receive time from each capacitive button (in .1ms). */
uint8_t rx_times[5] = {0x00, 0x00, 0x00, 0x00, 0x00};


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
    local_pressed <<= 2;
    P3OUT &= local_pressed;
}


/* Read from P2IN to detect pin input voltage and store the state of all
 * capacitive puttons in the 5 LSBs of a single byte to recognize received
 * pulses.
 *
 * Update the global variable "pulse_rx" to reflect received pulses.
 * pulse_rx - Ordered from LSB to MSB: Up, Right, Down, Left, Middle.
 */
void
raw_button_state()
{
    uint8_t raw_state = 0;
    uint8_t switch_vals = ~P2IN;     // Flip P2IN so that switches are high when pressed
    
    /* Up - P3.2  Right - P3.3  Down - P3.4  Left - P3.5  Middle P3.6 */
    raw_state |= switch_vals & BIT2;
    raw_state |= switch_vals & BIT3;
    raw_state |= switch_vals & BIT4;
    raw_state |= switch_vals & BIT5;
    raw_state |= switch_vals & BIT6;
    
    /* Shift raw_state down to LSB. */
    raw_state >>= 2;
    
    /* Update the received pulse flags. */
    pulse_rx |= raw_state;
}


/* Detects and handles capacitive touch PWM pulse reception.
 * Triggered by TA0 interrupt.
 * Upon new pulse transmission ("pulse_time" is reset to 0), the rx_times of the
 * previous pulses are filtered to updated the global "button_state" representing
 * button presses in the order:
 * 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle.
 *
 * Otherwise, detect new pulse reception and update rx_times for every unreceived pulse.
 */
void
check_pulse()
{
    // A new pulse has been sent.
    if (!pulse_time) {
        // Convert previous rx_times to button press state.
        uint8_t new_state = 0x00;
        
        /* Update button press state based on previous rx times.
         * 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle
         */
        new_state |= (rx_times[4] > PRESS_THRESHOLD);
        new_state <<= 1;
        
        new_state |= (rx_times[3] > PRESS_THRESHOLD);
        new_state <<= 1;
        
        new_state |= (rx_times[2] > PRESS_THRESHOLD);
        new_state <<= 1;
        
        new_state |= (rx_times[1] > PRESS_THRESHOLD);
        new_state <<= 1;
        
        new_state |= (rx_times[0] > PRESS_THRESHOLD);
        button_state &= new_state;
        
        // Reset pulse rx flags and times.
        pulse_rx = 0x00;
        int rx_idx;
        for (rx_idx = 4; rx_idx >= 0; rx_idx--) {
            rx_times[rx_idx] = 0x01;
        }

        pulse_time++;
        return;
    }
    
    // Update pulse_rx to reflect all received pulses.
    raw_button_state();
    
    /* Increment rx_times for unreceived pulses until next pulse is sent.
     * 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle
     */
    if (pulse_rx & BIT0) rx_times[0]++;
    if (pulse_rx & BIT1) rx_times[1]++;
    if (pulse_rx & BIT2) rx_times[2]++;
    if (pulse_rx & BIT3) rx_times[3]++;
    if (pulse_rx & BIT4) rx_times[4]++;
    
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
    
    /* Track pulse rx time and update button pressed state upon new pulse transmission. */
    check_pulse();
    __bic_SR_register_on_exit(LPM0_bits);    // Exit low power mode 0.
}


/* Timer A1 interrupt service routine - called on pulse send. */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER0_A1_VECTOR
__interrupt void Timer_A1 (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_A1_VECTOR))) Timer_A (void)
#else
#error Compiler not supported!
#endif
{
    // Reset time from pulse send to receive to zero and pulse receive flag.
    pulse_time = 0;
}
