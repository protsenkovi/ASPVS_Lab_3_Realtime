#include <stdlib.h>
#include <stdio.h>

#define T 83.0
#define dt 0.03
#define deltaT 0.2


int main(int argc, char *argv[]) {
	printf("P0 started.\n");
	char *argv0 = (char*)malloc(8*sizefof(char));
	*(float*)((int)argv0) = dt;
	*(float*)((int)argv0 + 4) = T;
	int pid_1 = spawnl(P_NOWAITO, "/tmp/P1.c", argv0, NULL);

	*(float*)((int)argv0) = deltaT;
	*(float*)((int)argv0 + 4) = T;
	int pid_2 = spawnl(P_NOWAITO, "/tmp/P2.c", argv0, NULL);
	printf("Huston, P0 is shutting down.\n");
	return EXIT_SUCCESS;
}
