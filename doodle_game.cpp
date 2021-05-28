/*
 * doodle_game.cpp
 *
 *  Created on: Nov 28, 2020
 *      Author: Kyle
 */

//#define _DEBUG
// Our Libraries
#include "chu_init.h"
#include "gpio_cores.h"
#include "vga_core.h"
#include "ps2_core.h"
#include "uart_core.h"
#include "xadc_core.h"

// Standard libraries
#include "stdio.h"
#include "math.h"

// Definitions
#define NUM_HORIZ_LINES 15	// Number of vertical squares
#define NUM_VERT_LINES 20	// Number of horizontal squares
#define Y_REFERENCE 4		// Y coordinate reference the screen focuses on
#define XADC_REFERENCE 0.5	// XADC reference that is the median value
#define XADC_OFFSET 0.3		// XADC offset which determines direction

// Global variables
int platform_location[NUM_HORIZ_LINES * 2][NUM_VERT_LINES];	// Platform array, store an extra "screen" of values
int square_width;			// Coordinate square width, determined by (width of screen / number vertical squares)
int square_height;			// Coordinate square height, determined by (height of screen / number horizontal squares)
int character_x;			// Character current x position
int character_y;			// Character current y position
int score = 0;				// Score


/**
 * Debug method to view the stored platforms in platform_location
 * over UART
 *
 * @note: It is outputted from the bottom of the screen printed
 * 		first
 */
void print_locations()
{
	for(int y = 0; y < NUM_HORIZ_LINES * 2; y++)
	{
		for(int x = 0; x < NUM_VERT_LINES; x++)
		{
			uart.disp(platform_location[y][x]);
		}
		uart.disp("\n");
	}

	uart.disp("\n\r\n\r");
}

/**
 * Generate the background of the game
 *
 * @param: frame_p FrameCore pointer
 */

void grid_draw( FrameCore *frame_p ) {

	// Get the resolution dimensions of frame
	int vmax = frame_p -> VMAX;
	int hmax = frame_p -> HMAX;

	// Set global variables for the width and height of the
	// generated squares
	square_width = hmax / NUM_VERT_LINES;
	square_height = vmax / NUM_HORIZ_LINES;

	// Make sure frame is shown
	frame_p->bypass(0);

	// Beige color
	frame_p->clr_screen(0x1FE);

	// Vertical Grid Lines (Gray)
	for( int i = 0; i < hmax; i = i + square_width )
	{
		for( int j = 0; j < vmax; j++ )
		   frame_p -> wr_pix(i, j, 0x1B5);
	}

	// Horizontal Grid lines (Darker Beige)
	for( int i = 0; i < vmax; i = i + square_height )
	{
	   for( int j = 0; j < hmax; j++ )
		   frame_p -> wr_pix(j, i, 0x1B5);
	}
}

/**
 * Draw a square based on its input coordinate. Start at
 * the bottom left corner of the square, going down-up
 * and left-right
 *
 * @param: frame_p FrameCore pointer
 * @param: x_square integer saying the x coordinate of the
 * 		box to be drawn
 * @param: y_square integer saying the y coordinate of the
 * 		box to be drawn
 *
 * @note: This is used for drawing the platforms
 */

void square_draw(FrameCore *frame_p, int x_square, int y_square)
{
	// Generate starting pixels based on the input coordinates
	// @note: Since our screen is draw from the top down, the
	//		y-coordinate is reversed to calculate
	int x_pixel_start = x_square * square_width;
	int y_pixel_start = (NUM_HORIZ_LINES - y_square - 1) * square_height;

	// Draw a green box, starting from the bottom left of the square
	for(int x = x_pixel_start; x < x_pixel_start + square_width; x++)
	{
		for(int y = y_pixel_start; y < y_pixel_start + square_height; y++)
			frame_p -> wr_pix(x, y, 0x028);
	}
}

/**
 * Restore a square based on its input coordinate. Start
 * from the bottom of the box to the top. This includes
 * redrawing the grid lines
 *
 * @param: frame_p FrameCore pointer
 * @param: x_square integer saying the x coordinate of the
 * 		box to be restored
 * @param: y_square integer saying the y coordinate of the
 * 		box to be restored
 *
 * @note: This is used for refilling squares of previous
 * 		platforms when shifting the screen
 */
void square_restore(FrameCore *frame_p, int x_square, int y_square)
{
	// Generate starting pixels based on the input coordinates
	// @note: Since our screen is draw from the top down, the
	//		y-coordinate is reversed to calculate
	int x_pixel_start = x_square * square_width;
	int y_pixel_start = (NUM_HORIZ_LINES - y_square - 1) * square_height;

	// Restore the bottom of the square, which is the horizontal
	// gray line
	for(int x = x_pixel_start; x < x_pixel_start + square_width; x++)
		frame_p -> wr_pix(x, y_pixel_start, 0x1B5);

	// Restore the middle portion of the square
	// @note: leftmost and rightmost pixels being the vertical gray grid lines
	// @note: all other pixels are the body, beige color
	for(int x = x_pixel_start; x < x_pixel_start + square_width; x++)
	{
		for(int y = y_pixel_start + 1; y < y_pixel_start + square_height; y++)
		{
			if( (x == x_pixel_start) || (x == x_pixel_start + square_width) )
				frame_p -> wr_pix(x, y, 0x1B5);
			else
				frame_p -> wr_pix(x, y, 0x1FE);
		}
	}

	// Restore the top of the square, which is the horizontal
	// gray line
	for(int x = x_pixel_start; x < x_pixel_start + square_width; x++)
		frame_p -> wr_pix(x, y_pixel_start + square_width, 0x1B5);
}

/**
 * Generate the initial platforms for the start
 * of the game and store in global array
 *
 * @note: Platforms are always two wide
 */

void platform_intialize()
{
	// Reset Array to all 0's
	for(int y = 0; y < NUM_HORIZ_LINES * 2; y++)
	{
		for(int x = 0; x < NUM_VERT_LINES; x++)
			platform_location[y][x] = 0;
	}

	// Generate Starting Platform
	// @note: Row 0 is filled
	for(int x = 0; x < NUM_VERT_LINES; x++)
		platform_location[0][x] = 1;

	int y_rand;
	int x_rand;

	// Generate First Platform
	// @note: Make sure platform will not be added
	//		if in same location as player on start
	y_rand = rand() % 100;
	if( y_rand <= 90 )
		x_rand = rand() % (NUM_VERT_LINES - 2);
	if( !((x_rand == 9) | (x_rand == 10)) )
	{
		platform_location[1][x_rand] = 1;
		platform_location[1][x_rand + 1] = 1;
	}

	// Randomly Generate Rest of Platforms
	// @note: 80% chance that a platform will spawn on that y-level
	// @note: Platform location on x is randomized
	for(int y = 2; y < NUM_HORIZ_LINES * 2; y++)
	{
		y_rand = rand() % 100;
		if( y_rand <= 80 )
		{
			x_rand = rand() % (NUM_VERT_LINES - 2);
			platform_location[y][x_rand] = 1;
			platform_location[y][x_rand + 1] = 1;
		}
	}
}

/**
 * Update platforms by shifting each row downwards. Throw
 * away shifted out rows and generate platforms for the
 * new rows.
 *
 * @param: shift integer that notes by how many rows the
 * 		array is shifted and the topmost rows that need
 * 		platforms generated
 */

void platform_update(int shift)
{

	// Move the array down (shift) amount of arrays
	for(int y = 0 + shift; y < NUM_HORIZ_LINES * 2; y++)
	{
		for(int x = 0; x < NUM_VERT_LINES; x++)
		{
			platform_location[y - shift][x] = platform_location[y][x];
		}
	}

	// Clear the topmost (shift) arrays to 0s, so new
	// platforms can be generated for them
	for(int y = (NUM_HORIZ_LINES * 2) - shift; y < NUM_HORIZ_LINES * 2; y++)
	{
		for(int x = 0; x < NUM_VERT_LINES; x++)
		{
			platform_location[y][x] = 0;
		}
	}

	int y_rand;
	int x_rand;

	// Generate platforms for the topmost (shift) arrays
	for(int i = (NUM_HORIZ_LINES * 2) - shift; i < NUM_HORIZ_LINES * 2; i++)
	{
		y_rand = rand() % 100;
		if( y_rand <= 90 )
		{
			x_rand = rand() % (NUM_VERT_LINES - 2);
			platform_location[i][x_rand] = 1;
			platform_location[i][x_rand + 1] = 1;
		}
	}
}

/**
 * Draw the platforms using the positions from the
 * platform_location array and square_draw method
 *
 * @param: frame_p FrameCore pointer
 *
 * @note: This is used for refilling squares of previous
 * 		platforms when shifting the screen
 */

void platform_draw(FrameCore *frame_p)
{
	// Check if current coordinate in the array has
	// a platform, draw a square there if so
	for(int y = 0; y < NUM_HORIZ_LINES; y++)
	{
		for(int x = 0; x < NUM_VERT_LINES; x++)
		{
			if(platform_location[y][x] == 1)
			{
				square_draw(frame_p, x, y);
			}
		}
	}
}

/**
 * Check if the sprite is sitting on top of a platform,
 * checking if the current coordinate of the sprite is
 * on top of a coordinate with a platform
 *
 * @param: x integer saying the x pixel of the sprite
 * @param: y integer saying the y pixel of the sprite
 *
 * @return: 0 if no collision
 * 			1 if collision
 * 			-1 if out of bounds (bottom of screen)
 *
 * @note: x and y are the pixels at the top leftmost
 * 		of the sprite
 * @note: The sprite is two squares tall. x and y
 * 		represent the top square of the sprite
 * @note: Method checks two coordinates below the
 * 		coordinate of the sprite
 * @note: Method checks one coordinate to the right
 * 		to account for when the sprite is halfway
 * 		across a platform
 */

int collision_check(int x, int y)
{
	// Generate starting coordinate based on the input pixels
	// @note: Since our screen is draw from the top down, the
	//		y-square is reversed to calculate
	int character_xsquare = x / square_width;
	int character_ysquare = (NUM_HORIZ_LINES - 1) - (y / square_height);

	// Check if sprite will touch the bottom of the screen, return -1
	if( character_ysquare == 1 )
		return -1;

	// Check if sprite is touching a platform, return 1 if so
	if(character_xsquare + 1 != NUM_VERT_LINES)
	{
		if( platform_location[character_ysquare - 2][character_xsquare] |
				platform_location[character_ysquare - 2][character_xsquare + 1])
			return 1;
	}

	// Special check when the sprite is on the rightmost wall, to prevent
	// out of bounds error do not check one square to the right
	else
	{
		if( platform_location[character_ysquare - 2][character_xsquare] )
			return 1;
	}

	return 0;
}

/**
 * Death animation for the sprite, flash on and off
 * four times
 *
 * @param: sprite_p SpriteCore pointer
 */

void death_animation(SpriteCore *sprite_p)
{
	for(int i = 0; i < 4; i++)
	{
		sprite_p -> bypass(1);
		sleep_ms(100);
		sprite_p -> bypass(0);
		sleep_ms(100);
	}
}

/**
 * Handles displaying game over message and score
 *
 * @param: osd_p OsdCore pointer
 */

void gameover_draw(OsdCore *osd_p)
{
	// Char message arrays for OSD to display
	static char gameover_message[] = {'G', 'A', 'M', 'E', ' ', 'O', 'V', 'E', 'R'};
	static char score_message[] = { 'F', 'I', 'N', 'A', 'L', ' ', 'S', 'C', 'O', 'R', 'E', ':' };
	static char control_message[] = { '[', 'Y', ']', ' ', 'T', 'O', ' ', 'P', 'L', 'A', 'Y', ' ', 'A', 'G', 'A', 'I', 'N'};

	// Calculate number of digits in the score
	int number_digits;

	if(score == 0)
		number_digits = 1;
	else
		number_digits = floor( log10(abs(score))) + 1;

	// Generate char array for the score;
	char score_char[number_digits];
	sprintf(score_char, "%d", score);

	// Display messages
	osd_p -> clr_screen();

	for (int i = 0; i < 9; i++)
		osd_p->wr_char((40 - (9 / 2)) + i, 4, gameover_message[i], 0);

	for( int i = 0; i < 17; i++)
		osd_p->wr_char((40 - (17 / 2)) + i, 5, control_message[i], 0);

	for (int i = 0; i < 12; i++)
		osd_p->wr_char((40 - (12 / 2)) + i, 7, score_message[i], 0);

	for (int i = 0; i < number_digits; i++)
		osd_p->wr_char((40 - (number_digits / 2)) + i, 8, score_char[i], 0);

	osd_p->bypass(0);
}

/**
 * Handles displaying the score counter during
 * the game
 *
 * @param: osd_p OsdCore pointer
 */

void score_draw(OsdCore *osd_p)
{
	const char score_message[] = {'S', 'C', 'O', 'R', 'E', ':'};

	// Calculate number of digits in the score
	int number_digits;

	if(score == 0)
		number_digits = 1;
	else
		number_digits = floor( log10(abs(score))) + 1;

	// Generate char array for the score;
	char score_char[number_digits];
	sprintf(score_char, "%d", score);

	for(int i = 0; i < 6; i++)
			osd_p -> wr_char(1 + i, 1, score_message[i]);

	for(int i = 0; i < number_digits; i++)
			osd_p -> wr_char(7 + i, 1, score_char[i]);

	osd_p -> bypass(0);
}

/**
 * Handles the pause and unpause functionality,
 * freezing the system if it is paused and polling
 * until the unpause request is met
 *
 * @param: ps2_p Ps2Core pointer
 * @param: osd_p OsdCore pointer
 */

void pause_check(Ps2Core  *ps2_p, OsdCore *osd_p)
{
	// Char message arrays for OSD to display
	const char pause_message[6] = {'P', 'A', 'U', 'S', 'E', 'D'};
	const char subpause_message[14] = {'[', 'U', ']', ' ', 'T', 'O', ' ', 'U', 'N', 'P', 'A', 'U', 'S', 'E'};

	char pause_ch, unpause_ch;

	if(ps2_p->get_kb_ch(&pause_ch))		// Check if a keyboard input was made
	{
		if(pause_ch == 'p')				// If inputted character was 'p', enter paused state
		{
			for(int i = 0; i < 6; i++)	// Display paused messages using OSD
				osd_p -> wr_char((40 - (6 / 2)) + i, 4, pause_message[i]);

			for(int i = 0; i < 14; i++)
				osd_p -> wr_char((40 - (14 / 2)) + i, 5, subpause_message[i]);

			osd_p -> bypass(0);

			// Keep in loop until unpause character 'u' was entered
			while(1)
			{
				if(ps2_p -> get_kb_ch(&unpause_ch))	// Check if a keyboard input was made
				{
					if(unpause_ch == 'u')	// If inputted character was 'u', exit paused state
					{
						osd_p -> clr_screen();	// Clear OSD and hide it
						osd_p -> bypass(1);
						return;					// Exit
					}
				}
			}
		}
	}

	return;
}

/**
 * Handles the main game logic of moving the character,
 * involving updating the screen, checking for collision,
 * and calculating the score
 *
 * @param: ps2_p Ps2Core pointer
 * @param: sprite_p SpriteCore pointer
 * @param: adc_p XadcCore pointer
 * @param: frame_p FrameCore pointer
 * @param: osd_p OsdCore pointer
 */

void char_move(Ps2Core *ps2_p, SpriteCore *sprite_p, XadcCore *adc_p, FrameCore *frame_p, OsdCore *osd_p) {
	int hmax = frame_p -> HMAX;
	int x_temp, y_temp;

	// Constants for calculating the movement using the XADC
	// read_reference is the median value in the XADC read range
	// read_offset is the offset for determining if the character moves
	const double read_reference = XADC_REFERENCE;
	const double read_offset = XADC_OFFSET;

	int isFalling = 0;
	int highest_line = 2;

	// Main loop of the game, runs until the character moves out of bounds and the game is over
	while(1)
	{
		// Jump Logic, separated into 64 steps for fluid movement
		// @note: At 64 steps and character_y decreasing by 2 each step,
		//		total jump is 128 pixels, or 4 coordinates
		for(int i = 0; i < 64; i++)
		{
			// Check for pause
			pause_check(ps2_p, osd_p);
			// Redraw score in case it was removed from the pause
			score_draw(osd_p);

			// Read from XADC, see which direction character is moving
			double read_temp = adc_p -> read_adc_in(0);
			double read_diff = read_reference - read_temp;
			int direction;
			if(abs(read_diff) > read_offset)	// If the difference between reference and read_temp
			{									// is greater than threshold, a direction is specified
				if(read_diff < 0)
					direction = 1;		// If the difference is positive, moving rightwards
				else if(read_diff > 0)		// If the difference is negative, moving leftwards
					direction = -1;
			}
			else 							// Else, no direction is specified
			{
				direction = 0;
			}

			// Use direction to determine the next x_value
			if(direction == -1)
				x_temp = character_x - 2;
			else if(direction == 0)
				x_temp = character_x;
			else if(direction == 1)
				x_temp = character_x + 2;

			// Check if next x_value is within the screen, if so, update
			// global character_x value
			if(x_temp >= 0 && x_temp <= hmax - 32)
			{
				character_x = x_temp;
			}

			// Since jumping, always decrease global character_y. No need for a y_temp
			// since we are not checking for collision when jumping
			// @note: Screen reads pixels with the top being 0 and bottom being the max.
			//		Subtracting means it is going up (jumping)
			character_y -= 2;

			// Update the characters position
			sprite_p -> move_xy(character_x, character_y);
			// Delay between steps, this dictates how smoothly the character moves
			sleep_ms(10);
		}

		// Falling logic, runs indefinitely until either hitting a platform or
		// moving out of bounds
		// @note: Hitting a platform continues the while loop, back to Jumping
		// @note: Moving out of bounds exits the method and the game

		isFalling = 1;	// Reset isFalling flag to 1
		do {
			// Check for pause
			pause_check(ps2_p, osd_p);
			// Redraw score in case it was removed from the pause
			score_draw(osd_p);

			// Reset the lineReference at beginning of each step
			int Y_Reference_temp = Y_REFERENCE;

			// Read from XADC, see which direction character is moving
			double read_temp = adc_p -> read_adc_in(0);
			double read_diff = read_reference - read_temp;
			int direction;
			if(abs(read_diff) > read_offset)	// If the difference between reference and read_temp
			{									// is greater than threshold, a direction is specified
				if(read_diff < 0)
					direction = 1;		// If the difference is positive, moving rightwards
				else if(read_diff > 0)		// If the difference is negative, moving leftwards
					direction = -1;
			}
			else 							// Else, no direction is specified
			{
				direction = 0;
			}

			// Use direction to determine the next x_value
			if(direction == -1)
				x_temp = character_x - 2;
			else if(direction == 0)
				x_temp = character_x;
			else if(direction == 1)
				x_temp = character_x + 2;

			// Check if next x_value is within the screen, if so, update
			// global character_x value
			if(x_temp >= 0 && x_temp <= hmax - 32)
			{
				character_x = x_temp;
			}

			// Falling, set y_temp to the current position + 2, y_temp is used
			// since we need to check for collision
			// @note: Screen reads pixels with the top being 0 and bottom being the max.
			//		Adding means it is going down (falling)
			y_temp = character_y + 2;

			// Only check collision if the next position will be at the bottom of a
			// coordinate, this prevents changes when the sprite is in the middle
			// of a coordinate
			int highest_line_temp;
			if(y_temp % square_height == 0)
			{
				// Check if the next position will cause a collision or go out out of bounds
				int check = collision_check(character_x, y_temp);

				if( check == 1 )	// If it causes a collision
				{
					// Set isFalling flag to 0, to end this while loop and go back to Jumping
					isFalling = 0;

					// Calculate the new y-coordinate the sprite will be on
					// @note: Since the sprite is two squares tall, calculate the y-coordinate of
					//		the bottom sprite
					Y_Reference_temp = NUM_HORIZ_LINES - ((y_temp + square_height) / square_height);

					// Check if the new y-coordinate the character landed on will be the highest,
					// add to score based on how many lines passed
					highest_line_temp = Y_Reference_temp;
					if(highest_line_temp > highest_line)
					{
						score += 100 * (highest_line_temp - highest_line);
						score_draw(osd_p);
						highest_line = highest_line_temp;
					}
				}
				else if( check == -1 )	// If it causes the character to go out of bounds ( game over)
				{
					// Play death animation
					death_animation(sprite_p);
					// Exit this method, ending the game logic
					return;
				}
				else	// If no collision occurs, update global character_y to the new position
				{
					character_y = y_temp;
				}
			}
			else	// If new position is not at the bottom of a coordinate, update global character_y
			{		// to the new position
				character_y = y_temp;
			}

			// Moving screen logic
			int diff;

			// Check if the new coordinate is at least 2 above the reference coordinate,
			// if so, restore the current platforms on screen back to normal, then
			// update the platform_location with the new coordinate difference and
			// redraw the new platform location
			if( Y_Reference_temp - 2 >= Y_REFERENCE)
			{
				diff = Y_Reference_temp - Y_REFERENCE;

				// Check if a platform is at a current coordinate, restore if so
				for(int i = 0; i < NUM_HORIZ_LINES; i++)
				{
					for(int j = 0; j < NUM_VERT_LINES; j++)
					{
						if(platform_location[i][j] == 1)
						{
							square_restore(frame_p, j, i);
						}
					}
				}

				// Update the highest line with the array shift
				highest_line -= diff;
				// Shift the array according to the difference
				platform_update(diff);
				// Draw the new platforms
				platform_draw(frame_p);
				// Move the character down so they are still on the same platform
				character_y += (32 * diff);
			}

			// Update the character position
			sprite_p -> move_xy(character_x, character_y);

			// Delay between steps, this dictates how smoothly the character moves
			sleep_ms(10);

		} while( isFalling );
	}
}

/**
 * Controls the entire game, from initializing it, calling
 * the game logic, displaying OSDs, and handling game over
 *
 * @param: ps2_p Ps2Core pointer
 * @param: sprite_p SpriteCore pointer
 * @param: adc_p XadcCore pointer
 * @param: frame_p FrameCore pointer
 * @param: osd_p OsdCore pointer
 * @param: osd2_p OsdCore pointer ( for score only )
 */

void game_run(Ps2Core *ps2_p, SpriteCore *sprite_p, XadcCore *adc_p, FrameCore *frame_p, OsdCore *osd_p)
{
	// Reset OSDs
	osd_p->set_color(0x0f0, 0x001); // dark gray/green
	osd_p->clr_screen();

	// Display Background Grid Lines
	grid_draw(frame_p);

	// Generate Map in Memory
	platform_intialize();
	print_locations();

	// Display First Platforms
	platform_draw(frame_p);

	// Instantiate Keyboard, if not found, quit.
	int id;

	uart.disp("\n\rPlease Connect Keyboard ");
	id = ps2_p->init();
	uart.disp(id);
	if( id == 1 )
		uart.disp("...Connected!\n\r");
	else
	{
	   uart.disp("...Not Connected, Quitting!\n\r");
	   return;
	}

	// Display Sprite Once Ready
	character_x = 320;
	character_y = (frame_p -> VMAX) - (3* square_height);

	sprite_p -> move_xy(character_x, character_y);
	sprite_p -> bypass(0);

	// Display Game Start title screen on OSD
	const char title_message[] = {'E', 'C', 'E', ' ', '4', '3', '0', '5'};
	const char subtitle_message[] = {'D', 'O', 'O', 'D', 'L', 'E', ' ', 'J', 'U', 'M', 'P'};
	const char ready_message[] = {'P', 'R', 'E', 'S', 'S', ' ', '[', 'R', ']', ' ',
						'T', 'O', ' ', 'S', 'T', 'A', 'R', 'T', '!'};
	const char controls_message[] = {'P', 'A', 'U', 'S', 'E', ':' , ' ', '[', 'P', ']',
							' ','U', 'N', 'P', 'A', 'U', 'S', 'E', ':', ' ', '[', 'U', ']'};

	for(int i = 0; i < 8; i++)
		osd_p -> wr_char((40 - (8 / 2)) + i, 4, title_message[i]);

	for(int i = 0; i < 11; i++)
		osd_p -> wr_char((40 - (11 / 2)) + i, 5, subtitle_message[i]);

	for (int i = 0; i < 19; i++)
		osd_p->wr_char((40 - (19 / 2)) + i, 7, ready_message[i]);

	for( int i = 0; i < 23; i++)
	   osd_p->wr_char((40 - (23 / 2)) + i, 8, controls_message[i]);

   osd_p->bypass(0);

   // Wait for user to input 'r' to start game
   char ready;

   while(1)
   {
	   if(ps2_p->get_kb_ch(&ready))
	   {
		   if(ready == 'r')
			   break;
	   }
	}

   // Remove Game Start title after user is ready
   osd_p -> clr_screen();
   osd_p -> bypass(1);

   // Start Game, Exit if Player Loses or Game Ends
   score = 0;
   score_draw(osd_p);
   char_move(ps2_p, sprite_p, adc_p, frame_p, osd_p);

   // Game Ended, Display Death Animation and "GAME OVER" message on OSD
   death_animation(sprite_p);
   gameover_draw(osd_p);

   char input;

   // Wait for user to input 'y' to re-start the game
   while(1)
   {
	   if(ps2_p->get_kb_ch(&input))
	   {
		   if(input == 'y')
		   {
			   osd_p -> bypass(1);
			   break;
		   }
	   }
	}

   sprite_p -> bypass(1);
}

Ps2Core ps2(get_slot_addr(BRIDGE_BASE, S11_PS2));
SpriteCore mouse(get_sprite_addr(BRIDGE_BASE, V1_MOUSE), 1024);
SpriteCore ghost(get_sprite_addr(BRIDGE_BASE, V3_GHOST), 1024);
SpriteCore doodle(get_sprite_addr(BRIDGE_BASE, V5_USER5), 1024);
FrameCore frame(FRAME_BASE);
XadcCore adc(get_slot_addr(BRIDGE_BASE, S5_XDAC));
OsdCore osd(get_sprite_addr(BRIDGE_BASE, V2_OSD));

int main() {
	ghost.bypass(1);
	mouse.bypass(1);
	osd.bypass(1);

	doodle.bypass(1);

	while(1)
	{
		game_run(&ps2, &doodle, &adc, &frame, &osd);
	}
}

