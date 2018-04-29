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
#include "timing_funcs.h"

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


// Board size
#define COLUMNS 8
#define ROWS 8

// WS2812 LEDs require GRB format
typedef struct {
    unsigned char green;
    unsigned char red;
    unsigned char blue;
} LED;


// Prototypes.
void start_spi(uint8_t *transmit, unsigned int count);
void setLEDColor(unsigned int p, unsigned char r, unsigned char g, unsigned char b);
void clearStrip();
void fillStrip(unsigned char r, unsigned char g, unsigned char b);
void gradualFill(unsigned int n, unsigned char r, unsigned char g, unsigned char b);

// Animations
void animate_start();

// RNG
unsigned int generate_seed(void);
unsigned int rand32(int seed);
void waitForRelease(void);

void leds_from_press();
void refresh_board();

void game_fsm();
void slide_block(uint8_t row, uint8_t num_blocks);
void animate_block_loss(unsigned int *lost_blocks, unsigned int num_lost_blocks);

void shift_left(uint8_t row, uint8_t num_blocks);
void shift_right(uint8_t row, uint8_t num_blocks);

// SPI
//unsigned int spi_led_idx = 0;
unsigned int data_len = 24;               // The number of bytes to be transmitted
unsigned int frame_idx = 0;                // Track transmission count in interrupts
uint8_t *dataptr;                         // Pointer which traverses the tx information


// Capacitive Sensing
/* Pressed Buttons: 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle */
uint8_t button_state = 0x00;

// Random number generation
unsigned int rng_seed;
unsigned int random_num = 0;


// Represent every LED with one byte.
static LED led_board[NUM_LEDS];
unsigned int maxLights[ROWS] = {4, 4, 3, 3, 2, 2, 1, 1};
unsigned char startFilled[64];

// Game parameters
static int current_state = START;
//static int leftmost_block = 0;
static int start_width = 4;
//static int current_width = 4;
static int current_row = 0;
static int dir = 1;


static unsigned int leftmost_block = 0;
static unsigned int current_width = 4;
static unsigned int prev_leftmost = 4;
static unsigned int prev_width = 4;


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
    
    /* Generate a unique random seed based on VLO - DCO differences */
    rng_seed = generate_seed();
    
    // Initialize clocks, timers, and SPI protocol.
    setup();
    
    /* Seed the random number generator with rng_seed */
    random_num = rand32(1);
    
    // Turn off the LED grid.
    clearStrip();
    refresh_board();
    
    // Initialize the game FSM
    dir = 1;
    current_state = START;
    while (1) {
        game_fsm();
        //        slide_block(0, start_width);
        //        refresh_board();
        //        wait(100, &button_state, 0);
        
        // Slide block
        //        if (slide) {
        //
        //        }
        //
        //        //leds_from_press();
        //        if (wait(100, &button_state, 1))
        //            slide = 0;
        //        else
        //            slide = 1;
        //
        //__delay_cycles(1000000);       // lazy delay
        
        
        //        gradualFill(NUM_LEDS, 0x00, 0x08, 0x00);  // green
        //        gradualFill(NUM_LEDS, 0x00, 0x00, 0x08);  // blue
        //        gradualFill(NUM_LEDS, 0x08, 0x00, 0x08);  // magenta
        //        gradualFill(NUM_LEDS, 0x08, 0x08, 0x00);  // yellow
        //        gradualFill(NUM_LEDS, 0x00, 0x08, 0x08);  // cyan
        //        gradualFill(NUM_LEDS, 0x08, 0x00, 0x00);  // red
    }
}


/* Iterates over the game FSM.
 *
 */
void
game_fsm()
{
    static int pressed = 0;
    static int slide = 1;
    
    unsigned int block;
    unsigned int next_width;
    
    unsigned int next_leftmost;
    unsigned int lost_blocks[COLUMNS];
    unsigned int num_lost_blocks = 0;
    unsigned int change_leftmost = 0;
    switch(current_state) {
        case START:
            
            // Play start animation until a press is detected.
            if (!pressed) animate_start();
            
            // Detect button press.
            if (button_state) {
                pressed = 1;
            }
            
            // Detect release and transition.
            if (pressed) {
                if (!button_state) {
                    clearStrip();
                    slide = 1;
                    pressed = 0;
                    current_row = 0;
                    current_width = start_width;
                    current_state = PLAY;
                }
            }
            
            break;
        case PLAY:
            if (slide) {
                slide_block(current_row, current_width);
                refresh_board();
                if (wait(100, &button_state, 0)) return;
            }
            
            if (button_state) {
                current_row++;
                
                
                if (current_row == 1) {
                    prev_width = current_width;
                    prev_leftmost = leftmost_block;
                    waitForRelease();
                    return;
                }
                
                next_width = current_width;
                next_leftmost = leftmost_block;
                
                // Check the alignment of each block with the previous row.
                for (block = leftmost_block; block < leftmost_block + current_width; block++) {
                    // Off to the left or off to the right
                    if (block < prev_leftmost || block > prev_leftmost + prev_width - 1) {
                        
                        // If the leftmost block is off, next_leftmost needs to be determined.
                        if (block == leftmost_block) {
                            change_leftmost = 1;
                        }
                        
                        // Increment to determine the new next_leftmost block.
                        if (change_leftmost) {
                            next_leftmost++;
                        }
                        // Decrement the width of the next row
                        next_width--;
                        
                        lost_blocks[num_lost_blocks] = (current_row - 1) * COLUMNS + block;
                        num_lost_blocks++;
                        //animate_block_loss(current_row - 1, block);
                    }
                }
                
                animate_block_loss(lost_blocks, num_lost_blocks);
                
                // Save the current leftmost and width to check next row's alignment.
                prev_leftmost = next_leftmost;
                prev_width = next_width;
                current_width = maxLights[current_row];
                
                waitForRelease();
            }
            
            
            
            break;
        case WIN:
            break;
        case LOSE:
            break;
    }
}

/* Blocks until all buttons are released. */
void
waitForRelease(void) {
    while (1) {
        wait(1, &button_state, 0);
        if (button_state == 0)
            return;
    }
}


/* Called repeatedly to incrementally slides a row back and forth.
 * Determines the movement direction and updates the global parameter
 * "dir", then moves the led row one led to the left or right.
 */
void
slide_block(uint8_t row, uint8_t num_blocks)
{
    // Shift right
    if (dir) {
        // Detect right edge.
        if (leftmost_block + num_blocks >= COLUMNS) {
            dir = 0; // change direction to left.
            shift_left(row, num_blocks);
        } else {
            shift_right(row, num_blocks);
        }
    } else {
        // Detect left edge.
        if (leftmost_block == 0) {
            dir = 1;
            shift_right(row, num_blocks);
        } else {
            shift_left(row, num_blocks);
        }
    }
    
}


/* Shifts a LED row left one block.
 *
 * row - index from 0 of the row to be shifted
 * num_blocks - number of blocks in the row.
 */
void
shift_left(uint8_t row, uint8_t num_blocks)
{
    // Remove rightmost block
    int old_rightmost = row*COLUMNS + leftmost_block + num_blocks - 1;
    setLEDColor(old_rightmost, 0x00, 0x00, 0x00);
    
    // update left_most block
    leftmost_block--;
    int new_leftmost = row*COLUMNS + leftmost_block;
    setLEDColor(new_leftmost, 0x08, 0x00, 0x00);
}


/* Shifts a LED row right one block.
 *
 * row - index from 0 of the row to be shifted
 * num_blocks - number of blocks in the row.
 */
void
shift_right(uint8_t row, uint8_t num_blocks)
{
    // Remove leftmost block
    int old_leftmost = row*COLUMNS + leftmost_block;
    setLEDColor(old_leftmost, 0x00, 0x00, 0x00);
    
    // Add a new led to the right.
    int new_rightmost = row*COLUMNS + leftmost_block + num_blocks;
    setLEDColor(new_rightmost, 0x08, 0x00, 0x00);
    leftmost_block++;
    
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



/* Sets the color of the LED at index led_idx to the specified color
 * input in RGB format.
 */
void setLEDColor(unsigned int led_idx, unsigned char r, unsigned char g, unsigned char b) {
    led_board[led_idx].green = g;
    led_board[led_idx].red = r;
    led_board[led_idx].blue = b;
}

/* Sets the color of all LEDs on the board to black. */
void clearStrip() {
    fillStrip(0x00, 0x00, 0x00);
}

/* Sets every LED on the board to the same color.
 * Transmits the updated board state.
 */
void fillStrip(unsigned char r, unsigned char g, unsigned char b) {
    int i;
    for (i = 0; i < NUM_LEDS; i++) {
        setLEDColor(i, r, g, b);  // set all LEDs to specified color
    }
    refresh_board();  // refresh strip
}

/* Fill with delay */
void gradualFill(unsigned int n, unsigned char r, unsigned char g, unsigned char b) {
    int i;
    for (i = 0; i < n; i++){        // n is number of LEDs
        setLEDColor(i, r, g, b);
        refresh_board();
        __delay_cycles(1000000);       // lazy delay
    }
}

/* Writes the contents of LED_BOARD to the grid of WS2812 LEDs.
 *
 * These LEDs transmit and interpret information via an NRZ protocol. This
 * requires transmitting each bit as a long (550 - 850) or short
 * (200 - 500) pulse representing a 1 or a 0 respectively.
 *
 * SPI is used for to send a number of 1's set by the macros HIGH_CODE and
 * LOW_CODE for timing purposes.
 *
 * After writing the entire contents of LED_BOARD, this function delays for
 * 50us to ensure future calls overwrite the current LED board state. (50us
 * delay communicates end of new data).
 */
void
refresh_board()
{
    // Disable interrupts to avoid normal SPI driven protocols.
    __bic_SR_register(GIE);
    
    // send RGB color for every LED
    unsigned int i, j;
    for (i = 0; i < NUM_LEDS; i++) {
        unsigned char *rgb = (unsigned char *)&led_board[i]; // get GRB color for this LED
        
        // Transmit the colors in GRB order.
        for (j = 0; j < 3; j++) {
            
            // Mask out the MSB of each byte first.
            unsigned char mask = 0x80;
            
            // Send each of the 8 bits as long and short pulses.
            while (mask != 0) {
                
                // Wait on the previous transmission to complete.
                while (!(IFG2 & UCA0TXIFG))
                    ;
                if (rgb[j] & mask) {
                    UCA0TXBUF = HIGH_CODE;  // Send a long pulse for 1.
                } else {
                    UCA0TXBUF = LOW_CODE;   // Send a short pulse for 0.
                }
                
                mask >>= 1;  // Send the next bit.
            }
        }
    }
    
    // Delay for at least 50us to send RES code and signify end of transmission.
    __delay_cycles(800);
    
    // Re-enable interrupts
    __bis_SR_register(GIE);
}

void
clearStart(void)
{
    int li;
    for (li = 0; li < 64; li ++)
        startFilled[li] = 0;
}


/* Start animation for STACKER
 * Randomly lights up LEDs on the board, then flashes 3 times
 */
void
animate_start()
{
    clearStrip();
    if (wait(500, &button_state, 1)) return;
    int led;
    
    clearStart();
    
    int r1;
    int r2;
    for (led = 0; led < (NUM_LEDS); led++) {
        int numWhile = 0;
        while (1) {
            numWhile ++;
            
            if (numWhile >= 100 && led > 52) {
                numWhile = 0;
                break;
            }
            
            r1 = rand32(0);
            r2 = rand32(0);
            if (startFilled[r1+r2] == 1)
                continue;
            else {
                startFilled[r1+r2] = 1;
                break;
            }
            
        }
        //    	r1 = rand32(0);
        //    	r2 = rand32(0);
        //    	startFilled[r1+r2] = 1;
        
        setLEDColor(r1 + r2, 0x08, 0x00, 0x00);
        
        refresh_board();
        if (wait(100, &button_state, 1)) return;
    }
    fillStrip(0x08, 0x00, 0x00);
    if (wait(500, &button_state, 1)) return;
    clearStrip();
    if (wait(500, &button_state, 1)) return;
    fillStrip(0x08, 0x00, 0x00);
    if (wait(500, &button_state, 1)) return;
    clearStrip();
    if (wait(500, &button_state, 1)) return;
    fillStrip(0x08, 0x00, 0x00);
    if (wait(500, &button_state, 1)) return;
    
    
}


/* Fade animation for misaligned blocks
 *
 */
void
animate_block_loss(unsigned int *lost_blocks, unsigned int num_lost_blocks)
{
    
    int fade_idx;
    int color = 0x08;
    for (fade_idx = 0; fade_idx < 4; fade_idx++) {
        color >>= 1;
        int led;
        for (led = 0; led < num_lost_blocks; led++) {
            setLEDColor(lost_blocks[led], color, 0x00, 0x00);
        }
        refresh_board();
        wait(500, &button_state, 0);
    }
    
}


/*
 * Generates a random number between 0 and 31 based on a LFSR.
 *
 * If seed is 1, the random number generator is initialized with
 * a random starting seed.
 */
unsigned int rand32(int seed)
{
    static unsigned int prev_lfsr = 0x0005;        // Track the previous state and initialize to an optimal seed
    unsigned int lfsr;                         //Initialize lfsr to the previous state
    unsigned int new_bit;
    
    
    // Detect the first call to rand4() and initialize with a random seed.
    if (seed) {
        prev_lfsr = rng_seed;
    }
    
    // Restore the previous state
    lfsr = prev_lfsr;
    
    /* taps: 16 14 13 11; feedback polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
    new_bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    lfsr =  (lfsr >> 1) | (new_bit << 15);
    
    prev_lfsr = lfsr;
    
    // Mask out the value
    return (lfsr & 0b11111);
}


/*
 * Use the difference between the DCO and VLO to generate a
 * random 16 bit number. This function manipulates clocks and
 * timers, so run it first. (http://www.ti.com/lit/an/slaa338/slaa338.pdf)
 * (https://github.com/0/msp430-rng).
 */
unsigned int
generate_seed(void) {
    int i, j;
    unsigned int result = 0;
    
    /* Save default settings */
    unsigned int BCSCTL3_default = BCSCTL3;
    unsigned int TACCTL0_default = TACCTL0;
    unsigned int TACTL_default = TACTL;
    
    /* Stop TimerA */
    TACTL = 0x0;
    
    /* Set up timer */
    BCSCTL3 = (~LFXT1S_3 & BCSCTL3) | LFXT1S_2; // Source ACLK from VLO
    TACCTL0 = CAP | CM_1 | CCIS_1;              // Capture mode, positive edge
    TACTL = TASSEL_2 | MC_2;                    // SMCLK, continuous up
    
    /* Generate bits */
    for (i = 0; i < 16; i++) {
        unsigned int ones = 0;
        
        for (j = 0; j < 5; j++) {
            while (!(CCIFG & TACCTL0));       // Wait for interrupt
            
            TACCTL0 &= ~CCIFG;                // Clear interrupt
            if (1 & TACCR0)                   // If LSb set, count it
                ones++;
        }
        
        result >>= 1;                         // Save previous bits
        
        if (ones >= 3)                        // Best out of 5
            result |= 0x8000;                 // Set MSb
    }
    
    // Rest timers and clocks to their default settings.
    BCSCTL3 = BCSCTL3_default;
    TACCTL0 = TACCTL0_default;
    TACTL = TACTL_default;
    
    return result;
}



/* Timer A0 interrupt service for capacitive touch timing */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) Timer_A0 (void)
#else
#error Compiler not supported!
#endif
{
    /* Exit low power mode 0 locally to send/ process PWM signal without
     * interferring with TimerA1 LMP0 based timing
     */
    __bic_SR_register(LPM0_bits);
    
    /* Track pulse rx time and update button pressed state upon
     * new pulse transmission. Update Leds to represented pressed state.
     */
    check_pulse(&button_state);
    leds_from_press();
    
}


/* Timer A1 interrupt service routine for general timing. */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer_A1 (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER1_A0_VECTOR))) Timer_A1 (void)
#else
#error Compiler not supported!
#endif
{
    __bic_SR_register_on_exit(LPM0_bits);    // Exit low power mode 0.
}
