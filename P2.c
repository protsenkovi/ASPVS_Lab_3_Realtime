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

const char *PROCNAME = "P2 process.";

void test_parameter(double *val) {
	printf("test_f val = %f\n", val);
	printf("test_f *val = %f\n", *val);
	*val = 3.0;
	printf("test_f val = %f\n", val);
	printf("test_f *val = %f\n", *val);
}


int main(int argc, char *argv[]) {
	printf("P2 started.\n");
	double *value = (double*)malloc(sizeof(double));
	double v2 = 2.0;

	printf("val = %f\n", value);
	printf("*val = %f\n", *value);
	test_parameter(value);
	printf("val = %f\n", value);
	printf("*val = %f\n", *value);

	printf("val = %f\n", v2);
	printf("*val = %f\n", &v2);
	test_parameter(&v2);
	printf("val = %f\n", v2);
	printf("*val = %f\n", &v2);
//	// Initializing interval timer.
//	timer_t timerid;
//	struct sigevent event; // one sigvevent for two timers possible?
//	struct itimerspec timer;
//	int chid, coid;
//	if ((chid = ChannelCreate(0)) == -1) {
//		fprintf(stderr, "%s: can't create channel!\n", PROCNAME);
//		perror(NULL);
//		exit(EXIT_FAILURE);
//	}
//	// connect to own channel
//	coid = ConnectAttach(0, 0, chid, 0, 0);
//	if (coid == -1) {
//		fprintf(stderr, "%s: connection error", PROCNAME);
//		perror(NULL);
//		exit(EXIT_FAILURE);
//	}
//	SIGEV_PULSE_INIT(&event, coid, SIGEV_PULSE_PRIO_INHERIT, CODE_TIMER, 0);
//	// create timer
//	if (timer_create(CLOCK_REALTIME, &event, &timerid) == -1) {
//		fprintf(stderr, "%s: timer creation error", PROCNAME);
//		perror(NULL);
//		exit(EXIT_FAILURE);
//	}
//	timer.it_value.tv_sec = 1;
//	timer.it_value.tv_nsec = 0;
//	timer.it_interval.tv_sec = 1;
//	timer.it_interval.tv_nsec = 0;
//
//	timer_settime(timerid, 0, &timer, NULL);
//
//	int rcvid;
//	struct _pulse *pulse;
//	char *buf = (char*)malloc(100);
//	while(1) {
//		printf("P1 waiting for pulse\n");
//		rcvid = MsgReceive(chid, buf, 100,  NULL);
//		//MsgReceivePulse(chid, pulse, sizeof(struct _pulse),NULL);
//		printf("P1 pulse received\n");
//
//	}
	return EXIT_SUCCESS;
}
