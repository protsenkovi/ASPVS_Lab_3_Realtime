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
#define mutex_shared_name "/mshared"
#define cond_var_shared_name "/condshared"

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

void shared_cond_var_open(const char *name, int *var, int *fd) {
	*fd = shm_open(name, O_RDWR, 0777);
	if(*fd == -1) {
		fprintf(stderr, "%s: Error attaching shared memory '%s': %s\n",
				PROCNAME, name, strerror(errno));

		exit(EXIT_FAILURE);
	}
	var = mmap(0, sizeof(int),
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
struct sigevent eventInterv;
struct itimerspec timerInterv;
int chid;

void timer_init(double dt) {
	int coid;
	if ((chid = ChannelCreate(0)) == -1) {
		fprintf(stderr, "%s: can't create channel!\n", PROCNAME);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	// Connecting to own channel
	coid = ConnectAttach(0, 0, chid, 0, 0);
	if (coid == -1) {
		fprintf(stderr, "%s: connection error", PROCNAME);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	SIGEV_PULSE_INIT(&eventInterv, coid, SIGEV_PULSE_PRIO_INHERIT, CODE_TIMER, 0);
	// Creating timer
	if (timer_create(CLOCK_REALTIME, &eventInterv, &timerInterv) == -1) {
		fprintf(stderr, "%s: timer creation error", PROCNAME);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	timerInterv.it_value.tv_sec = 0;
	timerInterv.it_value.tv_nsec = 0;
	timerInterv.it_interval.tv_sec = 0;
	timerInterv.it_interval.tv_nsec = dt * 1000000000;
}

int flag_started = 0;
int *fd_shared_mem;
double* shared_mem;
double t;

static void sigusr_hndlr(int signo) {
	if (signo == SIGUSR1) {
		printf("P1 after barrier. SIGUSR1 received.\n");
		if (!flag_started) {
			timer_settime(timerid, 0, &timer, NULL);
		}
	}
	if (signo == SIGUSR2) {
		if (flag_started) {
			close(*fd_shared_mem);
			exit(EXIT_SUCCESS);
		}
	}
}

double f(double t) {
	return 100 * exp(-t) * cos(0.1*t + 100);
}

void timer_tick_handle() {
	t += dt;
	*shared_mem = f(t);
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
	printf("3\n");

	printf("P1: cond_var %i\n", *cond_var);

	// Initializing interval timer.
	timer_init(dt);

	printf("P1 before barrier\n");
	kill(ppid, SIGUSR1);
	while(!flag_started) {
		pause();
	}

	struct _pulse *pulse;
	while(1) {
		MsgReceivePulse(chid, pulse, sizeof(struct pulse),NULL);
		timer_tick_handle();
	}
	return EXIT_SUCCESS;
}
