/*************************************************************************
 * Contains functions to detect presses based on capacticance changes.
 *
 * void check_pulse(uint8_t *button_state); 
 *      Triggered by TA0 interrupt every .1ms. Updates the value pointed to
 *      by button_state to represent the currently pressed buttons.
 * 
 * void raw_button_state()
 *      Updates a global variable to refelct the which pulses have been
 *      received. Used to increment rx time
 *
 ************************************************************************/

#ifndef cap_sense_h
#define cap_sense_h

#include <stdio.h>
void check_pulse(uint8_t *button_state);
void raw_button_state();
#endif /* cap_sense_h */
