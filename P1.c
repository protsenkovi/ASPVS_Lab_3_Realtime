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

const char *PROCNAME = "P1 process";

void shared_mutex_open(const char *name, pthread_mutex_t *mutex, int *fd) {
	*fd = shm_open(name, O_RDWR, 0777);
	if(*fd == -1) {
		fprintf(stderr, "%s: Error attaching shared memory '%s': %s\n",
				PROCNAME, name, strerror(errno));

		exit(EXIT_FAILURE);
	}
	mutex = mmap(0, sizeof(pthread_mutex_t),
						PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0777);
	if (mutex == MAP_FAILED) {
		fprintf(stderr, "%s: Error shared mem maping. %s\n", PROCNAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void shared_mem_open(const char *name, double *var, int *fd) {
	*fd = shm_open(name, O_RDWR, 0777);
	if(*fd == -1) {
		fprintf(stderr, "%s: Error attaching shared memory '%s': %s\n",
				PROCNAME, name, strerror(errno));

		exit(EXIT_FAILURE);
	}
	var = mmap(0, sizeof(double),
						PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0777);
	if (var == MAP_FAILED) {
		fprintf(stderr, "%s: Error shared mem maping. %s\n", PROCNAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void my_barrier_wait(pthread_mutex_t *mutex, int *cond_var, int number_threads) {
	int condition = 1; printf("4\n");
	pthread_mutex_lock(mutex); printf("5\n");
	*cond_var = *cond_var + 1;
	condition =(*cond_var < number_threads); printf("6\n");
	pthread_mutex_unlock(mutex);
	printf("7\n");
	while(condition) {
		printf("%i im am still working. cond_var = %i\n", getpid(), *cond_var);
		pthread_mutex_lock(mutex);
		condition =(*cond_var < number_threads);
		pthread_mutex_unlock(mutex);
		sleep(10);
	}
}

timer_t timerid;
struct sigevent event;
struct itimerspec timer;
int chid;

void timer_init(double dt) {
	int coid;
	if ((chid = ChannelCreate(0)) == -1) {
		fprintf(stderr, "%s: can't create channel!\n", PROCNAME);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	// Connecting to own channel
	coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
	if (coid == -1) {
		fprintf(stderr, "%s: connection error", PROCNAME);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	SIGEV_PULSE_INIT(&event, coid, SIGEV_PULSE_PRIO_INHERIT, CODE_TIMER, 0);
	// Creating timer
	if (timer_create(CLOCK_REALTIME, &event, &timerid) == -1) {
		fprintf(stderr, "%s: timer creation error", PROCNAME);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_nsec = dt * 1000000000;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_nsec = dt * 1000000000;
}

int flag_started = 0;
int *fd_shared_mem;
double* shared_mem;
double t;

static void sigusr_hndlr(int signo) {
	if (signo == SIGUSR1) {
		printf("P1 after barrier. SIGUSR1 received.\n");
		if (!flag_started) {
			printf("P1 Starting timer. Timer id: %i, Timer interval: %i\n", timerid, timer.it_interval.tv_sec);
			int status = timer_settime(timerid, 0, &timer, NULL);
			printf("Start status: %i\n", status);
			printf("P1 Timer id = %i\n", timerid);
			flag_started = 1;
		}
	}
	if (signo == SIGUSR2) {
		printf("P1 on termination. SIGUSR2 received.\n");
		if (flag_started) {
			close(*fd_shared_mem);
			exit(EXIT_SUCCESS);
		}
	}
}

double f(double t) {
	return 100 * exp(-t) * cos(0.1*t + 100);
}

void timer_tick_handle(double dt) {
	t += dt;
	*shared_mem = f(t);
	printf("t = %f, f(t) = %f\n", t, *shared_mem);
}

int main(int argc, char *argv[]) {
	printf("P1 started.\n");
	t = 0;
	int ppid =  atoi(argv[0]);
	float dt =  atof(argv[1]);
	char *shared_mem_name = argv[2];
	printf("ppid = %i, dt = %f,  shared_mem_name = %s\n", ppid, dt,  shared_mem_name);

	// Initializing signal handlers.
	// SIGUSR1 - sync signal. SIGUSR2 - terminating signal.
	signal(SIGUSR1, sigusr_hndlr);
	signal(SIGUSR2, sigusr_hndlr);

	// Initializing shared memory
	shared_mem = (double*)malloc(sizeof(double));
	fd_shared_mem = (int*)malloc(sizeof(int));
	shared_mem_open(shared_mem_name, shared_mem, fd_shared_mem);

	printf("P1: shared_mem %f\n", *shared_mem);

	// Initializing interval timer.
	timer_init(dt);

	printf("P1 before barrier\n");
	// Sending to P0 signal - "we are ready".
	kill(ppid, SIGUSR1);
	// Waiting for SIGUSR1 to come. It will switch flag to started.
	while(!flag_started) {
		pause();
	}
	printf("P1 after barrier\n");

	int rcvid;
	printf("P1 chid = %i\n", chid);
	printf("_pulse struct size = %i\n", sizeof(struct _pulse));
	struct _pulse *pulse;
	char *buf = (char*)malloc(100);
	while(1) {
		printf("P1 waiting for pulse\n");
		rcvid = MsgReceive(chid, buf, 100,  NULL);
		//MsgReceivePulse(chid, pulse, sizeof(struct _pulse),NULL);
		printf("P1 pulse received\n");
		timer_tick_handle(dt);
	}
	return EXIT_SUCCESS;
}
