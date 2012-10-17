#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define T 83.0
#define dt 0.03
#define deltaT 0.2
#define barrier_shared_name "/bshared"
const char *PROCNAME = "P0 process";

int main(int argc, char *argv[]) {
	printf("P0 started.\n");
	int fd;
	pthread_barrier_t *barrierStart;
	printf("%i\n", sizeof(pthread_barrier_t));
	shm_unlink(barrier_shared_name);
	fd = shm_open(barrier_shared_name, O_CREAT | O_RDWR, 0777);
	if(fd == -1) {
		fprintf(stderr, "%s: Error attaching shared memory '%s': %s\n",
				PROCNAME, barrier_shared_name, strerror(errno));

		exit(EXIT_FAILURE);
	}
	printf("fields = %i\n", fd);
	int status = ftruncate(fd, sizeof(pthread_barrier_t));
	if ( status == -1 ) {
		fprintf(stderr, "%s: Error truncating shared memory '%s': status: %i\n%s\n",
						PROCNAME, barrier_shared_name, status,strerror(errno));
		exit(EXIT_FAILURE);
	}
	barrierStart = mmap(0, sizeof(pthread_barrier_t),
						PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0777);
	if (barrierStart == MAP_FAILED) {
		fprintf(stderr, "%s: Error shared mem maping. %s\n", PROCNAME, strerror(errno));
		exit(EXIT_FAILURE);
	}
	pthread_barrier_init(barrierStart, NULL, 3);

	char *argv0 = (char*)malloc(2*sizeof(float) + 7*sizeof(char));
	*(float*)((int)argv0) = dt;
	*(float*)((int)argv0 + 4) = T;
	*(char*)((int)argv0 + 8) = barrier_shared_name;
	int pid_1 = spawnl(P_NOWAITO, "/tmp/P1.c", argv0, NULL);

	*(float*)((int)argv0) = deltaT;
	*(float*)((int)argv0 + 4) = T;
	*(char*)((int)argv0 + 8) = barrier_shared_name;
	int pid_2 = spawnl(P_NOWAITO, "/tmp/P2.c", argv0, NULL);

	printf("P0 at barrier.\n");
	pthread_barrier_wait(barrierStart);
	close(fd);
	printf("Huston, P0 is shutting down.\n");
	return EXIT_SUCCESS;
}
