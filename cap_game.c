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
    setup();
    start_spi(colors, data_len);
    
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
    // debounce();
    TA0CCR0 += INTERRUPT_INTERVAL;           // Re-trigger the interrupt in another interavl.
    // A new pulse has been sent.
    if (!pulse_time) {
        
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
          steps.
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

