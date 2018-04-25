/**********************************************************************
 * Contains functions to configure the MSP430 for the capacitive-touch
 * based sliding block game.
 *
 * Calling setup() configures the MSP430 clocks and timers in addition
 * to initializing the SPI transimission protocol.
 *
 *********************************************************************/
#ifndef cap_setup_h
#define cap_setup_h

#include <stdio.h>

void setup();
void setup_spi();

#endif /* cap_setup_h */
