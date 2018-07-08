#ifndef SEM_H
#define SEM_H

#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

union semun {
	int val;                    /* value for SETVAL */
	struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
	unsigned short int *array;  /* array for GETALL, SETALL */
	struct seminfo *__buf;      /* buffer for IPC_INFO */
};

static int set_sem(int sem_id) {
	
	union semun sem_union;
	sem_union.val = 1;

	if (semctl(sem_id, 0, SETVAL, sem_union) == -1) {
		fprintf(stderr, "semaphore_up failed\n");
		return 0;
	}
	return 1;
}

static int del_sem(int sem_id) {
	
	union semun sem_union;

	if (semctl(sem_id, 0, IPC_RMID, sem_union) == -1) {
		fprintf(stderr, "semaphore_up failed\n");
		return 0;
	}
	return 1;
}

static int semaphore_up(int sem_id) {
	
	struct sembuf sem_b;
	sem_b.sem_num = 0;
	sem_b.sem_op = -1;
	sem_b.sem_flg = SEM_UNDO;

	if (semop(sem_id, &sem_b, 1) == -1) {
		fprintf(stderr, "semaphore_up failed\n");
		return 0;
	}
	return 1;
}

static int semaphore_down(int sem_id) {
	
	struct sembuf sem_b;
	sem_b.sem_num = 0;
	sem_b.sem_op = 1;
	sem_b.sem_flg = SEM_UNDO;

	if (semop(sem_id, &sem_b, 1) == -1) {
		fprintf(stderr, "semaphore_down failed\n");
		return 0;
	}
	return 1;
}

#endif