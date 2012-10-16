#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <stddef.h>

int main(int argc, char *argv[]) {
	printf("P1 started.\n");
	char *argv0 = argv;

	float dt =  *(float*)((int)argv0);
	float T =  *(float*)((int)argv0 + 4);
	printf("dt = %f, T = %f", dt, T);

	// Initializing interval timer.
	timer_t timerid;
	struct sigevent eventInterv; // one sigvevent for two timers possible?
	struct itimerspec timerInterv;
	int chid, coid;
	if ((chid = ChannelCreate(0)) == -1) {
		fprintf(stdrerr, "%s: can't create channel!\n", PROCNAME);
		perror(NULL);
		exit(EXITE_FAILURE);
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

	//TODO Shared memory
	//TODO barrier
	return EXIT_SUCCESS;
}
