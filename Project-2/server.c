/* server 
   ------
   - sends information over a server
   - uses a window data structure
   - client will save the file locally
   We use UDP connection for process */

typedef enum {false, true} bool;

#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <stdlib.h>
#include <stdbool.h>

/* additionally added libraries */
#include <netinet/in.h>
#include <strings.h>
#include <sys/wait.h>	// for waitpid() needed to keep server running
#include <sys/stat.h>	// get the stats
#include <signal.h> 	// used for kill()
#include <time.h>

static int const TIMEOUT = 5;
static int const ssthresh = 10;

bool resend = false;
double p_loss = 0;
double p_corr = 0;

/* remember to add p_loss and p_corr later */

/* socket variables to use later */
int sockfd;
socklen_t clilen;
struct sockaddr_in server_addr, cli_addr;

/* struct for window pointer (data structure) */
typedef struct window {
	int start; 				// start of the window
	int window_length; 		// window size
	int send_next;
	int packet_count; 		// number of packets
	int* ACK; 				// 0 - not sent; 1 - sent but no ACK; 2 - sent
	char** packets; 
	int end_command;
	int* resend_command;
	int* timer; 			// keep track of remaining time for each packet
}* window_t;

window_t window;

/* predefine the function to send packets */
void sendPacket(int* command, int command_length, int sock, struct sockaddr* cli_addr, socklen_t clilen);

/* define a makeWindow function */
void makeWindow(FILE* file, int WINDOW_SIZE) {
	/* 1. Turn file into packets 
	      Split 1000 bytes:
	      * 984 data bytes
	      * 16 header bytes
	      	* 4 bytes of seq number
	      	* 8 bytes to store file size
	      	* 4 bytes to store length
	*/
	size_t packet_cap;
	packet_cap = 984;
	char* src = NULL; // string to hold the data
	fseek(file, 0L, SEEK_END); 
	int file_size = ftell(file); // extract size of the file
	src = malloc(sizeof(char)*(file_size+1)); // allocate memory for src
	fseek(file, 0L, SEEK_SET);
	size_t src_length = fread(src, sizeof(char), file_size, file); // read file
	src[src_length] = '\0'; // close with nullbyte

	/* initialize packets */
	char** packets;
	size_t packet_count = src_length / packet_cap;
	if (src_length % packet_cap > 0) {
		packet_count += 1;
	}
	packets = malloc(sizeof(char*)*packet_count); // allocate memory for packets
	int i;
	for (i = 0; i < packet_count; i++) {
		packets[i] = malloc(sizeof(char)*(packet_cap+16)); // allocate memory for each packet
	}
	int count = 0;
	int iter = 0;
	int data_length = 0;

	for (i = 0; i < packet_count; i++) {
		data_length = packet_cap;
		if (src_length - iter < packet_cap)
			data_length = src_length - iter;
		sprintf(packets[i], "%04d", i);
		sprintf(packets[i]+4, "%04d", data_length);
		sprintf(packets[i]+8, "%08d", file_size);
		/* place data into packet */
		while (iter < src_length && count < packet_cap) {
			packets[i][count+16] = src[iter];
			count += 1;
			iter += 1;
		}
		packets[i][count+16] = '\0'; // add nullbyte to end
		count = 0;
	}

	/* 2. Now, we can construct the window */
	window = malloc(sizeof(int)*5 + sizeof(int*)*3 + sizeof(char**)); // allocate enough memory for our window
	window->ACK = malloc(sizeof(int)*packet_count);
	/* use timer for each packet */
	window->timer = malloc(sizeof(int)*packet_count);
	for (i=0; i < packet_count; i++) {
		window->ACK[i] = 0; // set every packet's ACK  to 0
		window->timer[i] = TIMEOUT;
	}
	window->resend_command = malloc(sizeof(int));
	window->resend_command[0] = -1;	

	window->send_next = 0;
	window->packet_count = packet_count;
	window->packets = packets;
	window->start = 0;
	window->window_length = WINDOW_SIZE; /* check on this , should be WINDOW_SIZE*/
	// printf("makeWindow(): \n--------------\nfile_src: %s\nbegin: %d\npacket_count: %d\nwindow size: %d\n\n\n", src, window->start, window->packet_count, window->window_length);
	free(src);
}

/* helper function that checks if packet sending is complete */
bool isComplete() {
	if (window->start + window->window_length < window->packet_count - 1) {
		return false;
	}
	int i;
	for (i = 0; i < window->window_length; i++) {
		if (window->ACK[window->start+i] < 2)
			return false;
	}
	return true;
}

/* DEBUGGING: print the window information */
void printWindow() {
	printf("\n printWindow(): \n-------------- \n");
	printf("seq # | state | window_size | next | packet data \n");
	printf("-------------------------------------------------\n");
	int i;
	for (i = 0; i < window->packet_count; i++) {
		if ((i == window->start || i == window->start + window->window_length-1) && (i == window->send_next))
			printf("%04d |   %d   | *********** |  XX  |  %s \n", i, window->ACK[i], window->packets[i]+16);
		else if (i == window->start || i == window->start + window->window_length-1)
			printf("%04d |   %d   | *********** |      |  %s \n", i, window->ACK[i], window->packets[i]+16);
		else if (i == window->send_next)
			printf("%04d |   %d   |             |  XX  |  %s \n", i, window->ACK[i], window->packets[i]+16);
		else
			printf("%04d |   %d   |             |  XX  |  %s \n", i, window->ACK[i], window->packets[i]+16);
	}
}

/* function to update when recevied an ACK signal: 0 - not sent; 1 - sent but no ACK; 2 - sent 
   STILL need to do extra stuff like congestion control and ploss and pcorr */
void ack_update(int ack_num) {
	if (ack_num < 0) {
		printf("Corrupt ACK received! Discarding...\n");
		return;
	}
	printf("\nReceived ACK number: %d\n\n", ack_num);
	if (window->ACK[ack_num] == 1) { /* sent but no ACK */
		window->ACK[ack_num] = 2;
		/* slowstart extra credit */
		if (isComplete() == false && ssthresh >= window->window_length) {
			printf("Slow Start Algorithm: Current Window Size: %d\n", window->window_length);
			if (window->start + window->window_length < window->packet_count) {
				window->window_length += 1;
				printf("New Window Size: %d\n", window->window_length);
			}
			else {
				printf("Window size cannot be increased, probably due to end of packet\n");
			}
		}
		else {
			if (window->end_command == ack_num) {
				/* congestion avoidance extra credit */
				printf("Congestion Avoidance Algorithm: Current Window Size: %d\n", window->window_length);
				if (window->start + window->window_length < window->packet_count) {
					window->window_length += 1;
					printf("New Window Size: %d\n", window->window_length);
				}
			}
			else
				printf("Window size cannot be increased, probably due to end of packet\n");
		}

		while(window->ACK[window->start] == 2) {
			if (window->start + window->window_length < window->packet_count) {
				window->start += 1;
				if (window->ACK[window->start+1] != 2)
					printf("new start index: %d\n", window->start);
			}
			else {
				printf("cannot increase start index because we are at the end of our sequence\n");
				break;
			}
		}
		while(window->ACK[window->send_next] != 0) {
			if (window->send_next + 1 < window->packet_count) {
				window->send_next += 1;
				printf("New send next: %d\n", window->send_next);
			}
			else {
				printf("no more packets to send.\n");
				break;
			}
		}
	}
	else {
		printf("ACK %d has invalid number.\n", ack_num);
	}
}

/* function to retransmit - helper function for timeOutHandler */
void retransmit(int i, int sock, const struct sockaddr* cli_addr, socklen_t clilen) {
	int k;
	for (k=i; k < window->start + window->window_length; k++) {
		window->ACK[k] = 0;
		window->timer[k] = TIMEOUT;
	}
	window->send_next = i;
	window->start = i;
	window->window_length = 1;
	// printWindow();
	/* implement ploss and pcorr later */
	double r = (rand() % 100) * 1.0 / 100.0;
	if ( r < (1.0 - p_loss - p_corr)) {
		printf("Random generated r: %f\n", r);
		printf("Retransmitting packet : %d\n", i);
		int n = sendto(sock, window->packets[i], 1000, 0, cli_addr, (socklen_t) clilen);
		if (n < 0)
			printf("ERROR: problem sending packet. \n");
		window->ACK[i] = 1; // indicate sent, but no ACK
	}
	else if (r > (1.0 - p_corr)) {
		char corrupt_packet[4];
		sprintf(corrupt_packet, "%d", -1);
		int tmp = sendto(sock, corrupt_packet, strlen(corrupt_packet), 0, cli_addr, (socklen_t) clilen);
		if (tmp < 0)
			error("ERROR: can't write to socket");
		printf("There are packet corruption: sending corrupted packet...\n");
	}
	else {
		printf("Random generated r: %f\n", r);
		printf("Packet %d sent but experienced loss\n", i);
	}
	if (window->send_next == window->packet_count-1)
		window->send_next += 1;
}

/* function to handle timeOuts */ 
void timeOutHandler(int signal) {
	bool resetWindow = false;
	int i;
	for (i = window->start; i < window->start + window->window_length; i++) {
		if (resetWindow == false) {
			window->timer[i] -= 1;
			if (window->timer[i] == 0) {
				printf("timeout occured at packet %d\n", i);
				retransmit(i, sockfd, (struct sockaddr*)&cli_addr, clilen);
				resetWindow = true;
			}
		}
	}
	alarm(1);
}

/* function to prepare sending packets */
int* preSendPacket(int* command_length) {
	/* calculate number of packets to send
	   allocate space for a new packet */
	int send_size = window->start + window->window_length - window->send_next;
	printf("Send packet size: %d \n", send_size);
	if (send_size <= 0) {
		printf("Nothing to send; send_size = 0\n");
		return NULL;
	}
	int* command;
	command = (int *) malloc(send_size * sizeof(int)); // allocate size for the packet
	int i = 0;
	int j = 0;
	while (i < send_size) {
		if (window->ACK[i+ window->send_next] == 0) {
			command[j] = window->send_next+i;
			j += 1;
			i += 1;
		}
		else 
			i += 1;
	}
	*command_length = j;
	return command;
}

/* function for sending packets */
void sendPacket(int* command, int command_length, int sock, struct sockaddr* cli_addr, socklen_t clilen) {
	/* send the packet specified, and update window on sent packets 
	   STILL need to implement ploss and pcorr later */
	int i;
	for (i=0; i < command_length; i++) {
		double r = (rand() % 100) * 1.0 / 100.0;
		int k = command[i];
		if (r <(1.0 - p_loss - p_corr)) {
			printf("Random Generated r: %f\n", r);
			sendto(sock, window->packets[k], 1000, 0, cli_addr, (socklen_t) clilen);
			printf("Sent Packet: %d\n", i);
		}
		else if (r > (1 - p_corr)) {
			char corrupt_packet[4];
			sprintf(corrupt_packet, "%d", -1);
			int n = sendto(sock, corrupt_packet, strlen(corrupt_packet), 0, cli_addr, (socklen_t) clilen);
			if ( n < 0 )
				error("ERROR: can't write to socket");
			printf("There are packet corruption: sending corrupted packet...\n");
		}
		else {
			printf("Packet %d sent but loss\n", command[i]);
		}
		window->ACK[k] = 1; // sent but no ACK
		if (window->send_next == window->packet_count-1)
			window->send_next += 1;
	}
}

/* function to try finding a file and returning it */
FILE* findFile (char* c) {
	FILE* f = fopen(c, "r");
	if (f == NULL)
		fprintf(stderr, "ERROR: can't find file specified");
	printf("File Found! \nFile: %s\n\n", c);
	return f;
}


void error(char *message) {
	perror(message);
	exit(EXIT_FAILURE);
}

/* int MAIN */
int main(int argc, char *argv[]) {
	/* set up the sockets variables */
	// int sockfd = 0;
	// int mode = 0;
	// struct sockaddr_in server_addr, cli_addr;
	// int pid, clilen, portnumber, base, next_sequence_no;
	// packet require_pkt, response_pkt;
	// struct stat st;

	/* set up timeout handler */
	signal(SIGALRM, timeOutHandler);
	int newsockfd, window_length, portnumber, pid, WINDOW_SIZE;
	FILE *resrc;
	/* remember to do p_loss and p_corr later */
	char* tail;

	if (argc < 5) { /* remember to add in p_loss and p_corr */
		fprintf(stderr, "ERROR: format needs: ./server <portnumber> <window_size> <p_loss> <p_corrupt>\n");
		exit(1);
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) 
		error("ERROR: can't open socket");
	memset((char *)&server_addr, 0, sizeof(server_addr));

	portnumber = atoi(argv[1]);
	WINDOW_SIZE = atoi(argv[2]);

	p_loss = strtof(argv[3], &tail);
	p_corr = strtof(argv[4], &tail);

	printf("Input Loss Probability: %f\n", p_loss);
	printf("Input Corruption Porbability: %f\n", p_corr);
	
	server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(portnumber);

    if (bind(sockfd, (struct sockaddr_in*)&server_addr, sizeof(server_addr)) < 0)
    	error("ERROR on binding");

    clilen = sizeof(cli_addr);

    /* load the resrc from buffer */
	while(1) {
		int tmp;
		char buffer[256];
		memset(buffer, 0, 256);
		tmp = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*) &cli_addr, (socklen_t *) &clilen);
		if (tmp < 0) 
			error("ERROR: can't read from socket");
		else {
			resrc = findFile(buffer);
			break;
		}
	}

	/* make our window struct */
	makeWindow(resrc, WINDOW_SIZE);
	// printWindow();

	int command_length; 
	/* Steps to send packet: 
	   1. Prepare to send - check that it is ready */
	int* last_command = preSendPacket(&command_length);
	printf("Preparing to send packet; command length: %d\n", command_length);
	int j = 0;
	// for (j = 0; j < command_length; j++) {
	// 	printf("sending j = %d, %d\n", j, last_command[j]);
	// }
	sendPacket(last_command, command_length, sockfd, (struct sockaddr*) &cli_addr, clilen);
	// free(last_command); 
	command_length = 0;
	// printWindow();

	alarm(1); 

	/* we have sent the packets, but now we need to implement selective repeat */
	/* infinite loop to check for timing */
	while(1) {
		if (resend == true) {
			printf("RESEND NEEDED: preparing the resend\n");
			last_command = preSendPacket(&command_length);
			if (command_length > 0) {
				printf("RESEND NEEDED: preparing the resend\n");
				// printf("%d \n", last_command[0]);
				sendPacket(last_command, command_length, sockfd, (struct sockaddr*) &cli_addr, clilen);
			}
			// free(last_command);
			resend = false; /* no need to resend anymore */	
		}
		/* we have sent everything, receive ACKs now from client */
		int tmp;
		char ack_buffer[256];
		memset(ack_buffer, 0, 256);
		tmp = recvfrom(sockfd, ack_buffer, sizeof(ack_buffer), 0, (struct sockaddr*) &cli_addr, (socklen_t *) &clilen);
		if (tmp < 0)
			error("ERROR can't read from socket");
		else {
			int ack = 0;
			if (strncmp("done", ack_buffer, 4) == 0) {
				printf("\n---------------------\nALL ACKs recieved; process is done\n---------------------\n");
				exit(0);
			}
			ack = atoi(ack_buffer);
			ack_update(ack);

			// printWindow();
			if (isComplete()) {
				printf("SUCCESS: sent packets \n");
				break;
			}
			last_command = preSendPacket(&command_length);
			if (command_length > 0) {
				printf("LAST TIME Preparing to send packet: \n");
				int j = 0;
				for (j = 0; j < command_length; j++) {
						// printf("%d\n", last_command[j]);
				}
				sendPacket(last_command, command_length, sockfd, (struct sockaddr*) &cli_addr, clilen);
			}
			else 
				printf("command_length is 0; nothing to send \n");
			// printWindow();
			free(last_command);
			command_length = 0;
		}
	}
	return 0;
}