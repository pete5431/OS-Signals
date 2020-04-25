#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

// Two signals used for communication: SIGUSR1 and SIGUSR2
// Shared signal generated/sent counter for both.

typedef struct{

	int r1_received;
	int r2_received;
	int r1_sent;
	int r2_sent;

	pthread_mutex_t r1_received_lock;
	pthread_mutex_t r1_sent_lock;
	pthread_mutex_t r2_received_lock;
	pthread_mutex_t r2_sent_lock;
	pthread_mutexattr_t shared_attr;

} Counter;

typedef struct{

	time_t start_time;

	FILE* fp;
	int report_count;
	int r1_report_count;
	int r2_report_count;
	double r1_time_sum;
	double r2_time_sum;
	double previous_r1_time;
	double previous_r2_time;

} Reporter;

Counter* counter;
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

void signal_handler(int);
void signal_report_handler(int);

void initialize_globals();
double rand_interval();
double rand_prob();

int main(int argc, char* argv[]){

	srand(3394);

	initialize_globals();

	sigset_t blocked_set;
	sigset_t saved_set;
	sigemptyset(&blocked_set);
	sigaddset(&blocked_set, SIGUSR1);
	sigaddset(&blocked_set, SIGUSR2);
	sigaddset(&blocked_set, SIGINT);
	sigprocmask(SIG_BLOCK, &blocked_set, &saved_set);

	counter = (Counter*) shmat(shm_id, 0, 0);

	pid_t* handler_processes = create_handler_processes(2,2);

	pid_t* signaler_processes = create_signaler_processes(3);
	
	pid_t* reporter_process = create_reporter_process();

	sleep(10);

	killpg(0, SIGINT);

	//shmdt(counter);

	int i = 0;
	int status = 0;

        waitpid(reporter_process[0], &status, 0);
        printf("Reporter Process %d exited with status %d\n", reporter_process[0], status);

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

	printf("R1 count: %d\n", counter->r1_received);

	printf("R1 sent: %d\n", counter->r1_sent);

	printf("R2 count: %d\n", counter->r2_received);
	
	printf("R2 sent: %d\n", counter->r2_sent);

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

	signal(SIGUSR1, signal_handler);
	signal(SIGINT, signal_handler);

	sigset_t blocked_set;

	sigemptyset(&blocked_set);	

	sigaddset(&blocked_set, SIGUSR1);

	sigaddset(&blocked_set, SIGINT);
	
        sigprocmask(SIG_UNBLOCK, &blocked_set, NULL);

	while(1){

		pause();
	}
}

void r2_handler_process(){

	signal(SIGUSR2, signal_handler);
	signal(SIGINT, signal_handler);

	sigset_t blocked_set;

	sigemptyset(&blocked_set);

	sigaddset(&blocked_set, SIGUSR2);

	sigaddset(&blocked_set, SIGINT);

	sigprocmask(SIG_UNBLOCK, &blocked_set, NULL);

	while(1){

		pause();
	}

}

void signaler_process(){

	signal(SIGINT, signal_handler);

	sigset_t blocked_set;

        sigemptyset(&blocked_set);

        sigaddset(&blocked_set, SIGINT);

        sigprocmask(SIG_UNBLOCK, &blocked_set, NULL);

	while(1){

		double interval = rand_interval();

		double prob = rand_prob();

		//counter = (Counter*) shmat(shm_id, 0, 0);

		if(prob < 0.50){
			killpg(0, SIGUSR1);

			pthread_mutex_lock(&(counter->r1_sent_lock));
		
			(counter->r1_sent)++;

			pthread_mutex_unlock(&(counter->r1_sent_lock));

		}else{
			killpg(0, SIGUSR2);

			pthread_mutex_lock(&(counter->r2_sent_lock));

			(counter->r2_sent)++;

			pthread_mutex_unlock(&(counter->r2_sent_lock));

		}
		// Invoke kill system call to request kernel to send signal SIGUSR1 to processes in this group.

		//shmdt(counter);

		sleep(interval);
	}
}

void reporter_process(){

	signal(SIGINT, signal_report_handler);
	signal(SIGUSR1, signal_report_handler);
	signal(SIGUSR2, signal_report_handler);

        sigset_t blocked_set;

        sigaddset(&blocked_set, SIGUSR1);
	
	sigaddset(&blocked_set, SIGUSR2);

	sigaddset(&blocked_set, SIGINT);

        sigprocmask(SIG_UNBLOCK, &blocked_set, NULL);	

	while(1){

		pause();
	}
}

double rand_interval(){
	return ((double) rand() / RAND_MAX) * (0.09) + 0.01;
}

double rand_prob(){
	return ((double) rand() / (double) RAND_MAX);
}

void signal_handler(int sig){

	if(sig == SIGUSR1){
		write(STDOUT_FILENO, r1_msg, strlen(r1_msg));

		//signal(SIGUSR1, SIGUSR1_handler);

		//counter = (Counter*) shmat(shm_id, 0, 0);

		pthread_mutex_lock(&(counter->r1_received_lock));

       		(counter->r1_received)++;

		pthread_mutex_unlock(&(counter->r1_received_lock));

		//shmdt(counter);
	}else if(sig == SIGUSR2){
		write(STDOUT_FILENO, r2_msg, strlen(r2_msg));

        	//signal(SIGUSR2, SIGUSR2_handler);

       		//counter = (Counter*) shmat(shm_id, 0, 0);

        	pthread_mutex_lock(&(counter->r2_received_lock));

        	(counter->r2_received)++;

        	pthread_mutex_unlock(&(counter->r2_received_lock));

        	//shmdt(counter);      
	}else if(sig == SIGINT){
		shmdt(counter);
		exit(1);
	}
}

void signal_report_handler(int sig){

	if(sig == SIGUSR1){
		write(STDOUT_FILENO, r1_msg, strlen(r1_msg));
	}
	else if(sig == SIGUSR2){
		write(STDOUT_FILENO, r2_msg, strlen(r2_msg));
	}
	else if(sig == SIGINT){
		shmdt(counter);
		exit(1);
	}
}

void initialize_globals(){

	// Using the shm functions to create a shared memory that contains one SigusCount struct.
        shm_id = shmget(IPC_PRIVATE, sizeof(Counter), IPC_CREAT | 0666);

        counter = (Counter*) shmat(shm_id, 0, 0);

        counter->r1_received = 0;
        counter->r2_received = 0;
        counter->r1_sent = 0;
        counter->r2_sent = 0;

        // Make mutex shared across processes using the following share attribute.
        counter->shared_attr;
        pthread_mutexattr_init(&(counter->shared_attr));
        pthread_mutexattr_setpshared(&(counter->shared_attr), PTHREAD_PROCESS_SHARED);

        pthread_mutex_init(&(counter->r1_received_lock), &(counter->shared_attr));
        pthread_mutex_init(&(counter->r1_sent_lock), &(counter->shared_attr));
	pthread_mutex_init(&(counter->r2_received_lock), &(counter->shared_attr));
	pthread_mutex_init(&(counter->r2_sent_lock), &(counter->shared_attr));

        // Detach the shared memory.
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
