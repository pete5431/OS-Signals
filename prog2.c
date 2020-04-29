#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>

#define BILLION 1000000000;

typedef struct{

	int r1_received;
	int r2_received;
	int r1_sent;
	int r2_sent;

	pthread_mutex_t r1_received_lock;
	pthread_mutex_t r1_sent_lock;
	pthread_mutex_t r2_received_lock;
	pthread_mutex_t r2_sent_lock;

} Counter;

typedef struct{

	struct timespec r1_start_time;
	struct timespec r2_start_time;
	struct timespec system_time;

	int r_count;
	int write_report;
	int fp;

	int report_count;
	int r1_report_count;
	int r2_report_count;
	double r1_time_sum;
	double r2_time_sum;

} Reporter;

Counter* counter;
Reporter* reporter;

pthread_t threads[8];

void create_threads();
void* r1_handler_thread(void* arg);
void* r2_handler_thread(void* arg);
void* signaler_thread(void* arg);
void* reporter_thread(void* arg);

void signal_handler(int);
void signal_report_handler(int);

void initialize_counter();
void initialize_reporter();
void rand_sleep();
double rand_prob();

void block_SIGUSR1();
void unblock_SIGUSR1();
void block_SIGUSR2();
void unblock_SIGUSR2();
void block_SIGTERM();
void unblock_SIGTERM();

// Parent thread.
int main(int argc, char* argv[]){

	srand(time(NULL));

	initialize_counter();
	initialize_reporter();

	block_SIGUSR1();
	block_SIGUSR2();
	block_SIGTERM();

	create_threads();

	if(argc == 2){
		int total = atoi(argv[1]);

		while((counter->r1_received + counter->r2_received) < total){
		}
	}
	else{
		sleep(5);
	}	

	for(int i = 0; i < 8; i++){
		pthread_kill(threads[i], SIGTERM);
	}

	for(int i = 0; i < 8; i++){
		pthread_join(threads[i], NULL);
	}

	printf("R1 count: %d\n", counter->r1_received);

        printf("R1 sent: %d\n", counter->r1_sent);

        printf("R2 count: %d\n", counter->r2_received);

        printf("R2 sent: %d\n", counter->r2_sent);		

	free(counter);
	free(reporter);

	return 0;
}

void* r1_handler_thread(void* arg){

	sigset_t signal_targets;
	int accepted_signal;
	sigemptyset(&signal_targets);
	sigaddset(&signal_targets, SIGUSR1);
	sigaddset(&signal_targets, SIGTERM);

	while(1){

		sigwait(&signal_targets, &accepted_signal);

		if(accepted_signal == SIGUSR1){
			pthread_mutex_lock(&(counter->r1_received_lock));
			(counter->r1_received)++;
			pthread_mutex_unlock(&(counter->r1_received_lock));
		}
		else if(accepted_signal == SIGTERM){
			
			pthread_exit(NULL);
		}
	}
}

void* r2_handler_thread(void* arg){

	sigset_t signal_targets;
        int accepted_signal;
        sigemptyset(&signal_targets);
        sigaddset(&signal_targets, SIGUSR2);
        sigaddset(&signal_targets, SIGTERM);

	while(1){

		sigwait(&signal_targets, &accepted_signal);

                if(accepted_signal == SIGUSR2){
                        pthread_mutex_lock(&(counter->r2_received_lock));
                        (counter->r2_received)++;
                        pthread_mutex_unlock(&(counter->r2_received_lock));
                }
                else if(accepted_signal == SIGTERM){
                       	
                        pthread_exit(NULL);
                }
	}
}

void* signaler_thread(void* arg){

	signal(SIGTERM, signal_handler);
	unblock_SIGTERM();
	
	double prob = 0.0;

	while(1){

		prob = rand_prob();

		int signal;

		if(prob < 0.5){
			signal = SIGUSR1;
		}
		else signal = SIGUSR2;

		for(int i = 0; i < 8; i++){
			pthread_kill(threads[i], signal);
		}

		if(signal == SIGUSR1){
			pthread_mutex_lock(&(counter->r1_sent_lock));
			(counter->r1_sent)++;
			pthread_mutex_unlock(&(counter->r1_sent_lock));
		}
		else if(signal == SIGUSR2){
			pthread_mutex_lock(&(counter->r2_sent_lock));
			(counter->r2_sent)++;
			pthread_mutex_unlock(&(counter->r2_sent_lock));
		}
		
		rand_sleep();
	}
}

void* reporter_thread(void* arg){


}

void rand_sleep(){
	
	double interval = ((double) rand() / (double) RAND_MAX) * (0.09) + 0.01;

	struct timespec tm;
	
	tm.tv_sec = 0;
	
	tm.tv_nsec = interval * BILLION;

	nanosleep(&tm, NULL);
}

double rand_prob(){

	return ((double) rand() / (double) RAND_MAX);
}

void signal_handler(int sig){

	if(sig == SIGTERM){

		pthread_exit(NULL);
	}
}

void initialize_counter(){

	counter = (Counter*) malloc(sizeof(Counter));

        counter->r1_received = 0;
        counter->r2_received = 0;
        counter->r1_sent = 0;
        counter->r2_sent = 0;

        pthread_mutex_init(&(counter->r1_received_lock), NULL);
        pthread_mutex_init(&(counter->r1_sent_lock), NULL);
        pthread_mutex_init(&(counter->r2_received_lock), NULL);
        pthread_mutex_init(&(counter->r2_sent_lock), NULL);
}

void initialize_reporter(){

	reporter = (Reporter*) malloc(sizeof(Reporter));

        clock_gettime(CLOCK_MONOTONIC, &(reporter->r1_start_time));

        clock_gettime(CLOCK_MONOTONIC, &(reporter->r2_start_time));

        reporter->fp = open("thread_reports.txt", O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);

        reporter->r_count = 0;

        reporter->report_count = 0;

        reporter->r1_time_sum = 0.0;

        reporter->r2_time_sum = 0.0;

        reporter->r1_report_count = 0;

        reporter->r2_report_count = 0;
}

void create_threads(){

	if(pthread_create(&threads[0], NULL, reporter_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }
        if(pthread_create(&threads[1], NULL, r1_handler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }               
        if(pthread_create(&threads[2], NULL, r1_handler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }
	if(pthread_create(&threads[3], NULL, r2_handler_thread, NULL) != 0){
		printf("Thread create failed.\n");
		exit(1);
	}
	if(pthread_create(&threads[4], NULL, r2_handler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }
	if(pthread_create(&threads[5], NULL, signaler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }
	if(pthread_create(&threads[6], NULL, signaler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }
	if(pthread_create(&threads[7], NULL, signaler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }
}

void block_SIGUSR1(){

        sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGUSR1);

        pthread_sigmask(SIG_BLOCK, &set, NULL);
}

void unblock_SIGUSR1(){

        sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGUSR1);

        pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

void block_SIGUSR2(){

        sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGUSR2);

        pthread_sigmask(SIG_BLOCK, &set, NULL);

}

void unblock_SIGUSR2(){

        sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGUSR2);

        pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

void block_SIGTERM(){

        sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGTERM);

        pthread_sigmask(SIG_BLOCK, &set, NULL);

}

void unblock_SIGTERM(){

        sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGTERM);

        pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

