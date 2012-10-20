#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/siginfo.h>
#include <sys/neutrino.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <signal.h>

#define CODE_TIMER 1
#define RESULTS_FILE_FULL_NAME "/tmp/results.txt"
const char *PROCNAME = "P2 process.";

timer_t timerid;
struct sigevent event;
struct itimerspec timer;

int flag_started = 0;
int *fd_shared_mem;
double **shared_mem;
double t;
float deltaT;

FILE *resultFile;

void timer_tick_handle() {
	t += deltaT;
	printf("P2 tick");
	fprintf(resultFile, "%f %f", t, **shared_mem);
}

static void sig_hndlr(int signo) {
	if (signo == SIGUSR1) {
		printf("P2 received SIGUSR1\n");
		if (!flag_started) {
			printf("P2 Starting timer. Timer id: %i, Timer interval: %i\n", timerid, timer.it_interval.tv_nsec);
			int status = timer_settime(timerid, 0, &timer, NULL);
			printf("P2 start status: %i\n", status);
			printf("P2 Timer id = %i\n", timerid);
			flag_started = 1;
		} else
		{
			timer_tick_handle();
		}
	} else
	if (signo == SIGUSR2) {
			printf("P2 on termination. SIGUSR2 received.\n");
			if (flag_started) {
				close(*fd_shared_mem);
				close(resultFile);
				exit(EXIT_SUCCESS);
			}
	} else
	{
		fprintf(resultFile,"P2 interrupted by signal = %i\n", signo);
	}
}

void timer_signal_init(double deltaT) {
	SIGEV_SIGNAL_INIT(&event, SIGUSR1);
	// Creating timer
	if (timer_create(CLOCK_REALTIME, &event, &timerid) == -1) {
		fprintf(stderr, "%s: timer creation error", PROCNAME);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_nsec = deltaT * 1000000000; // 0 in it_value struct would not lunch timer
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_nsec = deltaT * 1000000000;
	printf("P2 timer tv_nsec = %i\n ", timer.it_value.tv_nsec);
	printf("P2 timer val tv_nsec = %i\n", timer.it_interval.tv_nsec);
}

int main(int argc, char *argv[]) {
	printf("P2 started.\n");
	t = 0;
	int ppid =  atoi(argv[0]);
	deltaT =  atof(argv[1]);
	char *shared_mem_name = argv[2];
	printf("P2 ppid = %i, dt = %f,  shared_mem_name = %s\n", ppid, deltaT,  shared_mem_name);

	// Initializing signals
	signal(SIGUSR1, sig_hndlr);
	signal(SIGUSR2, sig_hndlr);
	signal(11, SIG_IGN);
	int sig;
	for(sig = _SIGMIN; sig <= _SIGMAX; sig++) {
		if (sig != 11)
			signal(sig, sig_hndlr);
	}

	// Opening file to write trend
	resultFile = fopen(RESULTS_FILE_FULL_NAME, "w+");

	// Initializing timer
	timer_signal_init(deltaT);

	printf("P2 before barrier\n");
	// Sending to P0 signal - "we are ready".
	kill(ppid, SIGUSR1);
	// Waiting for SIGUSR1 to come. It will switch flag to started.
	while(!flag_started) {
		pause();
	}
	printf("P2 after barrier\n");

	while(1) {
		printf("P2 waiting for signal\n");
		pause();
		printf("P2 interrupted by signal\n");
	}
	close(resultFile);
	return EXIT_SUCCESS;
}
