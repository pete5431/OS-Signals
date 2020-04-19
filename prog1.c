#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/*

	Program 1

	Parent process

	Four signal-handling processes

	Three signal-generating processes

	One reporting process

	Total 9 processes

	They will run concurrently.
*/


// Two signals used for communication: SIGUSR1 and SIGUSR2
// Shared signal generated/sent counter for both.

typedef struct{

	int r1_received;
	int r2_received;
	int r1_sent;
	int r2_sent;

} SigusCount;

SigusCount* counter;
int shm_id;

char* r1_msg = "SIGUSR1 received\n";
char* r2_msg = "SIGUSR2 received\n";

pid_t* create_handler_processes(int, int);
pid_t* create_signaler_processes(int);
pid_t* create_reporter_process();

void r1_handler_process();
void r2_handler_process();
void signaler_process();
void reporter_process();

void SIGUSR1_handler(int);
void SIGUSR2_handler(int);

int main(int argc, char* argv[]){

	/*
	Create 8 child processes using fork().
	Control execution time. ???
	Wait for each child completion.
	*/

	shm_id = shmget(IPC_PRIVATE, sizeof(SigusCount), IPC_CREAT | 0666);

	counter = (SigusCount*) shmat(shm_id, 0, 0);

	counter->r1_received = 0;
	counter->r2_received = 0;
	counter->r1_sent = 0;
	counter->r2_sent = 0;

	shmdt(counter);

	sigset_t blocked_set;
	sigset_t saved_set;

	sigemptyset(&blocked_set);

	sigaddset(&blocked_set, SIGUSR1);
	sigaddset(&blocked_set, SIGUSR2);

	sigprocmask(SIG_BLOCK, &blocked_set, &saved_set);

	pid_t* handler_processes = create_handler_processes(2,2);

	pid_t* signaler_processes = create_signaler_processes(3);
	
	pid_t* reporter_process = create_reporter_process();

	int i = 0;
	int status = 0;

	while(i <= 3){
		waitpid(handler_processes[i], &status, 0);
		printf("Handler Process %d exited with status %d\n", handler_processes[i], status);
		i++;
	}

	i = 0;

	while(i <= 2){
		waitpid(signaler_processes[i], &status, 0);
		printf("Signaler Process %d exited with status %d\n", signaler_processes[i], status);
		i++;
	}

	waitpid(reporter_process[0], &status, 0);

	printf("Reporter Process %d exited with status %d\n", reporter_process[0], status);

	counter = (SigusCount*) shmat(shm_id, 0, 0);

	printf("R1 count: %d\n", counter->r1_received);

	printf("R1 sent: %d\n", counter->r1_sent);

	shmdt(counter);

	free(handler_processes);
	free(signaler_processes);
	free(reporter_process);
	shmctl(shm_id, IPC_RMID, NULL);

	return 0;
}

 // Signal generating processes run in a loop generating signals.
 // Picks one of the two at random.
 // Sends the signal to processes in its group (peers). ???
 // Random time delay between .01 and .1 seconds before next repetition of loop.

void r1_handler_process(){

	signal(SIGUSR1, SIGUSR1_handler);

	sigset_t saved_set;

	sigset_t blocked_set;

	sigaddset(&blocked_set, SIGUSR1);
	
        sigprocmask(SIG_UNBLOCK, &blocked_set, &saved_set);

	while(1){

		//signal(SIGUSR1, SIGUSR1_handler);

		//sleep(5);

	}
}

void r2_handler_process(){

	while(1){

		//break;		
	}

}

void signaler_process(){

	pid_t group_pid = getpgrp();

	while(1){

		// Invoke kill system call to request kernel to send signal SIGUSR1 to processes in this group.
		killpg(0, SIGUSR1);

		counter = (SigusCount*) shmat(shm_id, 0, 0);

		(counter->r1_sent)++;

		shmdt(counter);

		//break;
	}
}

void reporter_process(){

	//FILE* fp = fopen("reports.txt", "w");

	sigset_t saved_set;

        sigset_t blocked_set;

        sigaddset(&blocked_set, SIGUSR1);
	
	sigaddset(&blocked_set, SIGUSR2);

        sigprocmask(SIG_UNBLOCK, &blocked_set, &saved_set);	

	while(1){

		//break;	
		
	}
}

void SIGUSR1_handler(int sig){

	write(STDOUT_FILENO, r1_msg, strlen(r1_msg));

	counter = (SigusCount*) shmat(shm_id, 0 ,0);

       	(counter->r1_received)++;

       	shmdt(counter);
}

void SIGUSR2_handler(int sig){

	write(STDOUT_FILENO, r2_msg, strlen(r2_msg));

	counter = (SigusCount*) shmat(shm_id, 0, 0);
	(counter->r2_received)++;
	shmdt(counter);	
}

pid_t* create_handler_processes(int r1_num, int r2_num){

	pid_t* handler_processes = malloc((r1_num + r2_num) * sizeof(pid_t));

	if(handler_processes == NULL){
		printf("Error: Memory Allocation Failed.\n");
		exit(1);
	}

	int i = 0;

	for(; i < r1_num; i++){
		handler_processes[i] = fork();
		if(handler_processes[i] < 0){
			printf("Forking Error\n");
			exit(1);
		}
		else if(handler_processes[i] == 0){
			
			r1_handler_process();
			
			exit(1);
		} 
	}
	for(; i < r1_num + r2_num; i++){
		handler_processes[i] = fork();
		if(handler_processes[i] < 0){
			printf("Forking Error\n");
			exit(1);
		}
		else if(handler_processes[i] == 0){
			r2_handler_process();
			exit(1);
		}
	}

	return handler_processes;
}

pid_t* create_signaler_processes(int num){

	pid_t* signaler_processes = malloc((num) * sizeof(pid_t));

	if(signaler_processes == NULL){
		printf("Error: Memory Allocation Failed.\n");
		exit(1);
	}
	
	for(int i = 0; i < num; i++){
		signaler_processes[i] = fork();
		if(signaler_processes[i] < 0){
			printf("Forking Error\n");
			exit(1);
		}
		else if(signaler_processes[i] == 0){
			signaler_process();
			exit(1);
		}
	}

	return signaler_processes;
} 

pid_t* create_reporter_process(){

	pid_t* reporter = malloc(1 * sizeof(pid_t));

	if(reporter == NULL){
		printf("Error: Memory Allocation Failed\n");
		exit(1);
	}

	reporter[0] = fork();
	if(reporter[0] < 0){
		printf("Forking Error\n");
		exit(1);
	}
	else if(reporter[0] == 0){
		reporter_process();
		exit(1);
	}
	return reporter;
}
