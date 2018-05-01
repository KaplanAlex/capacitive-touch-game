/*************************************************************************
 * Contains functions to detect presses based on capacticance changes.
 *
 * void clear_strip(uint8_t *led_board);
 *      Turns off all the LEDs on the board
 * 
 * void expand_color(unsigned int led);
 *      Expands the internal representation of colors to 3 byte GRB form.
 * 
 * void expand_color(unsigned int led);
 *      Expands the internal representation of colors to 3 byte GRB form.
 * 
 * void fill_strip(uint8_t color, uint8_t *led_board);
 *      Sets all LEDs on the board to color.
 * 
 * void refresh_board(uint8_t *led_board);
 *      Updates the LED grip to the colors in internal representation
 *      "led_board".
 *      
 * void set_color(unsigned int led, uint8_t color, uint8_t *led_board);
 *      Sets the color in "led_board" to color.
 ************************************************************************/

#ifndef led_control_h
#define led_control_h

#include <stdio.h>
void clear_strip(uint8_t *led_board);
void expand_color(unsigned int led, uint8_t *led_board);
void fill_strip(uint8_t color, uint8_t *led_board);
void refresh_board(uint8_t *led_board);
void set_color(unsigned int led, uint8_t color, uint8_t *led_board);
#endif /* led_control_h */
