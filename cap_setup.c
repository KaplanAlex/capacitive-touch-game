#include <msp430.h>

#include "cap_setup.h"

#define INTERRUPT_INTERVAL 100            // Interrupt every .1ms for timing.

/*
 * Setup Clocks, Timers, and SPI protocol.
 */
void
setup()
{
    /* Stop the watchdog. */
    WDTCTL = WDTPW | WDTHOLD;
    
    /* Set the system clock to 1 MHz. */
    DCOCTL = CALDCO_1MHZ;
    BCSCTL1 = CALBC1_1MHZ;
    
    __bis_SR_register(GIE);                         // Enable interrupts.
    
    setup_spi();                                    // Setup the MSP430 to transmit over SPI
    
    /* Setup TA0 to generate 1ms interrupts. */
    TA0CTL |= TASSEL_2 + MC_2 + ID_0;               // Source from SMCLK, Continuous Mode
    
    TA0CCTL0 |= CCIE;                               // CCR0 interrupt enabled.
    TA0CCR0 = INTERRUPT_INTERVAL;                   // Interrupt in .1ms.
    
    /* Set all capacitive buttons to inputs and enable pull up resistors. */
    /* Up - P2.2  Right - P2.3  Down - P2.4 Left - P2.5  Middle - P2.6 */
    P2DIR &= ~(BIT2 + BIT3 + BIT4 + BIT5 + BIT6);   // Set input direction
    P2REN |= BIT2 + BIT3 + BIT4 + BIT5 + BIT6;      // Enable pull up/down resistors.
    P2OUT |= BIT2 + BIT3 + BIT4 + BIT5 + BIT6;      // Set pull UP resistor.
    
    /* Set all LEDs to output and intialize to off. */
    /* Up - P3.2  Right - P3.3  Down - P3.4  Left - P3.5  Middle P3.6 */
    P3DIR |= BIT2 + BIT3 + BIT4 + BIT5 + BIT6;
    P3OUT &= ~(BIT2 + BIT3 + BIT4 + BIT5 + BIT6);
    
    /* Initialize the PWM signal sent to the capacitive buttons.*/
    P2DIR |= BIT1;                            // P2.1 to output (corresponds to TA1CCR1).
    P2SEL |= BIT1;                            // Set the function of P2.1 to PWM
    P2SEL2 &= ~BIT1;
    
    TA1CTL |= TASSEL_2 + MC_1 + ID_0;         // Source from SMCLK, Up mode (to TA1CCR0).
    TA1CCTL0 |= CCIE;                         // CCR0 interrupt enable for PWM sent flag.
    
    /* Set PWM to toggle on and off with 10 millisecond period. Period needs to be long enough to prevent
     * overlapping pulses when capacitance is large.
     */
    TA1CCR0 = 10000;
    TA1CCR1 = 2000;                         // Off at CCR1. Needs to be long on enough to be detected.
    TA1CCTL1 = OUTMOD_7;                    // Set the PWM mode to toggle on at CCR0 and reset at CCR1.
    
}

/*
 * Configures the MSP430 to use USCI Module A to transmit SPI
 * Sets P1.2 as MOSI and P1.4 as CLK.
 */
void
setup_spi()
{
    UCA0CTL1 |= UCSWRST;                          // Put USCI_B state machine in reset
    UCA0CTL0 |= UCCKPH | UCMSB | UCMST | UCSYNC;  // 3-pin, 8-bit MSB first, SPI master
    //Bit is captured on the rising edge and changed on falling
    
    UCA0MCTL = 0;                                 // No modulation
    UCA0CTL1 |= UCSSEL_2;                         // SMCLK
    UCA0BR0 = 0x00;                               // Dont divide the clock further
    UCA0BR1 = 0x00;
    
    P1DIR = BIT2 + BIT4;                          // Set P1.2 and P1.4 to output
    
    P1SEL =  BIT2 + BIT4;                         //Enable SPI communication
    P1SEL2 = BIT2 + BIT4;                         //P1.2 as MOSI and P1.4 as CLK
    
    UCA0CTL1 &= ~UCSWRST;                         //Initialize USCI state machine - active low
    
    IE2 |= UCA0TXIE;                              // Enable USCI0 TX interrupt
}
