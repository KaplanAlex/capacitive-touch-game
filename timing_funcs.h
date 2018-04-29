#ifndef _TIMING_FUNCS_H_
#define _TIMING_FUNCS_H_
 
// Timing
void blocking_wait(int milliseconds);
unsigned int wait(int milliseconds, uint8_t *debounced_state, int allow_interrupt);

#endif
