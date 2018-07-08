/*	Distributed Memory Buffer Application								*/
/*													*/
/*	"server.c" is the implementation of the multiple servers programms that are going to be 	*/
/*	distributed. The server programm has just a tiny ammount of authentication steps, leaving 	*/
/*	the core steps to prevent the traffic of a corrupted/inconsistent message as a client's duty.	*/
/*													*/
/*	developed by: Luiz G. Xavier and Gabriel Moraes		April-May/2018				*/

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/shm.h>
#include "sem.h"

#define BUFF_SIZE 100
#define QUEUE_SIZE 10
#define MAX_DGTS_ADDR 4
#define SHM_KEY 1239

struct mem {
	char memory[4096];
	int *sem_id;
};

struct data {
	int connection;
	struct mem *pointer;
};

void fatal (char *string) {
	printf("%s\n",string);
	exit(1);
}

int autenticar_op(char* stream) {
	// String -> Int
	// receives a string and returns an integer representing:
	// 1- a well formed read operation
	// 2- a well formed write operation
	// 0- incorrect syntax requisition
	//
	// (check-expect (autenticar_op "Escreve(0, "teste", 5)") 1) ;; true
	// (check-expect (autenticar_op "Le(0, 5)") 2) ;; true
	// (check-expect (autenticar_op "Escreve("teste")") 0) ;; false
	// (check-expect (autenticar_op "Escreve(-23, "teste", 9889)") 1) ;; true, althought its not on position range has the right syntax
	// ....
	
	char *nbr = "0123456789";
	char *comma = ",";

	if (strncasecmp(stream, "Escreve", 7) == 0) {
		if ((stream[7] == '(') && (strpbrk(stream, nbr) != NULL) &&
		(strpbrk(stream, comma) != NULL) && (stream[strlen(stream)-1] == ')'))
			return 2;
		return 0;
	}
	else if (strncasecmp(stream, "Le", 2) == 0) {
		if ((stream[2] == '(') && (strpbrk(stream, nbr) != NULL) &&
		(strpbrk(stream, comma) != NULL) && (stream[strlen(stream)-1] == ')'))
			return 1;
		return 0; 
	}

	return 0;
}

int* extract(char* buff) {
	// String -> Int[]
	// receives the input string and extract an int array with the req. position and write length, produces NULL if
	// offset and length values are not present
	//
	// (check-expect (extract "Escreve(990, "abcde", 40)") [990, 40])
	// (check-expect (extract "Le(10, 20)") [10, 20])
	// (check-expect (extract "Le(98)") NULL)
	// ...

	int i, j = 0, k, w = 0;
	int *info = malloc(2*sizeof(int));
	char *nbr = "0123456789";
	char *aux = malloc(MAX_DGTS_ADDR*sizeof(char));
	
	if (strpbrk(buff, nbr) == NULL)
		return NULL;

	for (i = 0; i < strlen(buff); i++) {
		if (strchr(nbr, buff[i]) != NULL) {
			for (k = 0; k < MAX_DGTS_ADDR; k++)
				aux[k] = ' ';
			while ((buff[i] != ' ') && (buff[i] != ')') && (buff[i] != ',')) {
				aux[j] = buff[i];
				j++;
				i++;
			}
			info[w] = atoi(aux);
			j = 0;
			w++;
		}
	}
	return info;
}

char* extract_string(char* old_buff) {
	// String -> String
	// receives the buffer input and extracts its inner string word to be written on the memory
	//
	// (check-expect (extract_string "Escreve(1998, "qwertyu", 7)") "qwertyu")
	// ...

	int i, j;
	char *data = malloc(20*sizeof(char));

	if (strncasecmp(old_buff, "Escreve", 7) == 0) {
		for (i = 0; i < strlen(old_buff); i++) {
			if (old_buff[i] == '"')	{
				i++;
				memset(&data[0], 0, 20);
				for (j = 0; old_buff[i] != '"'; j++) {
					data[j] = old_buff[i];
					i++;
				}
				data[j+1] = '\0';
				//printf ("data = %s \n", data);
				break;
			}
		}
	}
	return data;
}

void *cliente(void *args) {

	int receive, c;
	int i, check, k, n, j;
	char *buffer, *res;
	struct data *d = args;
	
	buffer = malloc(BUFF_SIZE*sizeof(char));
	res = malloc(BUFF_SIZE*sizeof(char));

	FILE *config = fopen("config.txt", "r+");
	fscanf(config, "%d\n", &k);
	fscanf(config, "%d\n", &n);

	printf ("k = %d \n", k);
	printf ("n = %d \n", n);
	fclose(config);
	
	struct mem *m = d->pointer;
	//int sem_id = *(d->sem_id);
	int sem_id = m->sem_id;

	c = d->connection;
	while(receive = (recv(c, buffer, BUFF_SIZE, 0)) > 0)
	{			
		printf ("Read Message: %s\n", buffer);

		check = autenticar_op(buffer);
		sleep(1);
		if (check == 1) {
			//read operation
			printf ("===Correct Read Statement!===\n\n");

			int *aux = calloc(2, sizeof(int));
			aux = extract(buffer);

			semaphore_up(sem_id);
			j = 0;
			for (i = aux[0]; i <= (aux[0] + aux[1]); i++) {
				res[j] = m->memory[i];
				j++;
			}
			semaphore_down(sem_id);

			memset(&buffer[0], 0, BUFF_SIZE);
			snprintf(buffer, BUFF_SIZE, "Memory|%d|->|%d| = %s", aux[0], aux[0] + aux[1], res);
			send(c, buffer, BUFF_SIZE, 0);
		
			free(aux);
		}
		else if (check == 2) {
			//write operation
			printf ("===Correct Write Statement!===\n\n");

			int *aux = calloc(2, sizeof(int));
			aux = extract(buffer);
			char *input = extract_string(buffer);

			semaphore_up(sem_id);

			j = 0;
			for (i = aux[0]; i < (aux[0] + aux[1]); i++) {
				m->memory[i] = input[j];
				j++;
			}

			semaphore_down(sem_id);

			memset(&buffer[0], 0, BUFF_SIZE);
			snprintf(buffer, BUFF_SIZE, "Sucessfully Writed Content at |%d|->|%d|", aux[0], aux[0] + aux[1] - 1);
			send(c, buffer, BUFF_SIZE, 0);

			free(aux);
			free(input);	
		}

		else {
			send(c, "Wrong Requisition Syntax", BUFF_SIZE, 0);
		}

		memset(&res[0], 0, BUFF_SIZE);
		memset(&buffer[0], 0, BUFF_SIZE);
	}
	if(receive == 0)
		printf("Client disconnected!\n");

	free(buffer);
	//del_sem(sem_id);
	pthread_exit(NULL);
}

int main(int argc, char* argv[]) {

	int skt, bnd, lsn, connection;
	int on = 1;
	char *buffer = malloc(BUFF_SIZE*sizeof(char));

	int sem_id, k, i;
	void *shared = (void*) 0;
	int shmid;

	struct sockaddr_in channel;
	struct data *d = malloc(sizeof(struct data*));
	socklen_t addr_size;
	pthread_t client;

	FILE *arq = fopen("config.txt", "r");
	fscanf(arq, "%d", &k);
	fclose(arq);

	if (argc < 2)
		fatal("Insert server port");

	memset(&channel, 32, sizeof(channel));
	channel.sin_family = AF_INET;
	channel.sin_addr.s_addr = htonl(INADDR_ANY);
	channel.sin_port = htons(atoi(argv[1]));

	skt = (int) socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (skt < 0) 
		fatal("Socket Fail");
	
	// keep an unique socket for multiclient communication
	//setsockopt(skt, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (char *) &on, sizeof(on));

	bnd = (int) bind(skt, (struct sockaddr *) &channel, sizeof(channel));
	if (bnd < 0) 
		fatal("Bind Failed");

	lsn = (int) listen(skt, QUEUE_SIZE);
	if (lsn < 0) 
		fatal("Listen Failed");

	// creating shared struct
	struct mem *m;
	
	// configuring shared semaphore
	sem_id = semget((key_t)SHM_KEY, 1, 0666 | IPC_CREAT);
	if (!set_sem(sem_id))
		fatal("Failed to initialize semaphore");
	
	// configuring shared memory
	shmid = shmget((key_t)SHM_KEY, sizeof(struct mem), 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "shmget failed\n");
		exit(EXIT_FAILURE);
	}

	shared = shmat(shmid, (void*)0, 0);
	if (shared == (void*)-1) {
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	}
	printf("Memory attached at %X\n\n", (int)shared);
	m = (struct mem*) shared;

	m->sem_id = sem_id;
	d->pointer = m;
	
	for (i = 0; i < k; i++)
	{
		d->pointer->memory[i] = '-';
	}		

	addr_size = sizeof(struct sockaddr_in);
	while (connection = (int) accept(skt, (struct sockaddr *) &channel, &addr_size)) {
		
		if (connection < 0) 
			fatal("Accept Failed");
		
		printf("Client Connected!\n");
		d->connection = connection;
		pthread_create(&client, NULL, cliente, d);
	}

	if(shmdt(shared) == -1)
	{
		fprintf(stderr, "shmdt failed\n");
		exit(EXIT_FAILURE);
	}

	if(shmctl(shmid, IPC_RMID, 0) == -1)
	{
		fprintf(stderr, "shmctl(IPC_RMID) failed\n");
		exit(EXIT_FAILURE);
	}
	close(skt);
	return 0;
}