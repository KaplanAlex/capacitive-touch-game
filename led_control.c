#include <msp430.h>
#include <stdint.h>

#include "led_control.h"

// Transmit codes to send long pulse (1) and short pulse (0).
#define HIGH_CODE   (0xF0)      // b11110000
#define LOW_CODE    (0xC0)      // b11000000

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


// WS2812 LEDs require GRB format
typedef struct {
    unsigned char green;
    unsigned char red;
    unsigned char blue;
} LED;

LED expanded_color = {0, 0, 0};         // Single object colors are expaded into.


/* Sets the color of the led at index led in led_board
 * encoded into one byte.
 */
void
set_color(unsigned int led, uint8_t color, uint8_t *led_board)
{
    led_board[led] = color;
}

/* Expands the encoded color at index led in led_board to 8 bit hexadecimal
 * for SPI transmission.
 */
void
expand_color(unsigned int led, uint8_t *led_board)
{
    expanded_color.red = 0x00;
    expanded_color.green = 0x00;
    expanded_color.blue = 0x00;
    
    switch (led_board[led]) {
        case RED:
            expanded_color.red = (0x08 * BRIGHTNESS);
            break;
        case GREEN:
            expanded_color.green = (0x08 * BRIGHTNESS);
            break;
        case BLUE:
            expanded_color.blue = (0x08 * BRIGHTNESS);
            break;
        case RED_FADE_1:
            expanded_color.red = (0x04 * BRIGHTNESS);
            break;
        case RED_FADE_2:
            expanded_color.red = (0x02 * BRIGHTNESS);
            break;
        case RED_FADE_3:
            expanded_color.red = (0x01 * BRIGHTNESS);
            break;
        case BLUE_FADE_1:
            expanded_color.blue = (0x04 * BRIGHTNESS);
            break;
        case BLUE_FADE_2:
            expanded_color.blue = (0x02 * BRIGHTNESS);
            break;
        case BLUE_FADE_3:
            expanded_color.blue = (0x01 * BRIGHTNESS);
            break;
        case GREEN_FADE_1:
            expanded_color.green = (0x04 * BRIGHTNESS);
            break;
        case GREEN_FADE_2:
            expanded_color.green = (0x02 * BRIGHTNESS);
            break;
        case GREEN_FADE_3:
            expanded_color.green = (0x01 * BRIGHTNESS);
            break;
        case YELLOW:
            expanded_color.red = (0x12 * BRIGHTNESS);
            expanded_color.green = (0x09 * BRIGHTNESS);
            break;
        case PURPLE:
            expanded_color.red = (0x08 * BRIGHTNESS);
            expanded_color.blue = (0x08 * BRIGHTNESS);
            break;
        default:
            break;
    }
}


/* Sets the color of all LEDs on the board to black. */
void clear_strip(uint8_t *led_board) {
    fill_strip(OFF, led_board);
}

/* Sets every LED on the board to the same color.
 * Transmits the updated board state.
 */
void fill_strip(uint8_t color, uint8_t *led_board) {
    int i;
    for (i = 0; i < NUM_LEDS; i++) {
        set_color(i, color, led_board);
    }
    refresh_board(led_board);  // refresh strip
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
refresh_board(uint8_t *led_board)
{
    // Disable interrupts to avoid normal SPI driven protocols.
    __bic_SR_register(GIE);
    
    // send RGB color for every LED
    unsigned int i, j;
    for (i = 0; i < NUM_LEDS; i++) {
        expand_color(i, led_board);
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
