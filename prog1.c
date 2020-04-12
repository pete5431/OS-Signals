#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/*

	Program 1

	Parent process

	Four signal-handling processes

	Three signal-generating processes

	One reporting process

	Total 9 processes
*/


// Two signals used for communication: SIGUSR1 and SIGUSR2
// Shared signal generated/sent counter for both.

int main(int argc, char* argv[]){

	/*
	Create 8 child processes using fork().
	Control execution time. ???
	Wait for each child completion.
	*/	

	// Signal generating processes run in a loop generating signals.
	// Picks one of the two at random.
	// Sends the signal to processes in its group (peers). ???
	// Random time delay between .01 and .1 seconds before next repetition of loop.

	

	return 0;
}
