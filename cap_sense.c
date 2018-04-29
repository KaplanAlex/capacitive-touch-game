#include <msp430.h>
#include <stdint.h>

#include "cap_sense.h"

#define PRESS_THRESHOLD    4             // Minimum of 2ms delay to register press.
#define ON_TIME            6
#define CYCLE_TIME         100

unsigned int pulse_time = 0;

/* Button state variables: 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle */
uint8_t pulse_rx = 0x00;    // Flags representing propagated pulse rx.

/* rx_time for each capacitive button (in .1ms). */
uint8_t rx_times[5] = {0x00, 0x00, 0x00, 0x00, 0x00};

/* Read from P2IN to detect pin input voltage and store the state of all
 * capacitive buttons in the 5 LSBs of a single byte to recognize received
 * pulses.
 *
 * Update the global variable "pulse_rx" to reflect received pulses.
 * pulse_rx - Ordered from LSB to MSB: Up, Right, Down, Left, Middle.
 */
void
raw_button_state()
{
    uint8_t raw_state = 0;
    uint8_t switch_vals = P2IN;     // Flip P2IN so that switches are high when pressed
    
    /* Up - P3.2  Right - P3.3  Down - P3.4  Left - P3.5  Middle P3.6 */
    raw_state |= switch_vals & BIT2;
    raw_state |= switch_vals & BIT3;
    raw_state |= switch_vals & BIT4;
    raw_state |= switch_vals & BIT5;
    raw_state |= switch_vals & BIT6;
    
    /* Shift raw_state down to LSB. */
    raw_state >>= 2;
    
    /* Update the received pulse flags. */
    pulse_rx |= raw_state;
}


/* Detects and handles capacitive touch PWM pulse reception.
 * Triggered by TA0 interrupt.
 * Upon new pulse transmission ("pulse_time" is reset to 0), the rx_times of the
 * previous pulses are filtered to updated the value pointed to by "button_state" 
 * representing button presses in the order:
 * 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle.
 *
 * Otherwise, detect new pulse reception and update rx_times for every unreceived pulse.
 */
void
check_pulse(uint8_t *btn_state)
{
    // A new pulse has been sent.
    if (!pulse_time) {
        // Convert previous rx_times to button press state.
        uint8_t new_state = 0x00;
        
        /* Update button press state based on previous rx times.
         * 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle
         */
        new_state |= (rx_times[4] > PRESS_THRESHOLD);
        new_state <<= 1;
        
        new_state |= (rx_times[3] > PRESS_THRESHOLD);
        new_state <<= 1;
        
        new_state |= (rx_times[2] > PRESS_THRESHOLD);
        new_state <<= 1;
        
        new_state |= (rx_times[1] > PRESS_THRESHOLD);
        new_state <<= 1;
        
        new_state |= (rx_times[0] > PRESS_THRESHOLD);
        *btn_state = new_state;
        
        // Reset pulse rx flags and times.
        pulse_rx = 0x00;
        int rx_idx;
        for (rx_idx = 4; rx_idx >= 0; rx_idx--) {
            rx_times[rx_idx] = 0x01;
        }
        
        P2OUT |= BIT1; //Send pulse
        pulse_time++;
        return;
    }
    pulse_time++;
    
    // Set output low to turn off the pulse after ON_TIME ms.
    if(pulse_time > ON_TIME)
    {
        P2OUT &= ~BIT1;
    }

    // After CYCLE_TIME, reset the pulse.
    if(pulse_time >= CYCLE_TIME)
    {
        pulse_time = 0;
    }

    // Update pulse_rx to reflect all received pulses.
    raw_button_state();
    
    /* Increment rx_times for unreceived pulses until next pulse is sent.
     * 0 - Up   1 - Right   2 - Down    3 - Left    4 - Middle
     */
    if (!(pulse_rx & BIT0)) rx_times[0]++;
    if (!(pulse_rx & BIT1)) rx_times[1]++;
    if (!(pulse_rx & BIT2)) rx_times[2]++;
    if (!(pulse_rx & BIT3)) rx_times[3]++;
    if (!(pulse_rx & BIT4)) rx_times[4]++;
    
}

