/*	Distributed Memory Buffer Application								*/
/*													*/
/*	"client.c" is the implementation of the client programm, which is responsible for the user 	*/
/*	interface (console) to insert some data into each distributed buffer. Also, executes the main 	*/
/*	steps for the message authentication and routing, calculating from the 'pos' and 'len' message 	*/
/*	attributes the correct server in which the data has to be stored.				*/
/*													*/
/*	developed by: Luiz G. Xavier and Gabriel Moraes		April-May/2018				*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <unistd.h>

#define BUFF_SIZE 100
#define MAX_DGTS_ADDR 4

void fatal(char *string) {
	printf("%s\n",string);
	exit(1);
}

char* reverse(char* x) {
	// String -> String
	// simple function that reverses a given string
	//
	// (check-expect (reverse "oivelho") ohlevio)
	// ...

	int i, j = strlen(x)-1;
	char *aux = malloc((strlen(x)+1)*sizeof(char));

	for (i = 0; i < strlen(x); i++) {
		aux[j] = x[i];
		j--;
	}
	aux[strlen(x)] = '\0';
	//printf ("reverse: %s \n", aux);
	return aux;
}

char* modify(char* old_buff, int new_pos, int new_len, int rmg, int old_len, int flag) {
	// String, Int, Int -> String
	// receives the buffer input and modifies its values to a new write/read postion and bytes range
	//
	// (check-expect (modify "Escreve(1998, "qwertyu", 7)" 0 5) "Escreve(0, "ertyu", 5")
	// ...

	int i, j;
	char *data = malloc((new_len)*sizeof(char));
	char *output = malloc(BUFF_SIZE*sizeof(char));

	// dividing the input string for each server
	if (strncasecmp(old_buff, "Escreve", 7) == 0) {
		for (i = 0; i < strlen(old_buff); i++) {
			if (old_buff[i] == '"')	{

				i = i + ((old_len - rmg) + 1);
	
				// copy the FIRST n bytes
				memset(&data[0], 0, new_len);
				for (j = 0; j < new_len; j++) {
					data[j] = old_buff[i];
					i++;
				}
				data[j] = '\0';
				break;
			}
		}
		//printf ("data = %s \n", data);
		memset(&output[0], 0, BUFF_SIZE);
		snprintf(output, BUFF_SIZE, "Escreve(%d, \"%s\", %d)", new_pos, data, new_len);
	}

	else {
		memset(&output[0], 0, BUFF_SIZE);
		if (flag == 1)
			snprintf(output, BUFF_SIZE, "Le(%d, %d)", new_pos, new_len);
		else
			snprintf(output, BUFF_SIZE, "Le(%d, %d)", new_pos, new_len - 1);
	}

	return output;
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

	int i, j = 0, k, w = 0, n = 0;
	int *info = malloc(2*sizeof(int));
	char *nbr = "0123456789";
	char *aux = malloc(MAX_DGTS_ADDR*sizeof(char));
	
	//if (strpbrk(buff, nbr) == NULL)

	for (i = 0; i < strlen(buff); i++) {
		if (strchr(nbr, buff[i]) != NULL) {
			n++;
			while ((buff[i] != ',') && (buff[i] != ')'))
				i++;
		}
	}
	if (n < 2)
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

int which_server(int pos, int k, int n) {
	// Int, Int, Int -> Int
	// receives a memory write/read position and calculates the corresponding server's id in which that info is store
	// the num is greater than n*k, produces -1 instead.
	//
	// note: check-expects are made as k=1000, and server counting starts with zero
	// (check-expect (which_server 999) 0)
	// (check-expect (which_server  2490) 1)
	// (check-expect (which_server 3290) 2)
	// (check-expect (which_server 878876) -1)
	// ...

	if ((pos >= n*k) || (pos < 0))
		return -1;

	for (int i = 0; i <= n; i++) {
		if (pos < (i*k))
			return (i-1);
	}
}

int how_may(int* info, int num, int k) {
	// Int Array, Int, Int, Int -> Int
	// receives all server's capacity info. and read/write position and calculates how many server's that operation is
	// going to invoke
	//
	// (check-expect (how_many [50, 200], 0, 100) 3)

	// IDEIA: calcular o numero de servers a serem requisitados para criar um array auxiliar de msm tamanho que contenha a qtde de char a serem
	// escritas em cada servidor, sera necessario usar tbm uma variavel auxiliar para indicar a qual servidor o primeiro dado desde vetor representa,
	// depois utiliazar o return tambem como indice de iteracao para ir enviando as msg a cada server.

	int f_alloc = ((num + 1) * k) - info[0];
	return (((int) ceil(((double)info[1] - f_alloc) / k)) + 1);
}

int in_range(char* buff, int len, int n, int k) {
	// String -> Int(bool repre.)
	// receives the input string and checks if the substring between the " " has the same size as the req. length and if len
	// parameter is within the servers capacity
	//
	// (check-expect (in_range "Escreve(0, "oi", 2)") 1)    ;; true
	// (check-expect (in_range "Escreve(0, "oie", 10)") 0)    ;; false
	//
	// NOTE: I've added the server ranging checking later, thats why the function semantics are inconsistent with the check-expects
	// ...

	int i, j = 0, flag = 1;

	if (strncasecmp(buff, "Escreve", 7) == 0) {
		flag = 0;
		for (i = 0; i < strlen(buff); i++) 
			if (buff[i] == '"')
				break;
		i++;
		for (i; buff[i] != '"'; i++)
			j++;

		if (len != j)
			return 0; 

	}
	if (len > ((n * k) - flag))
		return 0;

	return 1;

}

int main(int argc, char *argv[]){
	
	int c, bytes, n, k, num, i, j, b, hm, remaining, check, read_last = 0;
	char *buffer, *ip_in, *line, *port_in;
	struct hostent *h;
	struct sockaddr_in channel;

	buffer = malloc(BUFF_SIZE*sizeof(char));
	ip_in = malloc(16*sizeof(char));
	line = malloc(BUFF_SIZE*sizeof(char));
	port_in = malloc(6*sizeof(char));

	FILE *config = fopen("config.txt", "r+");

	fscanf(config, "%d\n", &k);
	fscanf(config, "%d\n", &n);

	printf ("k = %d \n", k);
	printf ("n = %d \n", n);
	
	int socket_list[n];
	for (i = 0; i < n; i++) {
		
		fgets(ip_in, 16, config);
		for (j = 0; j < 16; j++) {
			if (ip_in[j] == '\n')
				ip_in[j] = '\0';
		}
		printf("IPv4: %s \n", ip_in);
		h = gethostbyname(ip_in);
		if (!h) 
			fatal("gethostname failed");

		socket_list[i] = (int) socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (socket_list[i] < 0) 
			fatal("socket failed");

		int on = 1;
		//setsockopt(socket_list[i], SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (char *) &on, sizeof(on));

		fgets(port_in, 6, config);
		for (j = 0; j < 6; j++) {
			if (port_in[j] == '\n')
				port_in[j] = '\0';
		}
		printf("Server Port: %s \n", port_in);
		memset(&channel, 0, sizeof(channel));
		channel.sin_family = AF_INET;
		memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
		channel.sin_port = htons(atoi(port_in));

		c = (int) connect(socket_list[i], (struct sockaddr *) &channel, sizeof(channel));
		if (c < 0) 
			fatal("connect failed");

		memset(ip_in, 0, 16);
	}
	fclose(config);

	for (;;) {
		memset(&buffer[0], 0, BUFF_SIZE);

		printf("Waiting for an operation input: \n--> ");
		fgets(buffer, BUFF_SIZE, stdin);

		// eliminate the '\n' added by fgets
		for (i = 0; i < BUFF_SIZE; i++) {
			if (buffer[i] == '\n')
				buffer[i] = '\0';
		}

		printf("Input message: %s \n", buffer);

		int* aux = calloc(2, sizeof(int));
		aux = extract(buffer);
		if (aux == NULL)
			printf("\n==ERROR== Requisition message must contain both position and length values, with no number on its input string. \n\n");
		
		else {
			check = in_range(buffer, aux[1], n, k);
			if (check == 0)
				printf("\n==ERROR== Message length must me equal to the given string size and within the server memory range. \n\n");
			else {
				// note:
				// aux[0] == pos
				// aux[1] == len
				
				num = which_server(aux[0], k, n);			
				hm = how_may(aux, num, k);
				int* alloc = calloc(hm, sizeof(int));
				char* aux_buffer = malloc(BUFF_SIZE*sizeof(char));
				strcpy(aux_buffer, buffer);
				
				remaining = aux[1];
				for (i = 0; i < hm; i++) {
					
					if (i == 0) {

						if (remaining + (aux[0] - (num*k)) <= k)
							alloc[0] = remaining;
						else
							alloc[0] = (num+1)*k - aux[0];
						
						buffer = modify(aux_buffer, aux[0] - (num*k), alloc[0], remaining, aux[1], read_last);
						printf("Sending message to server %d: %s \n", num, buffer);
						send(socket_list[num], buffer, BUFF_SIZE, 0);
						memset(&buffer[0], 0, BUFF_SIZE);
					
					} else {
						
						if (remaining > k)
							alloc[i] = k;
						else {
							alloc[i] = remaining;
							read_last = 1;
						}

						buffer = modify(aux_buffer, 0, alloc[i], remaining, aux[1], read_last);
						printf("Sending message to server %d: %s \n", num+i, buffer);
						send(socket_list[num+i], buffer, BUFF_SIZE, 0);
						memset(&buffer[0], 0, BUFF_SIZE);
					}
					remaining = remaining - alloc[i];
				
					// waiting for server answer...
					bytes = recv(socket_list[num+i], buffer, BUFF_SIZE, 0);
					if (bytes <= 0)
						exit(0);

					printf ("\nServer [%d]=============\n", num + i);
					printf ("%s\n", buffer);
					printf ("========================\n\n");
					
					read_last = 0;
				}
				free(alloc);
				free(aux_buffer);
			}
			free(aux);
		}
	}
	for (i = 0; i < n; i++)
		close(socket_list[i]);
}