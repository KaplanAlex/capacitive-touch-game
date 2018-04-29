#include <msp430.h>
#include <stdint.h>

#include "timing_funcs.h"

/*
 * Uses the timerA0 interrupt to wait for the input number of milliseconds. 
 * 
 * When allow_interupt is > 0, the wait can be interrupted when 
 * a button is pressed (i.e. button_state) > 0.
 * 
 * Returns 1 if interrupted.
 */
unsigned int
wait(int milliseconds, uint8_t *button_state, int allow_interrupt)
{
    // timerA1 interrupts every 1ms.
    int dur_count;
    for (dur_count = 0; dur_count < milliseconds; dur_count++) {
        if (*button_state && allow_interrupt) {
            return 1;
        }
          __bis_SR_register(LPM0_bits); //Enters low power mode for 1ms
    }
    return 0;
}

/*
 * Cannot be interrupted by button presses.
 * Uses the timerA0 interrupt to wait for the input number
 * of milliseconds.
 */
void
blocking_wait(int milliseconds)
{
    int dur_count;
    for (dur_count = 0; dur_count < milliseconds; dur_count++) {
          __bis_SR_register(LPM0_bits); //Enters low power mode for .1ms
    }
}
