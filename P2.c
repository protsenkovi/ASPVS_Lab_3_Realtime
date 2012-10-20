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
int *fd_shared_mem;
double **shared_mem;
char *shared_mem_name;
timer_t timerid;
struct sigevent event;
struct itimerspec timer;

int flag_started = 0;
double t;
float deltaT;

FILE *resultFile;

void timer_tick_handle() {
	t += deltaT;
	fprintf(resultFile, "%f %f\n", t, **shared_mem);
}

static void sig_hndlr(int signo) {
	if (signo == SIGUSR1) {
		printf("P2 received SIGUSR1\n");
		if (!flag_started) {
			printf("P2 Starting timer. Timer id: %i, Timer interval: %i\n", timerid, timer.it_interval.tv_nsec);

			// Conneting to shared memory
			shared_mem_open(shared_mem_name, shared_mem, fd_shared_mem);

			int status = timer_settime(timerid, 0, &timer, NULL);
			printf("P2 start status: %i\n", status);
			printf("P2 Timer id = %i\n", timerid);
			printf("P2 flag_started ptr = %i, val = %i", &flag_started, flag_started);
			flag_started = 1;
			printf("P2 start complete.\n");
		} else
		{
			printf("P2 tick\n");
			timer_tick_handle();
		}
		printf("P2 sig_hndlr SIGUSR1 end\n");
	} else
	if (signo == SIGUSR2) {
			printf("P2 on termination. SIGUSR2 received.\n");
			if (flag_started) {
				close(*fd_shared_mem);
				close(resultFile);
				exit(EXIT_SUCCESS);
			}
	}
	printf("P2 sig_hndlr end\n");
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

// Tricky var initialization.
void shared_mem_open(const char *name, double **var, int *fd) {
	printf("P2 opening shared memory: %s\n", name);
	*fd = shm_open(name, O_RDONLY, 0777);
	printf("P2 cheking for errors\n");
	if(*fd == -1) {
		fprintf(stderr, "%s: Error attaching shared memory '%s': %s\n",
				PROCNAME, name, strerror(errno));

		exit(EXIT_FAILURE);
	}
	printf("P2 Mapping memory\n");
	*var = mmap(0, sizeof(double), PROT_READ, MAP_SHARED, *fd, 0);
	if (*var == MAP_FAILED) {
		fprintf(stderr, "%s: Error shared mem maping. %s\n", PROCNAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("P2 Shared opened\n");
}

int main(int argc, char *argv[]) {
	printf("P2 started. Gid = %i\n", getgid());
	t = 0;
	shared_mem = (double**)malloc(sizeof(double));
	fd_shared_mem = (int*)malloc(sizeof(int));

	int ppid =  atoi(argv[0]);
	deltaT =  atof(argv[1]);
	shared_mem_name = argv[2];
	printf("P2 ppid = %i, dt = %f,  shared_mem_name = %s\n", ppid, deltaT,  shared_mem_name);
	printf("P2 Flag started ptr = %i, val = %i\n", &flag_started, flag_started);

	// Initializing signals
	signal(SIGUSR1, sig_hndlr);
	signal(SIGUSR2, sig_hndlr);

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
		pause();
	}
	printf("P2 Before close");

	close(resultFile);
	return EXIT_SUCCESS;
}
