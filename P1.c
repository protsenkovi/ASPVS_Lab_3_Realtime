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

#define CODE_TIMER 1

const char *PROCNAME = "P1 process";

int main(int argc, char *argv[]) {
	printf("P1 started.\n");
	float dt =  atof(argv[0]);
	float T =  atof(argv[1]);
	char *barrier_name = argv[2];
	printf("dt = %f, T = %f, barrier_name  = %s\n", dt, T, barrier_name);

	// Initializing barrier
	pthread_barrier_t *barrierStart;
	int	fd = shm_open(barrier_name, O_RDWR, 0777);
	if(fd == -1) {
		fprintf(stderr, "%s: Error attaching shared memory '%s': %s\n",
				PROCNAME, barrier_name, strerror(errno));
		exit(EXIT_FAILURE);
	}
	barrierStart = mmap(0, sizeof(pthread_barrier_t),
						PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (barrierStart == MAP_FAILED) {
		fprintf(stderr, "%s: Error shared mem maping. %s\n", PROCNAME, strerror(errno));
		exit(EXIT_FAILURE);
	}

	// Initializing interval timer.
	timer_t timerid;
	struct sigevent eventInterv; // one sigvevent for two timers possible?
	struct itimerspec timerInterv;
	int chid, coid;
	if ((chid = ChannelCreate(0)) == -1) {
		fprintf(stderr, "%s: can't create channel!\n", PROCNAME);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	// connect to own channel
	coid = ConnectAttach(0, 0, chid, 0, 0);
	if (coid == -1) {
		fprintf(stderr, "%s: connection error", PROCNAME);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	SIGEV_PULSE_INIT(&eventInterv, coid, SIGEV_PULSE_PRIO_INHERIT, CODE_TIMER, 0);
	// create timer
	if (timer_create(CLOCK_REALTIME, &eventInterv, &timerInterv) == -1) {
		fprintf(stderr, "%s: timer creation error", PROCNAME);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	timerInterv.it_value.tv_sec = 0;
	timerInterv.it_value.tv_nsec = 0;
	timerInterv.it_interval.tv_sec = 0;
	timerInterv.it_interval.tv_nsec = dt * 1000000000;

	printf("barrier = %i, count = %i", barrierStart->__barrier, barrierStart->__count);
	// TODO shared memory
	// barrier sharing problem  through shared memory?
	printf("P1 before barrier\n");
	pthread_barrier_wait(barrierStart);
	printf("P1 after barrier\n");

	return EXIT_SUCCESS;
}

double f(double t) {
	return 100 * exp(-t) * cos(0.1*t + 100);
}
