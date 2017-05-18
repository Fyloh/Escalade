#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "scheduler.h"
#include "timer.h"

unsigned char GND = 0x01; 
unsigned char B; 
unsigned char G; 
unsigned char R;

int led_arr[8][8]; 
int row = 0;
int seeder = 0;
unsigned char score = 0;
int height, width = 0;
unsigned char game_over = 0x00;
double frqs[58];
unsigned char powerup_activated = 0x00;
unsigned char B2;

/* set_PWM code for Music */
void set_PWM(double frequency) {
	
	
	// Keeps track of the currently set frequency
	// Will only update the registers when the frequency
	// changes, plays music uninterrupted.
	static double current_frequency;
	if (frequency != current_frequency) {

		if (!frequency) TCCR3B &= 0x08; //stops timer/counter
		else TCCR3B |= 0x03; // resumes/continues timer/counter
		
		// prevents OCR3A from overflowing, using prescaler 64
		// 0.954 is smallest frequency that will not result in overflow
		if (frequency < 0.954) OCR3A = 0xFFFF;
		
		// prevents OCR3A from underflowing, using prescaler 64					// 31250 is largest frequency that will not result in underflow
		else if (frequency > 31250) OCR3A = 0x0000;
		
		// set OCR3A based on desired frequency
		else OCR3A = (short)(8000000 / (128 * frequency)) - 1;

		TCNT3 = 0; // resets counter
		current_frequency = frequency;
	}
}

/* PWM_on() code for Music */
void PWM_on() {
	TCCR3A = (1 << COM3A0);
	// COM3A0: Toggle PB6 on compare match between counter and OCR3A
	TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);
	// WGM32: When counter (TCNT3) matches OCR3A, reset counter
	// CS31 & CS30: Set a prescaler of 64
	set_PWM(0);
}

void PWM_off() {
	TCCR3A = 0x00;
	TCCR3B = 0x00;
}

/* Shift Register Code */
void shift() {
	if(row == 7) {
		GND = 0x01;
		row = 0;
	}
	
	else {
		GND = (GND << 1);
		row++;
	}
	
	B = 0x00;
	R = 0x00;
	G = 0x00;
	
	// select a bit for a color and right shift if less than 7
	for(int col = 0; col < 8; col++) {
		if(led_arr[row][col] == 1) {
			G |= 0x80;
		}
		
		if(led_arr[row][col] == 2){
			B |= 0x80;
		}
		
		if(led_arr[row][col] == 3){
			R |= 0x80;
		}
		
		if(led_arr[row][col] == 4){
			G |= 0x80;  R |= 0x80;  B |= 0x80;
		} // WHITE
		
		if(col < 7) {
			G = G >> 1;
			B = B >> 1;
			R = R >> 1;
		}
	}
	
	/* Invert due to Common Anode LED Matrix */
	G = ~G;
	B = ~B;
	R = ~R;
	
	for(int i = 7; i >= 0; --i) {
		// Sets SRCLR to 1 allowing data to be set
		// Also clears SRCLK in preparation of sending data
		PORTD = 0x88;
		PORTC = 0x88;
		// set SER = next bit of data to be sent.
		PORTD |= ((R >> i) & 0x01);
		PORTD |= (((B >> i) << 4) & 0x10);
		PORTC |= (((G >> i) << 4) & 0x10);
		PORTC |= ((GND >> i) & 0x01);
		
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		PORTD |= 0x44;
		PORTC |= 0x44;
	}
	
	// set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
	PORTD |= 0x22;
	PORTC |= 0x22;
	
	// clears all lines in preparation of a new transmission
	PORTD = 0x00;
	PORTC = 0x00;
}

void InitADC() {
	ADMUX=(1<<REFS0);
	ADCSRA=(1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
	// ADEN: Set to turn on ADC , by default it is turned off
	//ADPS2: ADPS2 and ADPS0 set to make division factor 32

}

void convert_to_digital() {
	ADCSRA |=(1<<ADSC);
	while ( !(ADCSRA & (1<<ADIF))); // Wait for conversion
}

/* GLOBAL VARIABLES FOR GETMOVEMENT SM */
/* 0x01 = Right */
/* 0x02 = Left */
unsigned char movement_bit_val;
int x_val;

/* GETMOVEMENT SM */

/* Will detect the movement of the thumb stick using ADC to
digital conversion. Checks threshold values to assign global
variable, movement_bit_val, to upwards, downwards, left or right */
enum getMovement_States {init, wait, x_axis};
int getMovement(int state) {
	
	ADMUX = 0x40;
	switch(state) {
		case init:
			state = wait;
			break;
			
		case wait:
			state = x_axis;
			break;
			
		case x_axis:
			state = wait;
			break;
			
		default:
			break;
	}
	
	switch(state) {
		case init:
			break;
		
		case wait:
			movement_bit_val = 0x00;
			break;
		
		case x_axis:
			convert_to_digital();
			x_val = ADC;	
			
			if(x_val > 900) {
				movement_bit_val = 0x01; /* Right */
			}
			
			else if(x_val < 100) {
				movement_bit_val = 0x02; /* Left */
			}
			
			break;
		
			
		default:
			break;
	}
	
	return state;
} 

enum moveObject_States {mO_init, mO_wait, mO_right, mO_left};
int moveObject(int state) {
	switch(state) {
		case mO_init:
			state = mO_wait;
			break;
			
		case mO_wait:
			if(movement_bit_val == 0x01) {
				state = mO_right;
			}
			
			else if(movement_bit_val == 0x02) {
				state = mO_left;
			}
			
			else {
				state = mO_wait;
			}
			
			break;
			
		case mO_right:
			state = mO_wait;
			break;
		
		case mO_left:
			state = mO_wait;
			break;
			
		default: 
			break;
	}
	
	switch(state) {
		case mO_init:
			break;
		
		case mO_wait:
			break;
			
		case mO_right:
			/* Decrease width in the array */
			led_arr[height][width] = 0;
			
			if(width == 0) {
				width = 7;
			}
			
			else {
				--width;
			}
			
			if(led_arr[height][width] == 2) {
				game_over = 0x01;
			}
			
			else if(led_arr[height][width] == 1) {
				powerup_activated = 0x01;
				led_arr[height][width] = 3;
			}
			
			else {
				led_arr[height][width] = 3;
			}
			
			/* Creates new seed for randomness for walls */
			++seeder;
			
			break;
			
		case mO_left:
			/* Increase width in the array */
			/* Check boundary conditions */
			led_arr[height][width] = 0;
			
			if(width == 7) {
				width = 0;
			}
			
			else {
				++width;
			}
			
			if(led_arr[height][width] == 2) {
				game_over = 0x01;
			}
			
			else if(led_arr[height][width] == 1) {
				powerup_activated = 0x01;
				led_arr[height][width] = 3;
			}
			
			else {
				led_arr[height][width] = 3;
			}
			
			/* Creates new seed for randomness for walls */
			++seeder;
			
			break;
			
		default:
			break;
	}
	
	return state;
}

enum moveWalls_States {mW_init, mW_wait, mW_generate, mW_move};
int randomNum, powerup_randomNum, powerup_spawn;
unsigned char counter = 7;
unsigned pos_0, pos_1, pos_2, pos_3, pos_4, pos_5, pos_6, pos_7 = 0;
int moveWalls(int state) {
	switch(state) {
		case mW_init:
			state = mW_wait;
			break;
		
		case mW_wait:
			state = mW_generate;
			break;
			
		case mW_generate:
			state = mW_move;
			break;
		
		case mW_move:
			if(counter == 0) {
				score = score + 1;
				state = mW_generate;
				/* Fixes the issue of having a powerup spawn immedietely after previous powerup
				is finished */
				powerup_randomNum = 0;
			}
			
			else {
				state = mW_move;
			}
			
			break;
			
		default:
			break;
			
	}
	
	switch(state) {
		case mW_init:
			break;
		
		case mW_wait:
			break;
		
		/* Generates Random Walls */	
		case mW_generate:
			/* Reset move counter */
			counter = 7;
			
			/* New seeder & random number generated */
			++seeder;
			srand(seeder);
			randomNum = rand() % 10 + 1;
			
			/* Disables LED walls that were left over from previous
			wall iterations */
			/* 	O O O O O O O O
				O O O O O O O O
				. . . . . . . .
				X X X X X O O O 
			*/	
			for(int e = 0; e < 8; ++e) {
				if(led_arr[0][e] == 2 || led_arr[0][e] == 1) {
					led_arr[0][e] = 0;
				}	
			}
			
			for(int q = 0; q < 8; ++q) {
				led_arr[7][q] = 0;	
			}
			
			pos_0 = pos_1 = pos_2 = pos_3 = pos_4 = pos_5 = pos_6 = pos_7 = 0;
			
			if(randomNum == 1) {
				led_arr[7][0] = 2;
				led_arr[7][1] = 2;
				led_arr[7][2] = 2;
				led_arr[7][3] = 2;
				led_arr[7][4] = 2;
				
			}
			
			if(randomNum == 2) {
				led_arr[7][7] = 2;
				led_arr[7][6] = 2;
				led_arr[7][5] = 2;
				led_arr[7][4] = 2;
				led_arr[7][3] = 2;
			}
			
			if(randomNum == 3) {
				led_arr[7][0] = 2;
				led_arr[7][1] = 2;
				led_arr[7][2] = 2;
				led_arr[7][5] = 2;
				led_arr[7][6] = 2;
				led_arr[7][7] = 2;
				
			}
			
			if(randomNum == 4) {
				led_arr[7][7] = 2;
				led_arr[7][6] = 2;
				led_arr[7][5] = 2;
				led_arr[7][4] = 2;
				led_arr[7][3] = 2;
				led_arr[7][2] = 2;
			}
			
			if(randomNum == 5) {
				led_arr[7][0] = 2;
				led_arr[7][1] = 2;
				led_arr[7][2] = 2;
				led_arr[7][3] = 2;
				led_arr[7][4] = 2;
				led_arr[7][5] = 2;
			}
			
			if(randomNum == 6) {
				led_arr[7][0] = 2;
				led_arr[7][1] = 2;
				led_arr[7][3] = 2;
				led_arr[7][4] = 2;
				led_arr[7][6] = 2;
				led_arr[7][7] = 2;
			}
			
			if(randomNum == 7) {
				led_arr[7][1] = 2;
				led_arr[7][2] = 2;
				led_arr[7][3] = 2;
				led_arr[7][4] = 2;
				led_arr[7][5] = 2;
				led_arr[7][6] = 2;
			}
			
			if(randomNum == 8) {
				led_arr[7][0] = 2;
				led_arr[7][1] = 2;
				led_arr[7][2] = 2;
				led_arr[7][4] = 2;
				led_arr[7][5] = 2;
				led_arr[7][6] = 2;
			}
			
			if(randomNum == 9) {
				led_arr[7][1] = 2;
				led_arr[7][2] = 2;
				led_arr[7][3] = 2;
				led_arr[7][5] = 2;
				led_arr[7][6] = 2;
				led_arr[7][7] = 2;
			}
			
			if(randomNum == 10) {
				led_arr[7][0] = 2;
				led_arr[7][2] = 2;
				led_arr[7][4] = 2;
				led_arr[7][6] = 2;
			}
			
			/* Makes sure there is not a powerup already activated */
			if(powerup_activated == 0x00) {
				/* Generate powerup with a 20% 
				chance everytime a wall is generated */
				powerup_randomNum = rand() % 10 + 1;
				
				/* 1 is arbitrary */
				if(powerup_randomNum == 1 || powerup_randomNum == 5) {
					while(1) {
						/* Tries to determine where the open spot is for
						the power up */
						
						/* New seeder & random number generated */
						++seeder;
						srand(seeder);
					
						/* Generates random number n, 0 <= n <= 7 */
						powerup_spawn = rand() % 8;
						/* If there is an opening in the wall, display
						the powerup in the opening */
						if(led_arr[7][powerup_spawn] == 0) {
							led_arr[7][powerup_spawn] = 1;
							break;
						}
					}
				}
			}
			
			shift();
			
			break;
			
		case mW_move:
			if(randomNum == 1) {
				
				if(led_arr[counter][0] == 0) {
					pos_0 = 1;
				}
				
				if(led_arr[counter][1] == 0) {
					pos_1 = 1;
				}
				
				if(led_arr[counter][2] == 0) {
					pos_2 = 1;
				}
				
				if(led_arr[counter][3] == 0) {
					pos_3 = 1;
				}
				
				if(led_arr[counter][4] == 0) {
					pos_4 = 1;
				}
				
				
				led_arr[counter][0] = 0;
				led_arr[counter][1] = 0;
				led_arr[counter][2] = 0;
				led_arr[counter][3] = 0;
				led_arr[counter][4] = 0;
				
				/* Powerup */
				if(powerup_randomNum == 1 || powerup_randomNum == 5) {
					led_arr[counter][powerup_spawn] = 0;
				}
				
				counter = counter - 1;
				
				if((pos_0 == 0 && led_arr[counter][0] == 3) ||
					(pos_1 == 0 && led_arr[counter][1] == 3) ||
					(pos_2 == 0 && led_arr[counter][2] == 3) || 
					(pos_3 == 0 && led_arr[counter][3] == 3) ||
					(pos_4 == 0 && led_arr[counter][4] == 3)) {
						game_over = 0x01;
					}
					
				else {
					if(pos_0 == 1) {
						led_arr[counter][0] = 0;
					}
					
					else {
						led_arr[counter][0] = 2;
					}
					
					if(pos_1 == 1) {
						led_arr[counter][1] = 0;
					}
					
					else {
						led_arr[counter][1] = 2;
					}
					
					if(pos_2 == 1) {
						led_arr[counter][2] = 0;
					}
					
					else {
						led_arr[counter][2] = 2;
					}
					
					if(pos_3 == 1) {
						led_arr[counter][3] = 0;
					}
					
					else {
						led_arr[counter][3] = 2;
					}
					
					if(pos_4 == 1) {
						led_arr[counter][4] = 0;
					}
					
					else {
						led_arr[counter][4] = 2;
					}
					
					led_arr[height][width] = 3;
					
					/* Powerup */
					if(powerup_activated == 0x00) {
						if(powerup_randomNum == 1 || powerup_randomNum == 5) {
						
							/* If the powerup interacts with the player,
							activate global variable powerup_activated */
							if(led_arr[counter][powerup_spawn] == 3) {
								powerup_activated = 0x01;
							}
						
							/* Else, move the powerup down the grid */
							else {
								led_arr[counter][powerup_spawn] = 1;
							}
						}
					}
				}
				
			}
			
			if(randomNum == 2) {
				
				if(led_arr[counter][7] == 0) {
					pos_7 = 1;
				}
				
				if(led_arr[counter][6] == 0) {
					pos_6 = 1;
				}
				
				if(led_arr[counter][5] == 0) {
					pos_5 = 1;
				}
				
				if(led_arr[counter][4] == 0) {
					pos_4 = 1;
				}
				
				if(led_arr[counter][3] == 0) {
					pos_3 = 1;
				}
				
				led_arr[counter][7] = 0;
				led_arr[counter][6] = 0;
				led_arr[counter][5] = 0;
				led_arr[counter][4] = 0;
				led_arr[counter][3] = 0;
				
				if(powerup_randomNum == 1 || powerup_randomNum == 5) {
					led_arr[counter][powerup_spawn] = 0;
				}
				
				counter = counter - 1;
				
				if((pos_7 == 0 && led_arr[counter][7] == 3)||
					(pos_6 == 0 && led_arr[counter][6] == 3) || 
					(pos_5 == 0 && led_arr[counter][5] == 3) ||
					(pos_4 == 0 && led_arr[counter][4] == 3) ||
					(pos_3 == 0 && led_arr[counter][3] == 3)) {
						game_over = 0x01;
					}
					
				else {
					
					if(pos_7 == 1) {
						led_arr[counter][7] = 0;
					}
					
					else {
						led_arr[counter][7] = 2;
					}
					
					if(pos_6 == 1) {
						led_arr[counter][6] = 0;
					}
					
					else {
						led_arr[counter][6] = 2;
					}
					
					if(pos_5 == 1) {
						led_arr[counter][5] = 0;
					}
					
					else {
						led_arr[counter][5] = 2;
					}
					
					if(pos_4 == 1) {
						led_arr[counter][4] = 0;
					}
					
					else {
						led_arr[counter][4] = 2;
					}
					
					if(pos_3 == 1) {
						led_arr[counter][3] = 0;
					}
					
					else {
						led_arr[counter][3] = 2;
					}
					
					led_arr[height][width] = 3;
					
					/* Powerup */
					if(powerup_activated == 0x00) {
						if(powerup_randomNum == 1 || powerup_randomNum == 5) {
						
							/* If the powerup interacts with the player,
							activate global variable powerup_activated */
							if(led_arr[counter][powerup_spawn] == 3) {
								powerup_activated = 0x01;
							}
						
							/* Else, move the powerup down the grid */
							else {
								led_arr[counter][powerup_spawn] = 1;
							}
						}
					}
				}
				
			}
			
			if(randomNum == 3) {
				if(led_arr[counter][0] == 0) {
					pos_0 = 1;
				}
				
				if(led_arr[counter][1] == 0) {
					pos_1 = 1;
				}
				
				if(led_arr[counter][2] == 0) {
					pos_2 = 1;
				}
				
				if(led_arr[counter][5] == 0) {
					pos_5 = 1;
				}
				
				if(led_arr[counter][6] == 0) {
					pos_6 = 1;
				}
				
				if(led_arr[counter][7] == 0) {
					pos_7 = 1;
				}
				
				led_arr[counter][0] = 0;
				led_arr[counter][1] = 0;
				led_arr[counter][2] = 0;
				led_arr[counter][5] = 0;
				led_arr[counter][6] = 0;
				led_arr[counter][7] = 0;
				
				if(powerup_randomNum == 1 || powerup_randomNum == 5) {
					led_arr[counter][powerup_spawn] = 0;
				}
				
				counter = counter - 1;
				
				if((pos_0 == 0 && led_arr[counter][0] == 3) ||
					(pos_1 == 0 && led_arr[counter][1] == 3) ||
					(pos_2 == 0 && led_arr[counter][2] == 3) ||
					(pos_5 == 0 && led_arr[counter][5] == 3) ||
					(pos_6 == 0 && led_arr[counter][6] == 3) ||
					(pos_7 == 0 && led_arr[counter][7] == 3)) {
						game_over = 0x01;
					}
				
				else {
					
					if(pos_0 == 1) {
						led_arr[counter][0] = 0;
					}
					
					else {
						led_arr[counter][0] = 2;
					}
					
					if(pos_1 == 1) {
						led_arr[counter][1] = 0;
					}
					
					else {
						led_arr[counter][1] = 2;
					}
					
					if(pos_2 == 1) {
						led_arr[counter][2] = 0;
					}
					
					else {
						led_arr[counter][2] = 2;
					}
					
					if(pos_5 == 1) {
						led_arr[counter][5] = 0;
					}
					
					else {
						led_arr[counter][5] = 2;
					}
					
					if(pos_6 == 1) {
						led_arr[counter][6] = 0;
					}
					
					else {
						led_arr[counter][6] = 2;
					}
					
					if(pos_7 == 1) {
						led_arr[counter][7] = 0;
					}
					
					else {
						led_arr[counter][7] = 2;
					}
					
					led_arr[height][width] = 3;
					
					/* Powerup */
					if(powerup_activated == 0x00) {
						if(powerup_randomNum == 1 || powerup_randomNum == 5) {
						
							/* If the powerup interacts with the player,
							activate global variable powerup_activated */
							if(led_arr[counter][powerup_spawn] == 3) {
								powerup_activated = 0x01;
							}
						
							/* Else, move the powerup down the grid */
							else {
								led_arr[counter][powerup_spawn] = 1;
							}
						}
					}
				}
			}
			
			if(randomNum == 4) {
				
				if(led_arr[counter][7] == 0) {
					pos_7 = 1;
				}
				
				if(led_arr[counter][6] == 0) {
					pos_6 = 1;
				}
				
				if(led_arr[counter][5] == 0) {
					pos_5 = 1;
				}
				
				if(led_arr[counter][4] == 0) {
					pos_4 = 1;
				}
				
				if(led_arr[counter][3] == 0) {
					pos_3 = 1;
				}
				
				if(led_arr[counter][2] == 0) {
					pos_2 = 1;
				}
				
				led_arr[counter][7] = 0;
				led_arr[counter][6] = 0;
				led_arr[counter][5] = 0;
				led_arr[counter][4] = 0;
				led_arr[counter][3] = 0;
				led_arr[counter][2] = 0;
				
				if(powerup_randomNum == 1 || powerup_randomNum == 5) {
					led_arr[counter][powerup_spawn] = 0;
				}
				
				counter = counter - 1;
				
				if((pos_7 == 0 && led_arr[counter][7] == 3) ||
					(pos_6 == 0 && led_arr[counter][6] == 3) ||
					(pos_5 == 0 && led_arr[counter][5] == 3) ||
					(pos_4 == 0 && led_arr[counter][4] == 3) ||
					(pos_3 == 0 && led_arr[counter][3] == 3) ||
					(pos_2 == 0 && led_arr[counter][2] == 3)) {
						game_over = 0x01;
					}
					
				else {
					
					if(pos_7 == 1) {
						led_arr[counter][7] = 0;
					}
					
					else {
						led_arr[counter][7] = 2;
					}
					
					if(pos_6 == 1) {
						led_arr[counter][6] = 0;
					}
					
					else {
						led_arr[counter][6] = 2;
					}
					
					if(pos_5 == 1) {
						led_arr[counter][5] = 0;
					}
					
					else {
						led_arr[counter][5] = 2;
					}
					
					if(pos_4 == 1) {
						led_arr[counter][4] = 0;
					}
					
					else {
						led_arr[counter][4] = 2;
					}
					
					if(pos_3 == 1) {
						led_arr[counter][3] = 0;
					}
					
					else {
						led_arr[counter][3] = 2;
					}
					
					if(pos_2 == 1) {
						led_arr[counter][2] = 0;
					}
					
					else {
						led_arr[counter][2] = 2;
					}
					
					led_arr[height][width] = 3;
					
					/* Powerup */
					if(powerup_activated == 0x00) {
						if(powerup_randomNum == 1 || powerup_randomNum == 5) {
						
							/* If the powerup interacts with the player,
							activate global variable powerup_activated */
							if(led_arr[counter][powerup_spawn] == 3) {
								powerup_activated = 0x01;
							}
						
							/* Else, move the powerup down the grid */
							else {
								led_arr[counter][powerup_spawn] = 1;
							}
						}
					}
				}
			}
			
			if(randomNum == 5) {
				
				if(led_arr[counter][0] == 0) {
					pos_0 = 1;
				}
				
				if(led_arr[counter][1] == 0) {
					pos_1 = 1;
				}
				
				if(led_arr[counter][2] == 0) {
					pos_2 = 1;
				}
				
				if(led_arr[counter][3] == 0) {
					pos_3 = 1;
				}
				
				if(led_arr[counter][4] == 0) {
					pos_4 = 1;
				}
				
				if(led_arr[counter][5] == 0) {
					pos_5 = 1;
				}
				
				led_arr[counter][0] = 0;
				led_arr[counter][1] = 0;
				led_arr[counter][2] = 0;
				led_arr[counter][3] = 0;
				led_arr[counter][4] = 0;
				led_arr[counter][5] = 0;
				
				if(powerup_randomNum == 1 || powerup_randomNum == 5) {
					led_arr[counter][powerup_spawn] = 0;
				}
				
				counter = counter - 1;
				
				if((pos_0 == 0 && led_arr[counter][0] == 3) ||
					(pos_1 == 0 && led_arr[counter][1] == 3) ||
					(pos_2 == 0 && led_arr[counter][2] == 3) ||
					(pos_3 == 0 && led_arr[counter][3] == 3) ||
					(pos_4 == 0 && led_arr[counter][4] == 3) ||
					(pos_5 == 0 && led_arr[counter][5] == 3)) {
						game_over = 0x01;
					}	
					
				else {
					
					if(pos_0 == 1) {
						led_arr[counter][0] = 0;
					}
					
					else {
						led_arr[counter][0] = 2;
					}
					
					if(pos_1 == 1) {
						led_arr[counter][1] = 0;
					}
					
					else {
						led_arr[counter][1] = 2;
					}
					
					if(pos_2 == 1) {
						led_arr[counter][2] = 0;
					}
					
					else {
						led_arr[counter][2] = 2;
					}
					
					if(pos_3 == 1) {
						led_arr[counter][3] = 0;
					}
					
					else {
						led_arr[counter][3] = 2;
					}
					
					if(pos_4 == 1) {
						led_arr[counter][4] = 0;
					}
					
					else {
						led_arr[counter][4] = 2;
					}
					
					if(pos_5 == 1) {
						led_arr[counter][5] = 0;
					}
					
					else {
						led_arr[counter][5] = 2;
					}
					
					led_arr[height][width] = 3;
				
					/* Powerup */
					if(powerup_activated == 0x00) {
						if(powerup_randomNum == 1 || powerup_randomNum == 5) {
						
							/* If the powerup interacts with the player,
							activate global variable powerup_activated */
							if(led_arr[counter][powerup_spawn] == 3) {
								powerup_activated = 0x01;
							}
						
							/* Else, move the powerup down the grid */
							else {
								led_arr[counter][powerup_spawn] = 1;
							}
						}
					}
				}
			}
			
			if(randomNum == 6) {
				
				if(led_arr[counter][0] == 0) {
					pos_0 = 1;
				}
				
				if(led_arr[counter][1] == 0) {
					pos_1 = 1;
				}
				
				if(led_arr[counter][3] == 0) {
					pos_3 = 1;
				}
				
				if(led_arr[counter][4] == 0) {
					pos_4 = 1;
				}
				
				if(led_arr[counter][6] == 0) {
					pos_6 = 1;
				}
				
				if(led_arr[counter][7] == 0) {
					pos_7 = 1;
				}
				
				led_arr[counter][0] = 0;
				led_arr[counter][1] = 0;
				led_arr[counter][3] = 0;
				led_arr[counter][4] = 0;
				led_arr[counter][6] = 0;
				led_arr[counter][7] = 0;
				
				if(powerup_randomNum == 1 || powerup_randomNum == 5) {
					led_arr[counter][powerup_spawn] = 0;
				}
				
				counter = counter - 1;
				
				if((pos_0 == 0 && led_arr[counter][0] == 3) ||
					(pos_1 == 0 && led_arr[counter][1] == 3) ||
					(pos_3 == 0 && led_arr[counter][3] == 3) ||
					(pos_4 == 0 && led_arr[counter][4] == 3) ||
					(pos_6 == 0 && led_arr[counter][6] == 3) ||
					(pos_7 == 0 && led_arr[counter][7] == 3)) {
						game_over = 0x01;
					}
					
				else {
					
					if(pos_0 == 1) {
						led_arr[counter][0] = 0;
					}
					
					else {
						led_arr[counter][0] = 2;
					}
					
					if(pos_1 == 1) {
						led_arr[counter][1] = 0;
					}
					
					else {
						led_arr[counter][1] = 2;
					}
					
					if(pos_3 == 1) {
						led_arr[counter][3] = 0;
					}
					
					else {
						led_arr[counter][3] = 2;
					}
					
					if(pos_4 == 1) {
						led_arr[counter][4] = 0;
					}
					
					else {
						led_arr[counter][4] = 2;
					}
					
					if(pos_6 == 1) {
						led_arr[counter][6] = 0;
					}
					
					else {
						led_arr[counter][6] = 2;
					}
					
					if(pos_7 == 1) {
						led_arr[counter][7] = 0;
					}
					
					else {
						led_arr[counter][7] = 2;
					}
					
					led_arr[height][width] = 3;
					
					/* Powerup */
					if(powerup_activated == 0x00) {
						if(powerup_randomNum == 1 || powerup_randomNum == 5) {
						
							/* If the powerup interacts with the player,
							activate global variable powerup_activated */
							if(led_arr[counter][powerup_spawn] == 3) {
								powerup_activated = 0x01;
							}
						
							/* Else, move the powerup down the grid */
							else {
								led_arr[counter][powerup_spawn] = 1;
							}
						}
					}
				}
			}
			
			if(randomNum == 7) {
				if(led_arr[counter][1] == 0) {
					pos_1 = 1;
				}
				
				if(led_arr[counter][2] == 0) {
					pos_2 = 1;
				}
				
				if(led_arr[counter][3] == 0) {
					pos_3 = 1;
				}
				
				if(led_arr[counter][4] == 0) {
					pos_4 = 1;
				}
				
				if(led_arr[counter][5] == 0) {
					pos_5 = 1;
				}
				
				if(led_arr[counter][6] == 0) {
					pos_6 = 1;
				}
				
				led_arr[counter][1] = 0;
				led_arr[counter][2] = 0;
				led_arr[counter][3] = 0;
				led_arr[counter][4] = 0;
				led_arr[counter][5] = 0;
				led_arr[counter][6] = 0;
				
				if(powerup_randomNum == 1 || powerup_randomNum == 5) {
					led_arr[counter][powerup_spawn] = 0;
				}
				
				counter = counter - 1;
				
				if((pos_1 == 0 && led_arr[counter][1] == 3) ||
					(pos_2 == 0 && led_arr[counter][2] == 3) ||
					(pos_3 == 0 && led_arr[counter][3] == 3) ||
					(pos_4 == 0 && led_arr[counter][4] == 3) ||
					(pos_5 == 0 && led_arr[counter][5] == 3) ||
					(pos_6 == 0 && led_arr[counter][6] == 3)) {
						game_over = 0x01;
					}
					
				else {	
					
					if(pos_1 == 1) {
						led_arr[counter][1] = 0;
					}
					
					else {
						led_arr[counter][1] = 2;
					}
					
					if(pos_2 == 1) {
						led_arr[counter][2] = 0;
					}
					
					else {
						led_arr[counter][2] = 2;
					}
					
					if(pos_3 == 1) {
						led_arr[counter][3] = 0;
					}
					
					else {
						led_arr[counter][3] = 2;
					}
					
					if(pos_4 == 1) {
						led_arr[counter][4] = 0;
					}
					
					else {
						led_arr[counter][4] = 2;
					}
					
					if(pos_5 == 1) {
						led_arr[counter][5] = 0;
					}
					
					else {
						led_arr[counter][5] = 2;
					}
					
					if(pos_6 == 1) {
						led_arr[counter][6] = 0;
					}
					
					else {
						led_arr[counter][6] = 2;
					}
					
					led_arr[height][width] = 3;
					
					/* Powerup */
					if(powerup_activated == 0x00) {
						if(powerup_randomNum == 1 || powerup_randomNum == 5) {
						
							/* If the powerup interacts with the player,
							activate global variable powerup_activated */
							if(led_arr[counter][powerup_spawn] == 3) {
								powerup_activated = 0x01;
							}
						
							/* Else, move the powerup down the grid */
							else {
								led_arr[counter][powerup_spawn] = 1;
							}
						}
					}
				}
			}
			
			if(randomNum == 8) {
				if(led_arr[counter][0] == 0) {
					pos_0 = 1;
				}
				
				if(led_arr[counter][1] == 0) {
					pos_1 = 1;
				}
				
				if(led_arr[counter][2] == 0) {
					pos_2 = 1;
				}
				
				if(led_arr[counter][4] == 0) {
					pos_4 = 1;
				}
				
				if(led_arr[counter][5] == 0) {
					pos_5 = 1;
				}
				
				if(led_arr[counter][6] == 0) {
					pos_6 = 1;
				}
				
				led_arr[counter][0] = 0;
				led_arr[counter][1] = 0;
				led_arr[counter][2] = 0;
				led_arr[counter][4] = 0;
				led_arr[counter][5] = 0;
				led_arr[counter][6] = 0;
				
				if(powerup_randomNum == 1 || powerup_randomNum == 5) {
					led_arr[counter][powerup_spawn] = 0;
				}
				
				counter = counter - 1;
				
				if((pos_0 == 0 && led_arr[counter][0] == 3) ||
					(pos_1 == 0 && led_arr[counter][1] == 3) ||
					(pos_2 == 0 && led_arr[counter][2] == 3) ||
					(pos_4 == 0 && led_arr[counter][4] == 3) ||
					(pos_5 == 0 && led_arr[counter][5] == 3) ||
					(pos_6 == 0 && led_arr[counter][6] == 3)) {
						game_over = 0x01;
					}
				
				else {
					
					if(pos_0 == 1) {
						led_arr[counter][0] = 0;
					}
					
					else {
						led_arr[counter][0] = 2;
					}
					
					if(pos_1 == 1) {
						led_arr[counter][1] = 0;
					}
					
					else {
						led_arr[counter][1] = 2;
					}
					
					if(pos_2 == 1) {
						led_arr[counter][2] = 0;
					}
					
					else {
						led_arr[counter][2] = 2;
					}
					
					if(pos_4 == 1) {
						led_arr[counter][4] = 0;
					}
					
					else {
						led_arr[counter][4] = 2;
					}
					
					if(pos_5 == 1) {
						led_arr[counter][5] = 0;
					}
					
					else {
						led_arr[counter][5] = 2;
					}
					
					if(pos_6 == 1) {
						led_arr[counter][6] = 0;
					}
					
					else {
						led_arr[counter][6] = 2;
					}
					
					led_arr[height][width] = 3;
					
					/* Powerup */
					if(powerup_activated == 0x00) {
						if(powerup_randomNum == 1 || powerup_randomNum == 5) {
						
							/* If the powerup interacts with the player,
							activate global variable powerup_activated */
							if(led_arr[counter][powerup_spawn] == 3) {
								powerup_activated = 0x01;
							}
						
							/* Else, move the powerup down the grid */
							else {
								led_arr[counter][powerup_spawn] = 1;
							}
						}
					}
				}
			}
			
			if(randomNum == 9) {
				
				if(led_arr[counter][1] == 0) {
					pos_1 = 1;
				}
				
				if(led_arr[counter][2] == 0) {
					pos_2 = 1;
				}
				
				if(led_arr[counter][3] == 0) {
					pos_3 = 1;
				}
				
				if(led_arr[counter][5] == 0) {
					pos_5 = 1;
				}
				
				if(led_arr[counter][6] == 0) {
					pos_6 = 1;
				}
				
				if(led_arr[counter][7] == 0) {
					pos_7 = 1;
				}
				
				led_arr[counter][1] = 0;
				led_arr[counter][2] = 0;
				led_arr[counter][3] = 0;
				led_arr[counter][5] = 0;
				led_arr[counter][6] = 0;
				led_arr[counter][7] = 0;
				
				if(powerup_randomNum == 1 || powerup_randomNum == 5) {
					led_arr[counter][powerup_spawn] = 0;
				}
				
				counter = counter - 1;
				
				if((pos_1 == 0 && led_arr[counter][1] == 3) ||
					(pos_2 == 0 && led_arr[counter][2] == 3) ||
					(pos_3 == 0 && led_arr[counter][3] == 3) ||
					(pos_5 == 0 && led_arr[counter][5] == 3) ||
					(pos_6 == 0 && led_arr[counter][6] == 3) ||
					(pos_7 == 0 && led_arr[counter][7] == 3)) {
						game_over = 0x01;
					}
					
				else {
					
					if(pos_1 == 1) {
						led_arr[counter][1] = 0;
					}
					
					else {
						led_arr[counter][1] = 2;
					}
					
					if(pos_2 == 1) {
						led_arr[counter][2] = 0;
					}
					
					else {
						led_arr[counter][2] = 2;
					}
					
					if(pos_3 == 1) {
						led_arr[counter][3] = 0;
					}
					
					else {
						led_arr[counter][3] = 2;
					}
					
					if(pos_5 == 1) {
						led_arr[counter][5] = 0;
					}
					
					else {
						led_arr[counter][5] = 2;
					}
					
					if(pos_6 == 1) {
						led_arr[counter][6] = 0;
					}
					
					else {
						led_arr[counter][6] = 2;
					}
					
					if(pos_7 == 1) {
						led_arr[counter][7] = 0;
					}
					
					else {
						led_arr[counter][7] = 2;
					}
					
					led_arr[height][width] = 3;
					
					/* Powerup */
					if(powerup_activated == 0x00) {
						if(powerup_randomNum == 1 || powerup_randomNum == 5) {
						
							/* If the powerup interacts with the player,
							activate global variable powerup_activated */
							if(led_arr[counter][powerup_spawn] == 3) {
								powerup_activated = 0x01;
							}
						
							/* Else, move the powerup down the grid */
							else {
								led_arr[counter][powerup_spawn] = 1;
							}
						}
					}
				}
			}
			
			if(randomNum == 10) {
				if(led_arr[counter][0] == 0) {
					pos_0 = 1;
				}
				
				if(led_arr[counter][2] == 0) {
					pos_2 = 1;
				}
				
				if(led_arr[counter][4] == 0) {
					pos_4 = 1;
				}
				
				if(led_arr[counter][6] == 0) {
					pos_6 = 1;
				}
				
				if(powerup_randomNum == 1 || powerup_randomNum == 5) {
					led_arr[counter][powerup_spawn] = 0;
				}
				
				led_arr[counter][0] = 0;
				led_arr[counter][2] = 0;
				led_arr[counter][4] = 0;
				led_arr[counter][6] = 0;
				
				counter = counter - 1;
				
				if((pos_0 == 0 && led_arr[counter][0] == 3) ||
					(pos_2 == 0 && led_arr[counter][2] == 3) ||
					(pos_4 == 0 && led_arr[counter][4] == 3) ||
					(pos_6 == 0 && led_arr[counter][6] == 3)) {
						game_over = 0x01;
					}
				
				else {
					
					if(pos_0 == 1) {
						led_arr[counter][0] = 0;
					}
					
					else {
						led_arr[counter][0] = 2;
					}
					
					if(pos_2 == 1) {
						led_arr[counter][2] = 0;
					}
					
					else {
						led_arr[counter][2] = 2;
					}
					
					if(pos_4 == 1) {
						led_arr[counter][4] = 0;
					}
					
					else {
						led_arr[counter][4] = 2;
					}
					
					if(pos_6 == 1) {
						led_arr[counter][6] = 0;
					}
					
					else {
						led_arr[counter][6] = 2;
					}
					
					led_arr[height][width] = 3;
					
					/* Powerup */
					if(powerup_activated == 0x00) {
						if(powerup_randomNum == 1 || powerup_randomNum == 5) {
						
							/* If the powerup interacts with the player,
							activate global variable powerup_activated */
							if(led_arr[counter][powerup_spawn] == 3) {
								powerup_activated = 0x01;
							}
						
							/* Else, move the powerup down the grid */
							else {
								led_arr[counter][powerup_spawn] = 1;
							}
						}
					}
				}
			}
			
			shift();
			
			break;
	}
	
	return state;
}

enum powerupShooting_States {pS_init, pS_wait, pS_generate, pS_shoot};
unsigned char powerup_remainingTime = 0x00;
unsigned char powerup_heightCounter = 0x01;
unsigned char temp_width;
int powerupShooting(int state) {
	switch(state) {
		case pS_init:
			state = pS_wait;
			break;
		
		case pS_wait:
			if(powerup_activated == 0x01) {
				state = pS_generate;
				/*powerup_remainingTime = 0*/
				powerup_remainingTime = 96;
			}
			
			else {
				state = pS_wait;
			}
			
			break;
			
		case pS_generate:
			/* if(powerup_remainingTime < 100 */
			if(powerup_remainingTime > 0) {
				state = pS_shoot;
			}
			
			/* if(powerup_remainingTime >= 100) */
			if(powerup_remainingTime == 0) {
				state = pS_wait;
				powerup_activated = 0x00;
			}
			
			break;
		
		case pS_shoot:
			/* if(powerup_heightCounter == 7 && powerup_remainingTime >= 100) */
			if(powerup_heightCounter == 7 || powerup_remainingTime == 0) {
				state = pS_generate;
			}
			
			else {
				state = pS_shoot;
			}
			
			break;
			
		default:
			break;
	}
	
	switch(state) {
		case pS_init:
			break;
			
		case pS_wait:
			break;
			
		case pS_generate:
			temp_width = width;
			for(int i = 0; i < 8; ++i) {
				if(led_arr[7][i] == 4) {
					led_arr[7][i] = 0;
				}
			}
			
			if(powerup_remainingTime > 0) {	
				powerup_heightCounter = 1;
				if(led_arr[powerup_heightCounter][temp_width] == 2) {
					led_arr[powerup_heightCounter][temp_width] = 0;

				}
				
				else {
					led_arr[powerup_heightCounter][temp_width] = 4;
				}
			}
			shift();
			
			break;
			
		case pS_shoot:
			led_arr[powerup_heightCounter][temp_width] = 0;
			powerup_heightCounter = powerup_heightCounter + 1;
			if(led_arr[powerup_heightCounter][temp_width] == 2) {
				led_arr[powerup_heightCounter][temp_width] = 0;
			}
			else {
				led_arr[powerup_heightCounter][temp_width] = 4;
			}
			/* powerup_remainingTime = powerup_remainingTime + 1; */
			powerup_remainingTime = powerup_remainingTime - 1;
			shift();
			break;
			
		default:
			break;
	}
	
	
	return state;
}

enum playMusic_States {pM_wait, pM_play};
unsigned char i = 0;
int playMusic(int state) {
	
	switch(state) {
		case pM_wait:
			state = pM_play;
		break;
		
		case pM_play:
			state = pM_play;
			if(i <= 16) {
				++i;
			}
			
			if(i > 16) {
				i = 0;
			}
			
			break;
		
		default:
			break;
	}
	
	switch(state) {
		case pM_wait:
			set_PWM(0);
			i = 0;
			break;
			
		case pM_play:
			set_PWM(frqs[i]);
			break;
			
		default:
			break;
	}
	
	return state;
}

void set_frequencies() {
	frqs[0] = 164.81;
	frqs[1] = 164.81;
	frqs[2] = 164.81;
	frqs[3] = 130.81;
	frqs[4] = 164.81;
	frqs[5] = 195.99;
	frqs[6] = 195.99;
	
	frqs[7] = 164.81;
	frqs[8] = 195.99;
	frqs[9] = 164.81;
	frqs[10] = 220;
	frqs[11] = 246.94;
	frqs[12] = 233.08;
	frqs[13] = 220;
	frqs[14] = 195.99;
	
	frqs[15] = 164.81; // E
	frqs[16] = 195.99; // G
	frqs[17] = 220.00; // A
	frqs[18] = 174.61; // F
	frqs[19] = 195.99; // G
	frqs[20] = 164.81; // E
	frqs[21] = 261.63; // C
	frqs[22] = 146.83; // D
	frqs[23] = 246.94; // B
	
	frqs[22]= 261.63; // C
	frqs[23] = 195.99; // G
	frqs[24] = 164.81; // E
	frqs[25] = 220.00; // A
	frqs[26] = 246.94; // B
	frqs[27]= 116.54; // A#
	frqs[28] = 220.00; // A
	frqs[29] = 195.99; // G
	
	frqs[30] = 164.81; // E
	frqs[31] = 195.99; // G
	frqs[32] = 220.00; // A
	frqs[33] = 174.61; // F
	frqs[34] = 195.99; // G
	frqs[35] = 164.81; // E
	frqs[36] = 261.63; // C
	frqs[37] = 146.83; // D
	frqs[38] = 246.94; // B
	
	frqs[39] = 195.99; // G
	frqs[40] = 184.99; // F#
	frqs[41] = 174.61; // F
	frqs[42] = 311.13; // D#
	frqs[43] = 164.81; // E
	
	frqs[44] = 220.00; // A
	frqs[45] = 220.00; // A
	frqs[46] = 261.63; // C
	frqs[47] = 220.00; // A
	frqs[48] = 261.63; // C
	frqs[49] = 146.83; // D
	
	frqs[50] = 195.99; // G
	frqs[51] = 184.99; // F#
	frqs[52] = 174.61; // F
	frqs[53] = 311.13; // D#
	frqs[54] = 164.81; // E
	frqs[55] = 261.63; // C
	frqs[56] = 261.63; // C
	frqs[57] = 261.63; // C
}

int main(void)
{
	/* (DDR) F = output; 0 = input */
	
	/* A0 - A2: Input */
	/* A3 - A7: Output */
	DDRA = 0xFC; PORTA = 0x03;
	/* Ports B - D are output ports */
	DDRB = 0xFD; PORTB = 0x02;
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xFF; PORTD = 0x00;
	
	unsigned long int getMovement_period = 45;
	unsigned long int moveObject_period = 45;
	unsigned long int moveWalls_period = 200;
	unsigned long int powerupShooting_period = 75;
	unsigned long int playMusic_period = 250;
	
	//calculate GCD
	unsigned long int GCD = 1;
	
	//declare tasks and task array
	static task task1, task2, task3, task4, task5;
	task *tasks[] = {&task1, &task2, &task3, &task4, &task5};
	const unsigned short numTasks = 5;
	 
	task1.state = init; //initial state of task 1
	task1.period = getMovement_period;//task 1 period
	task1.elapsedTime = getMovement_period;//set to period so it ticks when it turns on
	task1.TickFct = &getMovement;
	
	task2.state = mO_init;
	task2.period = moveObject_period;
	task2.elapsedTime = moveObject_period;
	task2.TickFct = &moveObject;
	
	task3.state = mW_init;
	task3.period = moveWalls_period;
	task3.elapsedTime = moveWalls_period;
	task3.TickFct = &moveWalls;
	
	task4.state = pS_init;
	task4.period = powerupShooting_period;
	task4.elapsedTime = powerupShooting_period;
	task4.TickFct = &powerupShooting;
	
	task5.state = pM_wait;
	task5.period = playMusic_period;
	task5.elapsedTime = playMusic_period;
	task5.TickFct = &playMusic;
		
	/* Initialize Timer */
	TimerSet(1);
	TimerOn();
	
	/* Initialize ADC */
	InitADC();
	
	/* Intialize Random Seed */
	srand(seeder);
	
	/* Set music */
	set_frequencies();
	PWM_on();
	
	/* Initialize height and width for starting position*/
	height = 0;
	width = 3;
	
	/* Fill led_arr with 0's */
	for(int i = 0; i < 8; i++) {
		for(int j = 0; j < 8; j++) {
			led_arr[i][j] = 0;
		}
	}
	
	/* Set starting position based on initial height and width */
	led_arr[height][width] = 3;

	while (1) {
		shift();
		
		B2 = ~PINB & 0x02;
		if(B2 == 2) {
			
			/* Fill led_arr with 0's */
			for(int i = 0; i < 8; i++) {
				for(int j = 0; j < 8; j++) {
					led_arr[i][j] = 0;
				}
			}
			
			game_over = 0x00;
			score = 0;
			
			height = 0;
			width = 3;
			
			led_arr[height][width] = 3;
			
			task1.state = init;
			task1.period = getMovement_period;
			task1.elapsedTime = getMovement_period;
			
			task2.state = mO_init;
			task2.period = moveObject_period;
			task2.elapsedTime = moveObject_period;
			
			task3.state = mW_init;
			task3.period = moveWalls_period;
			task3.elapsedTime = moveWalls_period;
			
			task4.state = pS_init;
			task4.period = powerupShooting_period;
			task4.elapsedTime = powerupShooting_period;
			
			task5.state = pM_wait;
			task5.period = playMusic_period;
			task5.elapsedTime = playMusic_period;
			
			GND = 0x01;
			B2 = 0x01;
			PWM_on();
			i = 0;
			row = 0;
			seeder = 0;
			powerup_activated = 0x00;
			powerup_remainingTime = 0x00;
			powerup_heightCounter = 0x01;
			counter = 7;
			pos_0 = pos_1 = pos_2 = pos_3 = pos_4 = pos_5 = pos_6 = pos_7 = 0;
			shift();
		}
		
		if(game_over == 0x00 && score < 60 && B2 != 2) {
			for(int i = 0; i < numTasks; ++i) {
				//check if task is ready to tick
				if(tasks[i]->elapsedTime == tasks[i]->period) {
					//call the tick fct & set the next state
					tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
					//reset elapsed time to 0
					tasks[i]->elapsedTime = 0;
					if(game_over == 0x01) {
						break;
					}
				
					/* Score */
					PORTA = (score << 2);
				
					if(score == 20) {
						task3.period = 150;
					}
				
					else if(score == 40) {
						task3.period = 100;
					}
				
					/* Seeds new random time */
					srand(seeder);
				}
				//increment elapsed time for the task by the master clock period
				tasks[i]->elapsedTime += GCD;
			}
		}
			
		
		else if(score >= 60) {
			/* Fill led_arr with 0's */
			for(int i = 0; i < 8; i++) {
				for(int j = 0; j < 8; j++) {
					led_arr[i][j] = 0;
				}
			}
			
			while(1) {
				B2 = ~PINB & 0x02;
				led_arr[6][0] = 2;
				led_arr[6][1] = 2;
				led_arr[6][2] = 2;
				led_arr[6][5] = 2;
				led_arr[6][6] = 2;
				led_arr[6][7] = 2;
				led_arr[5][0] = 2;
				led_arr[5][2] = 2;
				led_arr[5][5] = 2;
				led_arr[5][7] = 2;
				led_arr[4][0] = 2;
				led_arr[4][1] = 2;
				led_arr[4][2] = 2;
				led_arr[4][5] = 2;
				led_arr[4][6] = 2;
				led_arr[4][7] = 2;
				
				led_arr[2][7] = 2;
				led_arr[1][6] = 2;
				led_arr[0][5] = 2;
				led_arr[0][4] = 2;
				led_arr[0][3] = 2;
				led_arr[0][2] = 2;
				led_arr[1][1] = 2;
				led_arr[2][0] = 2;
				shift();
				PWM_off();
				
				if(B2 == 2) {
					
					/* Fill led_arr with 0's */
					for(int i = 0; i < 8; i++) {
						for(int j = 0; j < 8; j++) {
							led_arr[i][j] = 0;
						}
					}
					
					game_over = 0x00;
					score = 0;
					
					height = 0;
					width = 3;
					
					led_arr[height][width] = 3;
					
					task1.state = init;
					task1.period = getMovement_period;
					task1.elapsedTime = getMovement_period;
					
					task2.state = mO_init;
					task2.period = moveObject_period;
					task2.elapsedTime = moveObject_period;
					
					task3.state = mW_init;
					task3.period = moveWalls_period;
					task3.elapsedTime = moveWalls_period;
					
					task4.state = pS_init;
					task4.period = powerupShooting_period;
					task4.elapsedTime = powerupShooting_period;
					
					task5.state = pM_wait;
					task5.period = playMusic_period;
					task5.elapsedTime = playMusic_period;
					
					GND = 0x01;
					B2 = 0x01;
					PWM_on();
					i = 0;
					row = 0;
					seeder = 0;
					powerup_activated = 0x00;
					powerup_remainingTime = 0x00;
					powerup_heightCounter = 0x01;
					counter = 7;
					pos_0 = pos_1 = pos_2 = pos_3 = pos_4 = pos_5 = pos_6 = pos_7 = 0;
					shift();
					break;
				}
				
				else {
					continue;
				}
			}
		}
		
		else if(game_over == 0x01) {
			/* Fill led_arr with 0's */
			for(int i = 0; i < 8; i++) {
				for(int j = 0; j < 8; j++) {
					led_arr[i][j] = 0;
				}
			}
			
			while(1) {
				B2 = ~PINB & 0x02;
				led_arr[6][0] = 2;
				led_arr[6][1] = 2;
				led_arr[6][2] = 2;
				led_arr[6][5] = 2;
				led_arr[6][6] = 2;
				led_arr[6][7] = 2;
				led_arr[5][0] = 2;
				led_arr[5][2] = 2;
				led_arr[5][5] = 2;
				led_arr[5][7] = 2;
				led_arr[4][0] = 2;
				led_arr[4][1] = 2;
				led_arr[4][2] = 2;
				led_arr[4][5] = 2;
				led_arr[4][6] = 2;
				led_arr[4][7] = 2;
				
				led_arr[0][7] = 2;
				led_arr[1][6] = 2;
				led_arr[2][5] = 2;
				led_arr[2][4] = 2;
				led_arr[2][3] = 2;
				led_arr[2][2] = 2;
				led_arr[1][1] = 2;
				led_arr[0][0] = 2;
				shift();
				PWM_off();
				
				if(B2 == 2) {
					
					/* Fill led_arr with 0's */
					for(int i = 0; i < 8; i++) {
						for(int j = 0; j < 8; j++) {
							led_arr[i][j] = 0;
						}
					}
					
					game_over = 0x00;
					score = 0;
					
					height = 0;
					width = 3;
					
					led_arr[height][width] = 3;
					
					task1.state = init;
					task1.period = getMovement_period;
					task1.elapsedTime = getMovement_period;
					
					task2.state = mO_init;
					task2.period = moveObject_period;
					task2.elapsedTime = moveObject_period;
					
					task3.state = mW_init;
					task3.period = moveWalls_period;
					task3.elapsedTime = moveWalls_period;
					
					task4.state = pS_init;
					task4.period = powerupShooting_period;
					task4.elapsedTime = powerupShooting_period;
					
					task5.state = pM_wait;
					task5.period = playMusic_period;
					task5.elapsedTime = playMusic_period;
					
					GND = 0x01;
					B2 = 0x01;
					PWM_on();
					i = 0;
					row = 0;
					seeder = 0;
					powerup_activated = 0x00;
					powerup_remainingTime = 0x00;
					powerup_heightCounter = 0x01;
					counter = 7;
					pos_0 = pos_1 = pos_2 = pos_3 = pos_4 = pos_5 = pos_6 = pos_7 = 0;
					shift();
					break;
				}
				
				else {
					continue;
				}
			}
		}
		
		while(!TimerFlag);
		TimerFlag = 0;
	}
	
	return 0;
}