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


// Transmit codes to send long pulse (1) and short pulse (0).
#define HIGH_CODE   (0xF0)      // b11110000
#define LOW_CODE    (0xC0)      // b11000000

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


#define SELF          YELLOW

// Global states
#define CHOOSE_GAME 0
#define STACKER 1
#define DODGE_GAME 2


// Game states
#define START 0
#define PLAY 1
#define WIN 2
#define LOSE 3


// Board size
#define NUM_LEDS 128
#define COLUMNS 8
#define ROWS 16

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
void fillStrip(uint8_t color);
void expand_color(unsigned int led);
void set_color(unsigned int led, uint8_t color);

// Animations
void animate_start();
void animate_win();
void animate_lose();

// RNG
unsigned int generate_seed(void);
unsigned int rand32(int seed);
void waitForRelease(void);

void leds_from_press();
void refresh_board();

// Stacker
void game_fsm();
void slide_block(uint8_t row, uint8_t num_blocks);
void animate_block_loss(unsigned int *lost_blocks, unsigned int num_lost_blocks);
void shift_left(uint8_t row, uint8_t num_blocks);
void shift_right(uint8_t row, uint8_t num_blocks);

// Dodge game
void move_character(uint8_t direction);
uint8_t buttons_to_direction();
void dodge_game_fsm();

// SPI
//unsigned int spi_led_idx = 0;
unsigned int data_len = 24;             // The number of bytes to be transmitted
unsigned int frame_idx = 0;             // Track transmission count in interrupts
uint8_t *dataptr;                       // Pointer which traverses the tx information

LED expanded_color = {0, 0, 0};

// Capacitive Sensing
/* Pressed Buttons: 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle */
uint8_t button_state = 0x00;

// Random number generation
unsigned int rng_seed;
unsigned int random_num = 0;


// Represent every LED with one byte.
static uint8_t led_board[NUM_LEDS];
unsigned int maxLights[ROWS] = {4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1};
unsigned char startFilled[NUM_LEDS];

// STACKER Game parameters
static int global_state = CHOOSE_GAME;
static int current_state = START;
static int start_width = 4;
static int current_row = 0;
static int dir = 1;
static uint8_t GAME_COLOR = BLUE;

static unsigned int leftmost_block = 0;
static unsigned int current_width = 4;
static unsigned int prev_leftmost = 0;
static unsigned int prev_width = 4;

// Dodge game parameters
static uint8_t position = 67;


static unsigned int fall_time = 500; //0.5s
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
    clearStrip();
    refresh_board();
    
    // Initialize the game FSM
    dir = 1;
    current_state = START;
    int pressed = 0;
    int next_game = 0;
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
                        clearStrip();
                        global_state = next_game;
                        current_state = START;
                    }
                }
                break;
            case STACKER:
                game_fsm();
                break;
            case DODGE_GAME:
                dodge_game_fsm();
                break;
            default:
                break;
        }
    }
}


/* Iterates over the STACKER FSM. */
void
game_fsm()
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
            
            // Initialize STACKER
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
            
            
            slide_block(current_row, current_width);
            refresh_board();
            if (wait(100, &button_state, 0)) return;
            
            
            if (button_state) {
                current_row++;
                
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
            
            // Check if all blocks were lost
            if (!prev_width) {
                current_state = LOSE;
                return;
            }
            
            if (current_row >= ROWS) {
                current_state = WIN;
                return;
            }
            
            break;
        case WIN:
            clearStrip();
            animate_win();
            global_state = CHOOSE_GAME;
            break;
        case LOSE:
            clearStrip();
            animate_lose();
            global_state = CHOOSE_GAME;
            break;
    }
}

/* Play the dodge game! */
void
dodge_game_fsm()
{
    int rng_col;
    switch (current_state) {
        case START:
            position = 67;
            fall_time_count = 0;
            clearStrip();
            set_color(position, SELF);
            refresh_board();
            GAME_COLOR = PURPLE;
            current_state = PLAY;
            break;
        case PLAY:
            
            move_character(buttons_to_direction());
            wait(100, &button_state, 0);
            // Move blocks down half second.
//            if (fall_time_count > fall_time) {
//                // Move blocks down
//                update_falling_blocks();
//                // Set blocks in top row.
//                for (rng_col; rng_col < COLUMNS; rng_col++) {
//                    if (rand32(0) < 8) {
//                        led_board[(ROWS-1) * COLUMNS + rng_col] = 1;
//                    }
//                }
//                
//                // Transmit the new LED board.
//                refresh_board();
//                fall_time_count = 0;
//            }
//            
//            if (button_state) {
//                move_character(buttons_to_direction());
//                wait(100, &button_state, 0);
//            }
            
            break;
        case LOSE:
            wait(2000, &button_state, 0);
            clearStrip();
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
            led_board[led] = OFF;
        }
    }
    
    for (led = COLUMNS; led < NUM_LEDS; led++) {
        // If the led is ON, set the LED below it on, and turn it off.
        if (led_board[led]) {
            
            // COLLISION!!!
            if ((led - COLUMNS) == position) {
                set_color(position, RED);
                current_state = LOSE;
                continue;
            }
            
            set_color(led, OFF);
            set_color(led - COLUMNS, GAME_COLOR);
        }
    }
}

/* Moves the character based on direction
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
                set_color(position, OFF);
                position += 8;
            } else {
                return;
            }
            break;
        case 2:
            // Move right if possible.
            if (position % COLUMNS != 0) {
                // Turn off current position.
                set_color(position, OFF);
                position--;
            } else {
                return;
            }
            break;
        case 3:
            // Move down if possible.
            if (position > 7) {
                // Turn off current position.
                set_color(position, OFF);
                position -= 8;
            } else {
                return
            }
            break;
        case 4:
            // Move left if possible.
            if (position % COLUMNS != COLUMNS - 1) {
                // Turn off current position.
                set_color(position, OFF);
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
        refresh_board();
        current_state = LOSE;
        return;
    }
    
    // Update position.
    set_color(position, SELF);
    refresh_board();
    
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
    set_color(old_rightmost, OFF);
    //setLEDColor(old_rightmost, 0x00, 0x00, 0x00);
    
    // update left_most block
    leftmost_block--;
    int new_leftmost = row*COLUMNS + leftmost_block;
    set_color(new_leftmost, GAME_COLOR);
    //setLEDColor(new_leftmost, 0x08, 0x00, 0x00);
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
    set_color(old_leftmost, OFF);
    //setLEDColor(old_leftmost, 0x00, 0x00, 0x00);
    
    // Add a new led to the right.
    int new_rightmost = row*COLUMNS + leftmost_block + num_blocks;
    set_color(new_rightmost, GAME_COLOR);
    //setLEDColor(new_rightmost, 0x08, 0x00, 0x00);
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


/* Sets the color of the led at index led in led_board
 * encoded into one byte.
 */
void
set_color(unsigned int led, uint8_t color)
{
    led_board[led] = color;
}

/* Expands the encoded color at index led in led_board to 8 bit hexadecimal
 * for SPI transmission.
 */
void
expand_color(unsigned int led)
{
    expanded_color.red = 0x00;
    expanded_color.green = 0x00;
    expanded_color.blue = 0x00;
    
    switch (led_board[led]) {
        case RED:
            expanded_color.red = 0x08;
            break;
        case GREEN:
            expanded_color.green = 0x08;
            break;
        case BLUE:
            expanded_color.blue = 0x08;
            break;
        case RED_FADE_1:
            expanded_color.red = 0x04;
            break;
        case RED_FADE_2:
            expanded_color.red = 0x02;
            break;
        case RED_FADE_3:
            expanded_color.red = 0x01;
            break;
        case BLUE_FADE_1:
            expanded_color.blue = 0x04;
            break;
        case BLUE_FADE_2:
            expanded_color.blue = 0x02;
            break;
        case BLUE_FADE_3:
            expanded_color.blue = 0x01;
            break;
        case GREEN_FADE_1:
            expanded_color.green = 0x04;
            break;
        case GREEN_FADE_2:
            expanded_color.green = 0x02;
            break;
        case GREEN_FADE_3:
            expanded_color.green = 0x01;
        case YELLOW:
            expanded_color.red = 0x08;
            expanded_color.green = 0x05;
        case PURPLE:
            expanded_color.red = 0x08;
            expanded_color.blue = 0x08;
        default:
            break;
    }
    
    
}
/* Sets the color of the LED at index led_idx to the specified color
 * input in RGB format.
 */
void setLEDColor(unsigned int led_idx, unsigned char r, unsigned char g, unsigned char b) {
    //led_board[led_idx].green = g;
    //led_board[led_idx].red = r;
    //led_board[led_idx].blue = b;
}

/* Sets the color of all LEDs on the board to black. */
void clearStrip() {
    fillStrip(OFF);
}

/* Sets every LED on the board to the same color.
 * Transmits the updated board state.
 */
void fillStrip(uint8_t color) {
    int i;
    for (i = 0; i < NUM_LEDS; i++) {
        set_color(i, color);
        //setLEDColor(i, r, g, b);  // set all LEDs to specified color
    }
    refresh_board();  // refresh strip
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
        expand_color(i);
        unsigned char *rgb = (unsigned char *)&expanded_color; // get GRB color for this LED
        
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
    for (li = 0; li < NUM_LEDS; li ++)
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
            
            r1 = rand32(0) + rand32(0);
            r2 = rand32(0) + rand32(0);
            if (startFilled[r1+r2] == 1)
                continue;
            else {
                startFilled[r1+r2] = 1;
                break;
            }
            
        }
        //      r1 = rand32(0);
        //      r2 = rand32(0);
        //      startFilled[r1+r2] = 1;
        
        set_color(r1 + r2, GAME_COLOR);
        //setLEDColor(r1 + r2, 0x08, 0x00, 0x00);
        
        refresh_board();
        if (wait(100, &button_state, 1)) return;
    }
    fillStrip(GAME_COLOR);
    if (wait(500, &button_state, 1)) return;
    clearStrip();
    if (wait(500, &button_state, 1)) return;
    fillStrip(GAME_COLOR);
    if (wait(500, &button_state, 1)) return;
    clearStrip();
    if (wait(500, &button_state, 1)) return;
    fillStrip(GAME_COLOR);
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
        set_color(lost_blocks[led], BLUE_FADE_1);
    }
    refresh_board();
    wait(500, &button_state, 0);
    
    for (led = 0; led < num_lost_blocks; led++) {
        set_color(lost_blocks[led], BLUE_FADE_2);
    }
    refresh_board();
    wait(500, &button_state, 0);
    
    for (led = 0; led < num_lost_blocks; led++) {
        set_color(lost_blocks[led], BLUE_FADE_3);
    }
    refresh_board();
    wait(500, &button_state, 0);
    
    for (led = 0; led < num_lost_blocks; led++) {
        set_color(lost_blocks[led], OFF);
    }
    refresh_board();
    wait(500, &button_state, 0);
    
}

/* Win animation. */
void
animate_win()
{
    clearStrip();
    
    int row;
    int led;
    int offset;
    
    for (row = 0; row < ROWS; row++) {
        offset = row * COLUMNS;
        for (led = 0; led < COLUMNS; led+=2) {
            set_color(offset + led, GREEN);
            //setLEDColor(offset + led, 0x00, 0x08, 0x00);
        }
        refresh_board();
        if (wait(100, &button_state, 0)) return;
    }
    
    for (row = 0; row < ROWS; row++) {
        offset = row * COLUMNS;
        for (led = 0; led < COLUMNS; led+=2) {
            set_color(offset + led, OFF);
            //setLEDColor(offset + led, 0x00, 0x00, 0x00);
        }
        refresh_board();
        if (wait(100, &button_state, 0)) return;
    }
    
    for (row = 0; row < ROWS; row++) {
        offset = row * COLUMNS;
        for (led = 1; led < COLUMNS; led+=2) {
            set_color(offset + led, GREEN);
            //setLEDColor(offset + led, 0x00, 0x08, 0x00);
        }
        refresh_board();
        if (wait(100, &button_state, 0)) return;
    }
    
    for (row = 0; row < ROWS; row++) {
        offset = row * COLUMNS;
        for (led = 1; led < COLUMNS; led+=2) {
            set_color(offset + led, OFF);
            //setLEDColor(offset + led, 0x00, 0x00, 0x00);
        }
        refresh_board();
        if (wait(100, &button_state, 0)) return;
    }
    
    // Green from top and bottom
    clearStrip();
    refresh_board();
    for (row = 0; row < (ROWS >> 1); row++) {
        offset = row * COLUMNS;
        for (led = 0; led < COLUMNS; led++) {
            set_color(offset + led, GREEN);
        }
        offset = (ROWS - row - 1) * COLUMNS;
        for (led = 0; led < COLUMNS; led++) {
            set_color(offset + led, GREEN);
        }
        refresh_board();
        if (wait(300, &button_state, 0)) return;
    }
    
    if (wait(300, &button_state, 0)) return;
    clearStrip();
    if (wait(300, &button_state, 0)) return;
    fillStrip(GREEN);
    if (wait(500, &button_state, 0)) return;
    clearStrip();
    if (wait(300, &button_state, 0)) return;
    refresh_board();
}

/* Lose animation */
void
animate_lose(void)
{
    int li = 0;
    while (1) {
        set_color(li, RED);
        //setLEDColor(li, 0x08, 0x00, 0x00);
        refresh_board();
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
        set_color(li, OFF);
        //setLEDColor(li, 0x00, 0x00, 0x00);
        refresh_board();
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
