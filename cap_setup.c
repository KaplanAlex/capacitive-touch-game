#include <msp430.h>

#include "cap_setup.h"

#define INTERRUPT_INTERVAL 8000            // Interrupt every .5ms for timing.

/*
 * Setup Clocks, Timers, and SPI protocol.
 */
void
setup()
{
    /* Stop the watchdog. */
    WDTCTL = WDTPW | WDTHOLD;
    
    /* Set the system clock to 1 MHz. */
    DCOCTL = CALDCO_16MHZ;
    BCSCTL1 = CALBC1_16MHZ;
    
    __bis_SR_register(GIE);                         // Enable interrupts.
    
    setup_spi();                                    // Setup the MSP430 to transmit over SPI
    
    /* Setup TA0 to generate interrupts. */
    TA0CTL |= TASSEL_2 + MC_1 + ID_0;               // Source from SMCLK, Up Mode
    
    TA0CCTL0 |= CCIE;                               // CCR0 interrupt enabled.
    TA0CCR0 = INTERRUPT_INTERVAL;                   // Interrupt in .5ms.
    
    /* Setup TA1 to generate interrupts. */
    TA1CTL |= TASSEL_2 + MC_1 + ID_0;               // Source from SMCLK, Up Mode
    
    TA1CCTL0 |= CCIE;                               // CCR0 interrupt enabled.
    TA1CCR0 = 16000;                                // Interrupt in 1ms.
    
    /* Set all capacitive buttons to inputs and enable pull up resistors. */
    /* Up - P2.2  Right - P2.3  Down - P2.4 Left - P2.5  Middle - P2.6 */
    P2DIR &= ~(BIT2 + BIT3 + BIT4 + BIT5 + BIT6);   // Set input direction

    /* Set all LEDs to output and intialize to off. */
    /* Up - P3.2  Right - P3.3  Down - P3.4  Left - P3.5  Middle P3.6 */
    P3DIR |= BIT2 + BIT3 + BIT4 + BIT5 + BIT6;
    P3OUT &= ~(BIT2 + BIT3 + BIT4 + BIT5 + BIT6);

    /* Initialize the PWM signal sent to the capacitive buttons.*/
    P2DIR |= BIT1;                            // P2.1 to output (corresponds to TA1CCR1).
    P2SEL &= ~BIT6;
    P2SEL2 &= ~BIT6;
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
    UCA0BR0 = 3;                                  // 16 MHz / 3 = .1875 us per bit
    UCA0BR1 = 0;
    
    P1DIR = BIT2 + BIT4;                          // Set P1.2 and P1.4 to output
    
    P1SEL =  BIT2 + BIT4;                         //Enable SPI communication
    P1SEL2 = BIT2 + BIT4;                         //P1.2 as MOSI and P1.4 as CLK
    
    UCA0CTL1 &= ~UCSWRST;                         //Initialize USCI state machine - active low
}
