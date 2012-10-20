#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define T 3
#define dt 0.03
#define deltaT 0.2
#define shared_mem_name "/memshared"
const char *PROCNAME = "P0 process";
int bthreads, bthreads_max;
int flag_started = 0;
int flag_terminating = 0;

timer_t timerid;
struct sigevent event;
struct itimerspec timer;

static void sig_hndlr(int signo) {
	if (signo == SIGUSR1) {
		printf("P0 Got SIGUSR1\n");
		bthreads++;
		if ((!flag_started) && (bthreads >= bthreads_max)) {
			kill(0, SIGUSR1); //broadcasting start signal
			printf("P0 Starting timer. Timer id: %i, Timer interval (sec): %i\n", timerid, timer.it_value.tv_sec);
			timer_settime(timerid, 0, &timer, NULL);
			flag_started = 1;
		}
	}
	if (signo == SIGUSR2) {
		printf("P0 got SIGUSR2\n");
		if (!flag_terminating) {
			kill(0, SIGUSR2);
			flag_terminating = 1;
		}
	}
}

// Tricky var initialization.
void shared_mem_init(const char *name, double **var, int *fd) {
	*fd = shm_open(name, O_CREAT | O_RDWR, 0777);
	if(*fd == -1) {
		fprintf(stderr, "%s: Error attaching shared memory '%s': %s\n",
				PROCNAME, name, strerror(errno));

		exit(EXIT_FAILURE);
	}
	int status = ftruncate(*fd, sizeof(double));
	if ( status == -1 ) {
		fprintf(stderr, "%s: Error truncating shared memory '%s': status: %i\n%s\n",
						PROCNAME, name, status,strerror(errno));
		exit(EXIT_FAILURE);
	}
	*var = mmap(0, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
	if (*var == MAP_FAILED) {
		fprintf(stderr, "%s: Error shared mem maping. %s\n", PROCNAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
	**var = 1.0;
	close(*fd);
}

void timer_signal_init(int time) {
	SIGEV_SIGNAL_INIT(&event, SIGUSR2);
	// Creating timer
	if (timer_create(CLOCK_REALTIME, &event, &timerid) == -1) {
		fprintf(stderr, "%s: timer creation error", PROCNAME);
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	timer.it_value.tv_sec = time;
	timer.it_value.tv_nsec = 0;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_nsec = 0;
}

void my_barrier() {
	bthreads++;
	while (!flag_started) {
		pause();
	}
}

int main(int argc, char *argv[]) {
	printf("P0 started. Gid = %i\n", getgid());
	int pid = getpid();
	printf("P0 flag_started ptr = %i, val = %i", &flag_started, flag_started);

	// Initializing barrier
	bthreads = 0;
	bthreads_max = 3;
	signal(SIGUSR1, sig_hndlr);
	signal(SIGUSR2, sig_hndlr);

	// Initializing shared memory. To return from init_function pointer on mapped area
	// we should use pointer on pointer.
	double **shared_mem = (double**)malloc(sizeof(double));
	int *fd_shared_mem = (int*)malloc(sizeof(int));
	shared_mem_init(shared_mem_name, shared_mem, fd_shared_mem);
	printf("P0 shared mem value after init = %f\n", **shared_mem);

	// Initializing arguments for P1 and starting it.
	char *argv0 = (char*)malloc(sizeof(float));
	char *argv1 = (char*)malloc(sizeof(float));
	char *argv2 = (char*)malloc(strlen(shared_mem_name) + 1);
	sprintf(argv0, "%i", pid);
	sprintf(argv1, "%f", dt);
	sprintf(argv2, "%s", shared_mem_name);
	int pid_1 = spawnl(P_NOWAIT, "/tmp/P1", argv0, argv1, argv2, NULL);

	// Initializing arguments for P2 and starting it.
	sprintf(argv0, "%i", pid);
	sprintf(argv1, "%f", deltaT);
	sprintf(argv2, "%s", shared_mem_name);
	int pid_2 = spawnl(P_NOWAIT, "/tmp/P2", argv0, argv1, argv2, NULL);

	// Initializing one-shot timer for P1, P2 termination. Notification by signal - SIGUSR2.
	timer_signal_init(T);

	printf("P0 at barrier\n");
	my_barrier();
	printf("P0 after barrier\n");

	// Separating part of process, where user signals could interrupt waitpid().
	while(!flag_terminating) {
		pause();
	}
	// Waiting for child processes exit.
	int st1 = waitpid(pid_1, NULL, WEXITED);
	printf("P0 :P1 termination status %i, errno = %i\n", st1, errno);
	int st2 = waitpid(pid_2, NULL, WEXITED);
	printf("P0 :P2 termination status %i, errno = %i\n", st2, errno);
	printf("P0 Last value of shared_mem = %f\n", **shared_mem);
	// Closing shared memory
	close(*fd_shared_mem);
	shm_unlink(shared_mem_name);  // in some examples memory is unlinked just after getting fd value.
	printf("Huston, P0 is shutting down.\n");
	return EXIT_SUCCESS;
}
