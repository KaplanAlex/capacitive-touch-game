#include <msp430.h>
#include <stdint.h>

int main(void)
{
    setup();
    
}



/*
 * Setup Clocks, Timers, and SPI protocol.
 */
void setup()
{
    /* Stop the watchdog. */
    WDTCTL = WDTPW | WDTHOLD;
    
    /* Set the system clock to 1 MHz. */
    DCOCTL = CALDCO_1MHZ;
    BCSCTL1 = CALBC1_1MHZ;
    
    __bis_SR_register(GIE);                   // enable interrupts
    
    setup_spi();                               // Setup the MSP430 to transmit over SPI
    
    /* Setup TA0 to generate 1ms interrupts. */
    TA0CTL |= TASSEL_2 + MC_2 + ID_0;          // Source from SMCLK count up to CCR0 (Continuous Mode)
    
    TA0CCTL0 |= CCIE;                          // CCR0 interrupt enabled
    TA0CCR0 = 1000;                             // Interrupt in .1ms
    
    /* Set all capacitive buttons to inputs and enable pull up resistors */
    /* Switch 1 P2.0    Switch 2 P2.2   Switch 3 P2.3   Switch 4 P2.4 */
    P2DIR &= ~(BIT0 + BIT2 + BIT3 + BIT4);      // Set input direction
    P2REN |= BIT0 + BIT2 + BIT3 + BIT4;         // Enable pull up/down resistor
    P2OUT |= BIT0 + BIT2 + BIT3 + BIT4;         // Set pull UP resistor.
}

/*
 * Configures the MSP430 to use USCI Module A to transmit SPI
 * Sets P1.2 as MOSI and P1.4 as CLK.
 */
void setup_spi()
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









//Timer A0 interrupt service routine for timing
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) Timer_A1 (void)
#else
#error Compiler not supported!
#endif
{
    debounce();
    TA0CCR0 += 1000; // Re-trigger the interrupt in one ms
    __bic_SR_register_on_exit(LPM0_bits);  // Exit low power mode 0
}

