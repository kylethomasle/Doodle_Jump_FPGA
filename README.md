# Final Project - Simple Doodle Jump
# Professor: Anas Salah Eddin: Assistant Professor, ECE-department, College of Engineering, California State Polytechnic University, Pomona 
Submitted by Kyle Thomas Le and Jonathan Suarez 

## Description
For our final project, we created a game heavily inspired by the popular mobile game, Doodle Jump. To acheieve this goal, we pulled from the multible labs we completed for class. This project utilizes the PS2 communication core (keyboard), XADC core, and the vga core for display on the VGA.

Similar to the mobile game, this game aims to make an infinitely generated platformer, which only ends when the character falls off the screen. As the character jumps, the screen moves along with it.  

## Game Functions 
This game uses a custom made character model that automatically jumps upon collision with a platorm. This collision is only checked when the character is falling, as the character is allowed to jump through platforms. The user has the ability to move the character along the x-axis. Depending on the voltage from the XADC the character will change its x direction. In addition, the user has the ability to pause and unpause the at anytime with the keyboard. 

## Movement Legend
  * If the voltage is greater than the reference voltage + threshold, the character moves right: **character_x=character_x+2**
  * If the voltage is less than the reference voltage - threshold, the character moves left: **character_x=character_x-2**
  * If the voltage is within the reference voltage +/- threshold, the character does not move: **character_x=character_x**

## KeyBoard Configuration
  * "R" - causes the game to start
  * "P" - causes the game to enter the paused state
  * "U" - causes the game to exit the paused state
  * "Y" - causes the game to start again, prompted at the Game Over screen

## Video Demonstration Link
https://youtu.be/HokU4xT6EE8
