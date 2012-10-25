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
#include <string.h>

const char *PROCNAME = "P1 process";

timer_t timerid;
struct sigevent event;
struct itimerspec timer;

int *fd_shared_mem;
double **shared_mem;

int *fd_shared_mutex;
pthread_mutex_t **shared_mutex;

int flag_started = 0;
double t, dt;

// Tricky var initialization.
void shared_mem_open(const char *name, double **var, int *fd) {
	*fd = shm_open(name, O_RDWR, 0777);
	if(*fd == -1) {
		fprintf(stderr, "%s: Error attaching shared memory '%s': %s\n",
				PROCNAME, name, strerror(errno));

		exit(EXIT_FAILURE);
	}
	*var = mmap(0, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
	if (*var == MAP_FAILED) {
		fprintf(stderr, "%s: Error shared mem maping. %s\n", PROCNAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
	close(*fd);
}

void shared_mutex_open(const char *name, pthread_mutex_t **mutex, int *fd) {
	*fd = shm_open(name, O_RDWR, 0777);
	if(*fd == -1) {
		fprintf(stderr, "%s: Error attaching shared memory '%s': %s\n",
				PROCNAME, name, strerror(errno));

		exit(EXIT_FAILURE);
	}
	*mutex = mmap(0, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
	if (*mutex == MAP_FAILED) {
		fprintf(stderr, "%s: Error shared mem maping. %s\n", PROCNAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
	close(*fd);
}

void timer_signal_init(double dt) {
	SIGEV_SIGNAL_INIT(&event, SIGUSR1);
	// Creating timer
	if (timer_create(CLOCK_REALTIME, &event, &timerid) == -1) {
		fprintf(stderr, "%s: timer creation error", PROCNAME);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_nsec = dt * 1000000000; // 0 in it_value struct would not lunch timer
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_nsec = dt * 1000000000;
}

double f(double t) {
	return 100 * exp(-t) * cos(0.1*t + 100);
}

void timer_tick_handle() {
	t += dt;
	pthread_mutex_lock(*shared_mutex);
	**shared_mem = f(t);
	pthread_mutex_unlock(*shared_mutex);
}

static void sig_hndlr(int signo) {
	if (signo == SIGUSR1) {
		if (!flag_started) {
			printf(" P1 Starting timer. Timer id: %i, Timer interval: %ld\n", timerid, timer.it_interval.tv_nsec);
			flag_started = 1;
		} else
		{
			timer_tick_handle();
		}
	} else
	if (signo == SIGUSR2) {
			printf(" P1 on termination. SIGUSR2 received.\n");
			if (flag_started) {
				close(*fd_shared_mem);
				exit(EXIT_SUCCESS);
			}
	}
}

void my_barrier(int ppid) {
	// Sending to P0 signal - "we are ready".
	kill(ppid, SIGUSR1);
	// Waiting for SIGUSR1 to come. It will switch flag to started.
	while(!flag_started) {
		pause();
	}
}

int main(int argc, char *argv[]) {
	printf(" P1 started. Gid = %i\n", getgid());
	t = 0;
	shared_mem = (double**)malloc(sizeof(double));
	fd_shared_mem = (int*)malloc(sizeof(int));
	shared_mutex = (pthread_mutex_t**)malloc(sizeof(pthread_mutex_t));
	fd_shared_mutex = (int*)malloc(sizeof(int));

	int ppid =  atoi(argv[0]);
	dt =  atof(argv[1]);
	char *shared_mem_name = argv[2];
	char *shared_mutex_name = argv[3];
	printf(" P1 ppid = %i, dt = %f,  shared_mem_name = %s\n", ppid, dt,  shared_mem_name);

	// Initializing signal handlers.
	// SIGUSR1 - sync signal. SIGUSR2 - terminating signal.
	signal(SIGUSR1, sig_hndlr);
	signal(SIGUSR2, sig_hndlr);

	// Connecting to shared memory
	shared_mem_open(shared_mem_name, shared_mem, fd_shared_mem);
	shared_mutex_open(shared_mutex_name, shared_mutex, fd_shared_mutex);

	// Initializing timer
	timer_signal_init(dt);

	printf(" P1 before barrier\n");
	my_barrier(ppid);
	printf(" P1 after barrier\n");

	// Main loop waiting for signals.
	while(1) {
		pause();
	}
	return EXIT_SUCCESS;
}
