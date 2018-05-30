# capacitive-touch-game
ELEC 327 Final Project

Alexander Kaplan,
Alfonso Morera,
Neil Seoni

Video Demo

[![Video Demo](https://i.imgur.com/SZCrklM.png)](http://www.youtube.com/watch?v=ffcCMSMpSBw "Video Demo")


Overview:
Our project focused on the design and implementation of capacitive touch buttons as well as management of large quantities of data (relative to the MSP430G3553’s 512 bytes). We designed a PCB with five capacitive touch buttons which interacted with an 8x16 LED grid via SPI to play interactive arcade games. In addition to the hardware, we developed a capacitive touch library to detect presses from our capacitive touch buttons. We designed this library to be easily calibrated for capacitive touch pads with different sensitivities. 

Our design emphasized all of major concepts of microcontroller programming we covered in this course including: low power modes, timers, PWM, and interaction with hardware modules (SPI communication).

Design:
As briefly mentioned in the Overview section, we created a capacitive touch based game through the development of the following distinct components: a PCB with five capacitive touch buttons, an 8x16 WS2812 LED grid, a capacitive touch library, a memory reduced SPI driven WS2812 communication library, and the stacker arcade game logic. The design of each of these discrete aspects of the project is outlined below:

PCB
As depicted in the image below, our PCB interface consisted of five circular 0.5” pads which served as up, down, left, right, and center capacitive touch buttons, as well as a five LED display (bottom left) representing the buttons’ pressed states. 

[Imgur](https://i.imgur.com/aH4VqKW.png)
Figure 1: PCB Board Design

Each pad was connected in parallel with a PWM signal’s path from P2.1 to one of five receive pins (P2.2-P2.6) as shown in the figure below.

[Imgur](https://i.imgur.com/0JYk5dQ.png)
Figure 2: PWM Signal Path


This structure setup each pad to act as half of a capacitor, with our bodies modeled as the other half of the capacitor connected to ground. Thus, touching a pad completed the capacitor and delayed the transmission of the PWM pulse. Our capacitive touch library used the transmission time delay from touching a pad to detect presses (the specifics of the library implementation are discussed in a later section).

To increase the sensitivity and reliability of these capacitive touch pads, our PCB design was centered around the isolation of the pads from all other circuit elements. We minimized capacitive noise by putting only the pads and their corresponding leds on the top of the PCB. Additionally, we covered all open space on the top of the PCB with a ground plate, which neutralized potential capacitive interference from the PCB material.

It is also important to note that to compensate for the large power consumption of our LED grid (discussed in the following section) while ensuring the MSP430 isn’t overpowered, the PCB board routes power from two sources: a 3V coin battery which powers the MSP430 and 5V DC power from a wall outlet which powers the NeoPixel LED grid. We choose to route these sources through the PCB to facilitate the connection of both power supplies to the same ground. By connecting the two grounds, we mitigated any potential issues transmitting SPI signals from the MSP430 to the LED grid.

WS2812 LED Boards

[Imgur](https://i.imgur.com/CnDdV7w.jpg)
Figure 3: LED Grid
We created an 8x16 led grid from two 8x8 NeoPixel blinky boards. These LEDs use an NRZ protocol to determine their color and forward information through the chain. This protocol does not rely on a clock signal, as it requires only a single wire for data transmission. Consequently, bits cannot be sent simply as pulses with high representing 1 and off representing 0, as the LEDs have no timing coordination with transmission. Instead, the W2812 NRZ protocol requires each transmitted bit to be encoded as a long (550 - 850ns) or short (200 - 500ns) pulse representing a 1 or a 0 respectively.

Ultimately, we capitalized on the timing and transmission capabilities of the MSP430 SPI module to communicate via these NRZ specifications. We masked out each bit we transmitted, then sent a 750ns pulse to represent ones and a 375ns pulse to represent zeros. We created these pulse times by calibrating the MSP430 clock to 16MHz and setting the SPI module bit transmission time to 187.5ns. Then, we used the SPI module to transmit 0b11110000 as a long pulse and 0b11000000 as a short pulse, as each bit is written as high or low for 187.5ns. 

The WS2812 leds required 3 bytes corresponding to 8 bit color in GRB format. To compensate for the small program memory of the MSP430, we designed internal state which encoded the color of each LED in a single byte and functions to expand encoded colors to their corresponding 3 byte GRB representation prior to transmission. 

Capacitive Touch Library
To detect capacitive touch, we constructed a library which tracks the transmission time of PWM signals through the pads to five separate receive pins. Touching a capacitive pad increases the pads capacitance and delays the PWM signal transmission. This library utilizes this delay to detect presses by flagging transmission times greater than a specified threshold. We designed this library to be easily calibrated to different pads and sensitivities by abstracting the PWM time parameters and threshold to macros.

Arcade Games
To demonstrate the capabilities of our capacitive touch pad, we created two arcade style games accessible from a global start state. At the start pressing either the right or up pad starts the dodge game. In this game, the player uses the pads to move up, down, left, and right on the board to dodge move between blocks which randomly spawn and fall in rows from the top of the screen. This game continues indefinitely until the player collides with a falling block, at which point the lose animation plays and the game returns to the start state. Pressing any other combination of pads starts stacker. In this game, a set number of blocks slides specific to the row back and forth across the screen. Touching any pad stops the row and starts sliding a block above the previous row. Any blocks which don’t align with the previous row are lost, leaving fewer blocks for the next row to be aligned with. Upon reaching the top of the board, the player wins the game and returns to the start, game select state.  All timing for these games was done in 1ms increments, allowing the MSP430 to enter LMP0 during game delays (player/ block movement speed). Additionally, we designed our program such that the capacitive touch sensing was separated from the game logic and thus was not impacted by the 1ms delays.
