# Embedded-Systems-Project
Embedded Systems Project for CS120B @ University of California, Riverside

## Introduction
Escalade is a game that will be played on an LED Matrix. In Escalade, you are a single dot on the LED Matrix that needs to maneuver around walls of enemies that will descend from the top of the LED Matrix to the bottom. There will be a powerup that will allow you to shoot and break through the descending walls

## Hardware

### Pinout
![pinout](https://cloud.githubusercontent.com/assets/22248908/26222003/41292542-3bcd-11e7-8926-0c94135c84a8.png)

### Parts List
The hardware that was used in this design is listed below.
1. ATMega1284p microcontroller
2. 10-LED bar
3. Button
4. Adafruit 2-axis Thumbstick
5. LED Matrix (Anode)
6. Speaker
7. Shift Register x4

## Known Bugs and Short-comings
A minor issue with my project is that the walls are randomly generated based on the number of positions the user moves the character. Therefore, a player can determine the amount of positions he needs to move, left or right, and he can win the game with ease by generating the easiest walls. Even though it is extremely difficult to figure out the positions to move in order to generate specific walls, it can be done. Therefore, I would need to have a variable that increments every millisecond until a start button is pressed. From there, I would seed the time with that incremented value. Though possible, it is nearly improbable for the user to press the start button every time within a millisecond range. However, both are possible, but the second solution is more secure.

## Sources
Thumbstick ADC: <br/>
http://maxembedded.com/2011/06/the-adc-of-the-avr/
