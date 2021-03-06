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
#include "led_control.h"
#include "timing_funcs.h"

// Board size
#define NUM_LEDS 128
#define COLUMNS 8
#define ROWS 16


// LED colors
#define OFF           0
#define RED           1
#define GREEN         2
#define BLUE          3
#define RED_FADE_1    4
#define RED_FADE_2    5
#define RED_FADE_3    6
#define BLUE_FADE_1   7
#define BLUE_FADE_2   8
#define BLUE_FADE_3   9
#define GREEN_FADE_1  10
#define GREEN_FADE_2  11
#define GREEN_FADE_3  12
#define YELLOW        13
#define PURPLE        14

#define BRIGHTNESS    3

// Global states
#define CHOOSE_GAME 0
#define STACKER 1
#define DODGE_GAME 2
#define SELF          YELLOW

// Game states
#define START 0
#define PLAY 1
#define WIN 2
#define LOSE 3



// WS2812 LEDs require GRB format
typedef struct {
    unsigned char green;
    unsigned char red;
    unsigned char blue;
} LED;

/* Function Prototypes */
void leds_from_press();

// Animations
void animate_start();
void animate_win();
void animate_lose();

// RNG
unsigned int generate_seed(void);
unsigned int rand32(int seed);
void waitForRelease(void);

// Stacker Functions
void stacker_fsm();
void slide_block(uint8_t row, uint8_t num_blocks);
void animate_block_loss(unsigned int *lost_blocks, unsigned int num_lost_blocks);
void shift_left(uint8_t row, uint8_t num_blocks);
void shift_right(uint8_t row, uint8_t num_blocks);

// Dodge game
void move_character(uint8_t direction);
uint8_t buttons_to_direction();
void dodge_game_fsm();
void update_falling_blocks();

/* Global Parameters */

// Represent every LED with one byte.
static uint8_t led_board[NUM_LEDS];
unsigned int maxLights[ROWS] = {4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1};
unsigned char startFilled[NUM_LEDS];

// Capacitive Sensing
/* Pressed Buttons: 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle */
uint8_t button_state = 0x00;

// Random number generation
unsigned int rng_seed;
unsigned int random_num = 0;

// Global game parameters
static int global_state = CHOOSE_GAME;
static uint8_t GAME_COLOR = BLUE;

// STACKER game parameters
static int current_state = START;
static int start_width = 4;
static int current_row = 0;
static int dir = 1;

static unsigned int leftmost_block = 0;
static unsigned int current_width = 4;
static unsigned int prev_leftmost = 0;
static unsigned int prev_width = 4;

// Dodge game parameters
static uint8_t position = 67;
static unsigned int fall_time = 500;        //0.5s
static unsigned int fall_time_count = 0;

int
main(void)
{
    // Initialize the led_board.
    int i;
    for (i = 0; i < NUM_LEDS; i++) {
        led_board[i] = 0x00;
    }
    
    /* Generate a unique random seed based on VLO - DCO differences */
    rng_seed = generate_seed();
    
    // Initialize clocks, timers, and SPI protocol.
    setup();
    
    /* Seed the random number generator with rng_seed */
    random_num = rand32(1);
    
    // Turn off the LED grid.
    clear_strip(led_board);
    refresh_board(led_board);
    
    
    dir = 1;
    current_state = START;
    int pressed = 0;
    int next_game = 0;
    
    // Initialize the game controlling FSM - choose stacker or dodge game
    while (1) {
        switch (global_state) {
            case CHOOSE_GAME:
                
                // Play start animation until a press is detected.
                if (!pressed) animate_start();
                
                // Detect button press.
                if (button_state) {
                    pressed = 1;
                    
                    // Determine which game was selected.
                    if (button_state <= 2) {
                        // Right or Up press.
                        next_game = DODGE_GAME;
                    } else {
                        next_game = STACKER;
                    }
                }
                
                // Detect release and transition.
                if (pressed) {
                    if (!button_state) {
                        pressed = 0;
                        clear_strip(led_board);
                        global_state = next_game;
                        current_state = START;
                    }
                }
                break;
            case STACKER:
                stacker_fsm();
                break;
            case DODGE_GAME:
                dodge_game_fsm();
                break;
            default:
                break;
        }
    }
}


/* Iterates over the stacker FSM. */
void
stacker_fsm()
{
    static int pressed = 0;
    
    unsigned int block;
    unsigned int next_width;
    unsigned int found_left;
    unsigned int next_leftmost;
    unsigned int lost_blocks[COLUMNS];
    unsigned int num_lost_blocks = 0;
    unsigned int change_leftmost = 0;
    
    switch(current_state) {
        case START:
            
            // Initialize Stacker parameters
            pressed = 0;
            current_row = 0;
            current_width = start_width;
            prev_width = start_width;
            prev_leftmost = 0;
            found_left = 0;
            GAME_COLOR = BLUE;
            current_state = PLAY;
            break;
        case PLAY:
            // Move block back and forth.
            slide_block(current_row, current_width);
            refresh_board(led_board);
            if (wait(100, &button_state, 0)) return;
            
            
            if (button_state) {
                current_row++;
                
                // The first row can be stopped anywhere.
                if (current_row == 1) {
                    prev_width = current_width;
                    prev_leftmost = leftmost_block;
                    waitForRelease();
                    return;
                }
                
                found_left = 0;
                change_leftmost = 0;
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
                        if (change_leftmost && !found_left) {
                            next_leftmost++;
                        }
                        // Decrement the width of the next row
                        next_width--;
                        
                        lost_blocks[num_lost_blocks] = (current_row - 1) * COLUMNS + block;
                        num_lost_blocks++;
                        //animate_block_loss(current_row - 1, block);
                    } else {
                        found_left = 1;
                    }
                }
                
                animate_block_loss(lost_blocks, num_lost_blocks);
                
                // Save the current leftmost and width to check next row's alignment.
                prev_leftmost = next_leftmost;
                prev_width = next_width;
                current_width = maxLights[current_row];
                
                waitForRelease();
            }
            
            // Check if all blocks were lost and transition.
            if (!prev_width) {
                current_state = LOSE;
                return;
            }
            
            // Detect win and transition.
            if (current_row >= ROWS) {
                current_state = WIN;
                return;
            }
            
            break;
        case WIN:
            clear_strip(led_board);
            animate_win();
            
            // Return to the outer fsm.
            global_state = CHOOSE_GAME;
            break;
        case LOSE:
            clear_strip(led_board);
            animate_lose();
            
            // Return to the outer fsm.
            global_state = CHOOSE_GAME;
            break;
    }
}

/* Play the dodge game! */
void
dodge_game_fsm()
{
    int rng_col;
    int rng_count;
    switch (current_state) {
        case START:
            // Start the player in the middle of the board.
            position = 67;
            fall_time_count = 0;
            clear_strip(led_board);
            set_color(position, SELF, led_board);
            refresh_board(led_board);
            GAME_COLOR = PURPLE;
            current_state = PLAY;
            break;
        case PLAY:
            
            // Move blocks down half second.
            if (fall_time_count > fall_time) {
                
                // Move blocks down
                update_falling_blocks();
                
                // Randomly generate at most 4 blocks in the top row.
                rng_count = 0;
                for (rng_col; rng_col < COLUMNS; rng_col++) {
                    if (rng_count > 4) break;
                    
                    // Light up the the led in rng_col.
                    if (rand32(0) < 3) {
                        led_board[((ROWS-1) * COLUMNS) + rng_col] = RED;
                        rng_count++;
                    }
                }
            
                // Transmit the new LED board.
                refresh_board(led_board);
                fall_time_count = 0;
            }
            
            // Check for button presses and move character.
            move_character(buttons_to_direction());
            
            // Delay .1s to control movement.
            wait(100, &button_state, 0);
            
            break;
        case LOSE:
            // Freeze board on lose, then transition.
            wait(2000, &button_state, 0);
            clear_strip(led_board);
            animate_lose();
            global_state = CHOOSE_GAME;
            break;
        default:
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

/* Parses the pressed button state to determine movement direction. */
uint8_t
buttons_to_direction()
{
    if (!button_state) return (0);
    
    // Parse button presses with priority.
    if ((button_state & BIT0) != 0) return (1);
    else if ((button_state & BIT1) != 0) return (2);
    else if ((button_state & BIT2) != 0) return (3);
    else if ((button_state & BIT3) != 0) return (4);
    
    
    return 0;
}

/* Moves all falling blocks down one position and detects blocks moving out of
 * the board.
 */
void
update_falling_blocks()
{
    int led;
    
    // Detect all leds falling out of the board (in first row).
    for (led = 0; led < COLUMNS; led++) {
        if (led_board[led]) {
            if (led == position) continue;
            led_board[led] = OFF;
        }
    }
    
    for (led = COLUMNS; led < NUM_LEDS; led++) {
        // If the led is ON, set the LED below it on, and turn it off.
        if (led_board[led]) {
            if (led == position) continue;
            // COLLISION!!!
            if ((led - COLUMNS) == position) {
                set_color(position, RED, led_board);
                current_state = LOSE;
                continue;
            }
            
            set_color(led, OFF, led_board);
            set_color(led - COLUMNS, GAME_COLOR, led_board);
        }
    }
}

/* Moves the character based on direction and detect colisions.
 * 1 - up, 2 - right, 3 - down, 4 - left
 */
void
move_character(uint8_t direction)
{
    switch (direction) {
        case 1:
            // Move up if possible.
            if (position < 120) {
                // Turn off current position.
                set_color(position, OFF, led_board);
                position += 8;
            } else {
                return;
            }
            break;
        case 2:
            // Move right if possible.
            if (position % COLUMNS != 0) {
                // Turn off current position.
                set_color(position, OFF, led_board);
                position--;
            } else {
                return;
            }
            break;
        case 3:
            // Move down if possible.
            if (position > 7) {
                // Turn off current position.
                set_color(position, OFF, led_board);
                position -= 8;
            } else {
                return;
            }
            break;
        case 4:
            // Move left if possible.
            if (position % COLUMNS != COLUMNS - 1) {
                // Turn off current position.
                set_color(position, OFF, led_board);
                position++;
            } else {
                return;
            }
            break;
        default:
            return;
            break;
            
    }
    
    // COLLISION!!
    if (led_board[position]) {
        led_board[position] = RED;
        refresh_board(led_board);
        current_state = LOSE;
        return;
    }
    
    // Update position.
    set_color(position, SELF, led_board);
    refresh_board(led_board);
    
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
    set_color(old_rightmost, OFF, led_board);
    
    // update left_most block
    leftmost_block--;
    int new_leftmost = row*COLUMNS + leftmost_block;
    set_color(new_leftmost, GAME_COLOR, led_board);
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
    set_color(old_leftmost, OFF, led_board);
    
    // Add a new led to the right.
    int new_rightmost = row*COLUMNS + leftmost_block + num_blocks;
    set_color(new_rightmost, GAME_COLOR, led_board);
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

/* Clear the color representation array for start animation. */
void
clearStart(void)
{
    int li;
    for (li = 0; li < NUM_LEDS; li ++)
        startFilled[li] = 0;
}


/* Start animation for STACKER
 * Randomly lights up LEDs on the board, then flashes 3 times
 */
void
animate_start()
{
    clear_strip(led_board);
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
            
            r1 = rand32(0) + rand32(0);
            r2 = rand32(0) + rand32(0);
            if (startFilled[r1+r2] == 1)
                continue;
            else {
                startFilled[r1+r2] = 1;
                break;
            }
            
        }

        set_color(r1 + r2, GAME_COLOR, led_board);
        
        refresh_board(led_board);
        if (wait(100, &button_state, 1)) return;
    }
    fill_strip(GAME_COLOR, led_board);
    if (wait(500, &button_state, 1)) return;
    clear_strip(led_board);
    if (wait(500, &button_state, 1)) return;
    fill_strip(GAME_COLOR, led_board);
    if (wait(500, &button_state, 1)) return;
    clear_strip(led_board);
    if (wait(500, &button_state, 1)) return;
    fill_strip(GAME_COLOR, led_board);
    if (wait(500, &button_state, 1)) return;
    
}


/* Fade animation for misaligned blocks. */
void
animate_block_loss(unsigned int *lost_blocks, unsigned int num_lost_blocks)
{
    
    int fade_idx;
    int led;
    int color = 0x08;
    
    // If no blocks were lost, just return.
    if (!num_lost_blocks) return;
    
    // Fade animation.
    for (led = 0; led < num_lost_blocks; led++) {
        set_color(lost_blocks[led], BLUE_FADE_1, led_board);
    }
    refresh_board(led_board);
    wait(500, &button_state, 0);
    
    for (led = 0; led < num_lost_blocks; led++) {
        set_color(lost_blocks[led], BLUE_FADE_2, led_board);
    }
    refresh_board(led_board);
    wait(500, &button_state, 0);
    
    for (led = 0; led < num_lost_blocks; led++) {
        set_color(lost_blocks[led], BLUE_FADE_3, led_board);
    }
    refresh_board(led_board);
    wait(500, &button_state, 0);
    
    for (led = 0; led < num_lost_blocks; led++) {
        set_color(lost_blocks[led], OFF, led_board);
    }
    refresh_board(led_board);
    wait(500, &button_state, 0);
    
}

/* Win animation. */
void
animate_win()
{
    clear_strip(led_board);
    
    int row;
    int led;
    int offset;
    
    for (row = 0; row < ROWS; row++) {
        offset = row * COLUMNS;
        for (led = 0; led < COLUMNS; led+=2) {
            set_color(offset + led, GREEN, led_board);
        }
        refresh_board(led_board);
        if (wait(100, &button_state, 0)) return;
    }
    
    for (row = 0; row < ROWS; row++) {
        offset = row * COLUMNS;
        for (led = 0; led < COLUMNS; led+=2) {
            set_color(offset + led, OFF, led_board);
        }
        refresh_board(led_board);
        if (wait(100, &button_state, 0)) return;
    }
    
    for (row = 0; row < ROWS; row++) {
        offset = row * COLUMNS;
        for (led = 1; led < COLUMNS; led+=2) {
            set_color(offset + led, GREEN, led_board);
        }
        refresh_board(led_board);
        if (wait(100, &button_state, 0)) return;
    }
    
    for (row = 0; row < ROWS; row++) {
        offset = row * COLUMNS;
        for (led = 1; led < COLUMNS; led+=2) {
            set_color(offset + led, OFF, led_board);
        }
        refresh_board(led_board);
        if (wait(100, &button_state, 0)) return;
    }
    
    // Green from top and bottom
    clear_strip(led_board);
    refresh_board(led_board);
    for (row = 0; row < (ROWS >> 1); row++) {
        offset = row * COLUMNS;
        for (led = 0; led < COLUMNS; led++) {
            set_color(offset + led, GREEN, led_board);
        }
        offset = (ROWS - row - 1) * COLUMNS;
        for (led = 0; led < COLUMNS; led++) {
            set_color(offset + led, GREEN, led_board);
        }
        refresh_board(led_board);
        if (wait(300, &button_state, 0)) return;
    }
    
    if (wait(300, &button_state, 0)) return;
    clear_strip(led_board);
    if (wait(300, &button_state, 0)) return;
    fill_strip(GREEN, led_board);
    if (wait(500, &button_state, 0)) return;
    clear_strip(led_board);
    if (wait(300, &button_state, 0)) return;
    refresh_board(led_board);
}

/* Lose animation */
void
animate_lose(void)
{
    int li = 0;
    while (1) {
        set_color(li, RED, led_board);
        refresh_board(led_board);
        if ((li / ROWS) % 2 == 0) {
            if ((li % COLUMNS) == COLUMNS - 1) {
                li += COLUMNS;
                wait(100, &button_state, 0);
                continue;
            }
            li++;
        } else {
            if ((li % COLUMNS) == 0) {
                li += COLUMNS;
                wait(100, &button_state, 0);
                continue;
            }
            li --;
        }
        if (li >= NUM_LEDS - 1) {
            break;
        }
        wait(100, &button_state, 0);
    }
    li = 0;
    while (1) {
        set_color(li, OFF, led_board);
        refresh_board(led_board);
        if ((li / ROWS) % 2 == 0) {
            if ((li % COLUMNS) == COLUMNS - 1) {
                li += COLUMNS;
                wait(100, &button_state, 0);
                continue;
            }
            li ++;
        } else {
            if ((li % COLUMNS) == 0) {
                li += COLUMNS;
                wait(100, &button_state, 0);
                continue;
            }
            li --;
        }
        if (li >= NUM_LEDS - 1) {
            break;
        }
        wait(100, &button_state, 0);
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


/* Timer A1 interrupt service routine for general timing. Interrrupts every ms. */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer_A1 (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER1_A0_VECTOR))) Timer_A1 (void)
#else
#error Compiler not supported!
#endif
{
    fall_time_count++;
    __bic_SR_register_on_exit(LPM0_bits);    // Exit low power mode 0.
}

