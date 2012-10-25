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

#define CODE_TIMER 1
#define RESULTS_FILE_FULL_NAME "/tmp/results.txt"
const char *PROCNAME = "P2 process.";

timer_t timerid;
struct sigevent event;
struct itimerspec timer;

int *fd_shared_mem;
double **shared_mem;

int *fd_shared_mutex;
pthread_mutex_t **shared_mutex;

int flag_started = 0;
double t, deltaT;


int chid;
FILE *resultFile;

// Tricky var initialization. Read only.
void shared_mem_open(const char *name, double **var, int *fd) {
	printf("  P2 Opening shared memory: %s\n", name);
	*fd = shm_open(name, O_RDONLY, 0777);
	printf("  P2 Checking for errors after shm_open.\n");
	if(*fd == -1) {
		fprintf(stderr, "%s: Error attaching shared memory '%s': %s\n",
				PROCNAME, name, strerror(errno));

		exit(EXIT_FAILURE);
	}
	printf("  P2 Mapping memory\n");
	*var = mmap(0, sizeof(double), PROT_READ, MAP_SHARED, *fd, 0);
	if (*var == MAP_FAILED) {
		fprintf(stderr, "%s: Error shared mem maping. %s\n", PROCNAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("  P2 Shared opened\n");
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

static void sig_hndlr(int signo) {
	if (signo == SIGUSR1) {
		if (!flag_started) {
			printf("  P2 Starting timer. Timer id: %i, Timer interval: %ld\n", timerid, timer.it_interval.tv_nsec);
			timer_settime(timerid, 0, &timer, NULL);
			flag_started = 1;
		}
	}
	if (signo == SIGUSR2) {
		printf("  P2 on termination. SIGUSR2 received.\n");
		if (flag_started) {
			close(*fd_shared_mem);
			ChannelDestroy(chid);
			fclose(resultFile);
			exit(EXIT_SUCCESS);
		}
	}
}

void timer_pulse_init(double dt) {
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
	timer.it_value.tv_nsec = dt * 1000000000; // 0 in it_value struct would not lunch timer
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_nsec = dt * 1000000000;
}

void timer_tick_handle() {
	static double temp_value = 0;
	t += deltaT;
	pthread_mutex_lock(*shared_mutex);
	temp_value = **shared_mem;
	pthread_mutex_unlock(*shared_mutex);
	fprintf(resultFile, "%f %f\n", t, temp_value);
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
	printf("  P2 started. Gid = %i\n", getgid());
	t = 0;
	shared_mem = (double**)malloc(sizeof(double));
	fd_shared_mem = (int*)malloc(sizeof(int));
	shared_mutex = (pthread_mutex_t**)malloc(sizeof(pthread_mutex_t));
	fd_shared_mutex = (int*)malloc(sizeof(int));

	int ppid =  atoi(argv[0]);
	deltaT =  atof(argv[1]);
	char *shared_mem_name = argv[2];
	char *shared_mutex_name = argv[3];
	printf("  P2 ppid = %i, dt = %f,  shared_mem_name = %s\n", ppid, deltaT,  shared_mem_name);
	printf("  P2 Flag started ptr = %p, val = %i\n", &flag_started, flag_started);

	// Initializing signals
	signal(SIGUSR1, sig_hndlr);
	signal(SIGUSR2, sig_hndlr);

	// Conneting to shared memory
	shared_mem_open(shared_mem_name, shared_mem, fd_shared_mem);
	shared_mutex_open(shared_mutex_name, shared_mutex, fd_shared_mutex);

	// Opening file to write trend
	resultFile = fopen(RESULTS_FILE_FULL_NAME, "w+");

	// Initializing interval timer.
	timer_pulse_init(deltaT);

	printf("  P2 before barrier\n");
	my_barrier(ppid);
	printf("  P2 after barrier\n");

	// Main loop waiting for pulse messages.
	while(1) {
		MsgReceivePulse(chid, NULL, 0, NULL); // No buffer.
		timer_tick_handle();
	}

	return EXIT_SUCCESS;
}
