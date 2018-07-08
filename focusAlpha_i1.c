/*	Distributed Memory Buffer Application								*/
/*													*/
/*	"focusAlpha.c" contains the implementations of a master logger programm, who acts as 		*/
/*	a client for other logger executions on different machines, sending log_requests for 		*/
/*	their local memory buffer and appending them all into a single string, saving it on an 		*/
/*	output file. The slave execution of the logger programm acts like a passive entity, waiting 	*/
/*	for log_requests from the master node.								*/
/*													*/
/*	developed by: Luiz G. Xavier and Gabriel Moraes		April-May/2018				*/

//SOCKETS
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>

//THREADS
#include<pthread.h>
#include<unistd.h>

//SHARED MEMORY
#include<sys/shm.h>

//LIST FILES
#include<dirent.h>

//DEFAULT
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

//SEMAPHORE
#include "sem.h"

#define BUF_SIZE 100
#define CFG_SIZE 3
#define SHM_KEY 1239
#define QUEUE_SIZE 10

typedef struct {

	char memory[4096];
	int *sem_id;
}shared_struct;

typedef struct
{
	int newSock;
	int mem_size;
	int k;
	char **addresses;
	int *ports;
	shared_struct *pointer;

}args_struct;

//APEND CHAR TO STRING
void append(char* s, char c)
{
		int len = strlen(s);
		s[len] = c;
		s[len+1] = '\0';
}

//RETURN LOG NUMBER
int getLogNumber(void)
{
	DIR *d;
	struct dirent *dir;
	int log_n = 0;
	int i = 0;
	char *number;
	char c;

	d = opendir(".");
	char *dot;
	dot = malloc(sizeof(char));
	number = malloc(7*sizeof(char));
	if(d)
	{
		while ((dir = readdir(d)) != NULL)
		{   
			dot = strrchr(dir->d_name, '.');
			if((dir->d_name[0] == 'l' && dir->d_name[1] == 'o' && dir->d_name[2] == 'g') && 
				(dot && !strcmp(dot, ".txt")))
			{
				memset(number, 0, 7);
				for(int i=0; dir->d_name[3+i] != '.'; i++)
				{
					c = dir->d_name[3+i];
					append(number, c);
				}
				
				if(atoi(number) > log_n)
					log_n = atoi(number);
			}
		}
		closedir(d);
		log_n++;
	}

	return(log_n);
}

int le_config(int *args)
{
	FILE *config;
	int k, mem_size;

	config = fopen("config.txt", "r");
	
	// read server's main config parameters
	if(config)
	{	
		fscanf(config, "%d", &mem_size);
		fscanf(config, "%d", &k);
		getc(config);
	}

	args[0] = k;
	args[1] = mem_size;

	fclose(config);
}

int le_config2(char **addresses, int *ports, int k)
{
	char config_read[50];
	char config_read2[50];
	FILE *config;
	int lixo;

	config = fopen("config.txt", "r");

	if(config)
	{	
		fscanf(config, "%d", &lixo);
		fscanf(config, "%d", &lixo);
		getc(config);
	}

	// read ip adresses
	for(int x=0; x < k; x++)
	{	
		fgets(config_read, 49, config);
		config_read[strcspn(config_read, "\n")] = 0; //REMOVE /n
		strcpy(addresses[x], config_read);
		fgets(config_read2, 49, config);
		ports[x] = atoi(config_read2)+10; //not the same server's ports
	}

	fclose(config);
}

int setMaster()
{
	FILE *config;

	config = fopen("config.txt", "a");

	if(config)
	{	
		fprintf(config, "\n%d", 1);
	}

	fclose(config);
}

int verifyMaster()
{
	FILE *config;
	char line[50];

	config = fopen("config.txt", "r");

	if(config)
	{	
		while(fscanf(config, "%s", line)!=EOF)
		{	
			if(strcmp(line, "1") == 0)
				return 1;
		}
	}

	fclose(config);
}

void *master(void *args)
{	
	//GENERAL VARIABLES
	char op;
	char con;
	char *request;
	char *msg;
	char *memory;
	char *all_memory;
	int mem_size;
	int total_size;
	int k, y;

	//ARQUIVO
	FILE *arq;
	char file_name[10];

	//SOCKET VARIABLES
	struct sockaddr_in server;
	int len;
	int log_c;
	int result;
	int *sockets;
	struct sockaddr_in channel;
	struct hostent *h;
	
	//SHARED MEMORY VARIABLES
	int running = 1;
	shared_struct *shared_stuff;
	int shmid;
	void *shared_memory = (void*) 0;

	args_struct *res = args;

	mem_size = res->mem_size;
	k = res->k;
	total_size = k*mem_size;

	shared_struct *m = res->pointer;

	memory = malloc(mem_size*sizeof(char));
	request = malloc(BUF_SIZE*sizeof(char));
	msg = malloc(BUF_SIZE*sizeof(char));
	sockets = malloc(k*sizeof(int));

	all_memory = malloc(total_size*sizeof(char));


	//SHARED MEMORY ACCESS
	shmid = shmget((key_t)SHM_KEY, sizeof(shared_struct), 0666 | IPC_CREAT);
	if(shmid == -1)
	{
		fprintf(stderr, "shmget failed\n");
		exit(EXIT_FAILURE);
	}

	shared_memory = shmat(shmid, (void*)0, 0);
	if(shared_memory == (void*)-1)
	{
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	} 

	printf("Memory attached at %X\n", (int)shared_memory);
	shared_stuff = (shared_struct*) shared_memory;
	//---------------------------------------------

	sprintf(request, "log_request");

	printf("Establish connection? [y/n]\n");
	scanf("%c", &con);
	
	if(con == 'y')
	{	
		// connect with other loggers (sockets[0] has its own addr)
		for(int i = 1; i < k; i++)
		{	
			h = gethostbyname(res->addresses[i]);
			if (!h) 
				perror("gethostname failed");

			sockets[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			printf("Server Port: %d \n", res->ports[i]);
			memset(&channel, 0, sizeof(channel));
			channel.sin_family = AF_INET;
			memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
			channel.sin_port = htons(res->ports[i]);

			result = (int) connect(sockets[i], (struct sockaddr *) &channel, sizeof(channel));
			if (result < 0) 
				perror("connect failed");

			printf("Connected! Logger IP: %s:%d\n", res->addresses[i], res->ports[i]);
		}

		while(1)
		{
			memset(all_memory, 0, total_size);
			memset(memory, 0, mem_size);
			sprintf(all_memory, ""); // initialize total memory

			printf("Request log? [y/n]\n");
			getchar(); //LIMPA BUFFER
			scanf("%c", &op);
			
			//semaphore_up(shared_stuff->sem_id);
			// saves its own buffer into total
			for(int j = 0; j < mem_size; j++)
			{	
				append(all_memory, shared_stuff->memory[j]);
			}
			//semaphore_down(shared_stuff->sem_id);
			
			for(int i = 1; i < k; i++)
			{				
				if(op == 'y')
				{	
					send(sockets[i], request, BUF_SIZE, 0);
					recv(sockets[i], memory, mem_size+1, 0);
					printf("Memory Logger %d Sent\n",i);

					for(int x = 0; x < mem_size; x++)
					{
						append(all_memory, memory[x]);
					}
				}
				else if(op == 'n')
				{
					if(shmdt(shared_memory) == -1)
					{
						fprintf(stderr, "shmdt failed\n");
						exit(EXIT_FAILURE);
					}
					if(shmctl(shmid, IPC_RMID, 0) == -1)
					{
						fprintf(stderr, "shmctl(IPC_RMID) failed\n");
						exit(EXIT_FAILURE);
					}
				}
			}

			//LOG CREATION
			log_c = getLogNumber();
			
			sprintf(file_name, "log%d.txt",log_c);
			
			arq = fopen(file_name,"w");

			printf("Criando arquivo log...\n");

			for(int i=0; i < total_size; i++)
			{
				//printf("all: %c\n", all_memory[i]);
				fprintf(arq, "%d | %c\n", i, all_memory[i]);
			}

			printf("Pronto!\n");
			fclose(arq);
		}
	
		// close conections
		for(int i = 1; i < k; i++)
			close(sockets[i]);
	}
	//-----------------------------------------------------
	pthread_exit(NULL);
}

void *slave(void *args)
{
	int on = 1; //CONTROLE DO SOCKET ALWAYS ON

	//VARIAVEIS
	char *request;
	char *msg;
	char *memory;
	int mem_size;

	//VARIAVEIS SOCKET
	struct sockaddr_in server, client;
	int newSock;
	int server_len, client_len;
	int sock, bnd, lsn;
	struct sockaddr_in channel;

	pthread_t cliente;

	//SHARED MEMORY VARIABLES
	int running = 1;
	shared_struct *shared_stuff;
	int shmid;
	void *shared_memory = (void*) 0;

	args_struct *res = args;

	mem_size = res->mem_size;

	request = malloc(BUF_SIZE*sizeof(char));
	msg = malloc(BUF_SIZE*sizeof(char));
	memory = malloc(mem_size*sizeof(char));

	// struct addr construction
	memset(&channel, 32, sizeof(channel));
	channel.sin_family = AF_INET;
	channel.sin_addr.s_addr = htonl(INADDR_ANY);

	sock = (int) socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) 
		perror("Socket Fail");
	
	// connect on his specific port
	for(int i = 1; i < res->k; i++)
	{
		channel.sin_port = htons(res->ports[i]);

		bnd = (int) bind(sock, (struct sockaddr *) &channel, sizeof(channel));
		if (bnd == 0) 
		{
			printf("Port: %d\n", res->ports[i]);
	 		break;
		}
	}
	
	lsn = (int) listen(sock, QUEUE_SIZE);
	if (lsn < 0) 
		perror("Listen Failed");

	//SHARED MEMORY ACCESS
	shmid = shmget((key_t)SHM_KEY, sizeof(shared_struct), 0666 | IPC_CREAT);
	if(shmid == -1)
	{
		fprintf(stderr, "shmget failed\n");
		exit(EXIT_FAILURE);
	}

	shared_memory = shmat(shmid, (void*)0, 0);
	if(shared_memory == (void*)-1)
	{
		fprintf(stderr, "shmat failed\n");
		exit(EXIT_FAILURE);
	} 

	printf("Memory attached at %X\n", (int)shared_memory);
	shared_stuff = (shared_struct*) shared_memory;
	//---------------------------------------------
	
	newSock = accept(sock, (struct sockaddr *)&channel, (void *)&client_len);
	if(newSock > 0)
	{
		res->newSock = newSock;
		printf("Client connected!\n");
	}

	while(1)
	{
		memset(memory, 0, mem_size);
		memset(request, 0, BUF_SIZE);
		sprintf(memory, "");

		recv(newSock, request, BUF_SIZE, 0);
		
		if(strcmp(request, "log_request") == 0)
		{
			printf("Log request received. Sending information...\n");
			
			// theres no need for semaphore BETWEEN logs because they just have a READ operation
			//semaphore_up(shared_stuff->sem_id);
			for(int i = 0; i < mem_size; i++)
			{	
				append(memory, shared_stuff->memory[i]);
			}
			//semaphore_down(shared_stuff->sem_id);

			send(newSock, memory, mem_size+1, 0);
			printf("Memory: %s\n", memory);
		}
		else
		{
			printf("Log request failed\n");
			getchar();
		}
	}
	close(newSock);
	pthread_exit(NULL);
}

int main()
{   
	pthread_t master_p;
	pthread_t slave_p;
	args_struct *res;

	res = malloc(sizeof(*res));

	//MEMORY VARIABLES
	int k;
	int mem_size;
	int total_size;
	char *all_memory;
	char **addresses;
	int *ports;

	//FILE VARIABLES
	int args[3];	
	
	le_config(args);
	k = args[0];
	mem_size = args[1];

	addresses = malloc(k*sizeof(char*));
	ports = malloc(k*sizeof(int));

	// adresses contains all ip from the config file
	for(int x = 0; x < k; x++)
		addresses[x] = malloc(16*sizeof(char));
	
	le_config2(addresses, ports, k);

	total_size = k * mem_size;

	all_memory = malloc(total_size*sizeof(char));

	res->k = k;
	res->mem_size = mem_size;
	res->addresses = addresses;
	res->ports = ports;
    
	if(!verifyMaster())
	{
		printf("Master Off. Creating Master...\n");
		pthread_create(&master_p, NULL, master, res);
		setMaster();
		pthread_join(master_p, NULL);
	}
	else
	{
		printf("Master On. Creating Slave...\n");
		pthread_create(&slave_p, NULL, slave, res);
		pthread_join(slave_p, NULL);
	}
	
	return 0;
}