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
#define TIMEOUT 1
/* initialize these to false FIRST */
bool test = false;
bool resend = false;

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
	packet_cap = 10;
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
	window->window_length = WINDOW_SIZE; /* check on this */
	printf("makeWindow(): \n--------------\nfile_src: %s\nbegin: %d\npacket_count: %d\nwindow size: %d\n\n\n", src, window->start, window->packet_count, window->window_length);
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

/* function to retransmit */
void retransmit() {

}

/* function to handle timeOuts */ 
void timeOut(int signal) {
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
void preSendPacket() {

}

/* function for sending packets */
void sendPacket(int* command, int command_length, int sock, struct sockaddr* cli_addr, socklen_t clilen) {
	
}


/* function to update when ACKed */


/* function to try finding a file */
FILE* findFile (char* c) {
	return NULL;
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
	signal(SIGALRM, TIMEOUT);
	int newsockfd, window_length, portnumber, pid;
	FILE *resrc;
	/* remember to do p_loss and p_corr later */

	if (argc < 3) { /* remember to add in p_loss and p_corr */
		fprintf(stderr, "ERROR: format needs: ./server <portnumber> <window_size>\n");
		exit(1);
	}
	if (argc == 3)
		test = true;

	char* test_input;
	test_input = argv[1];
	int WINDOW_SIZE = argv[2];
	printf("\n \n in main:() \n -------- \n Requested Filenam: %s \n \n", test_input);
	resrc = findFile(test_input);

	/* make our window struct */
	makeWindow(resrc, WINDOW_SIZE);
	printWindow();

	// int command_length; 
	// /* Steps to send packet: 
	//    1. Prepare to send - check that it is ready */
	// int* last_command = preSendPacket(&command_length);
	// printf("Preparing to send packet: \n");
	// int j = 0;
	// for (j = 0; j < command_length; j++) {
	// 	if (j == command_length - 1)
	// 		printf("%d \n", last_command[j]);
	// 	else
	// 		printf("%d, ", last_command[j]);
	// }
	// sendPacket(last_command, command_length, sockfd, (struct sockaddr*) &cli_addr, clilen);
	// free(last_command);
	// command_length = 0;
	// printWindow();

	// alarm(1); 

	//  we have sent the packets, but now we need to implement selective repeat 
	// /* infinite loop to check for timing */
	// while(1) {
	// 	if (resend == true) {
	// 		printf("RESEND NEEDED: preparing the resend\n");
	// 		last_command = preSendPacket(&command_length);
	// 		if (command_length > 0) {
	// 			printf("RESEND NEEDED: preparing the resend\n");
	// 			printf("%d \n", last_command[0]);
	// 			sendPacket(last_command, command_length, sockfd, (struct sockaddr*) &cli_addr, clilen);
	// 		}
	// 		free(last_command);
	// 		resend = false; /* no need to resend anymore */	
	// 	}
	// 	/* we have sent everything, receive ACKs now from client */
		
	// }

	return 0;

	// /* create socket */
	// sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	// if (sockfd < 0)
	// 	error("ERROR while tryin to open socket");
	// memset(&server_addr, '0', sizeof(server_addr));

	// portnumber = atoi(argv[1]);
	// WINDOW_SIZE = atoi(argv[2]);
	// // WINDOW = (struct packet *)malloc(sizeof(WINDOW_SIZE)); /* not sure if right */
	// WINDOW = malloc(WINDOW_SIZE*sizeof *WINDOW);
	// server_addr.sin_family = AF_INET;
 //    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
 //    server_addr.sin_port = htons(portnumber);

 //    if (bind(sockfd, (struct sockaddr_in*)&server_addr, sizeof(server_addr)) < 0)
 //    	error("ERROR on binding");

 //    clilen = sizeof(cli_addr);

	// /* we want to run an infinite loop so that server is always running */
	// while (1) {

	// 	/* divide up the packet */ 
	// 	if (recvfrom(sockfd, &require_pkt, sizeof(require_pkt), 0, (struct sockaddr_in*) &cli_addr, (socklen_t*) &clilen) < 0)
	// 		error("ERROR: can't receive anything from client");
	// 	printf("SUCCESS: Server received %d bytes from %s: %s\n", require_pkt.length, inet_ntoa(cli_addr.sin_addr), require_pkt.data);

	// 	base = 1;
	// 	next_sequence_no = 1;
	// 	/* clear window*/
	// 	bzero((char *) WINDOW, sizeof(WINDOW));

	// 	/* check if we can open our data */
	// 	resrc = fopen(require_pkt.data, 'rb');
	// 	if (resrc == NULL)
	// 		error("ERROR: can't open resource");

	// 	int itr, packets_total;

	// 	 split the data into packets 
	// 	stat(require_pkt.data, &st);
	// 	packets_total = st.st_size / DATA_SIZE;
	// 	if (st.st_size % DATA_SIZE != 0)
	// 		packets_total++;
	// 	printf("SPLITTING PACKETS: we have total packets = %d\n", packets_total);

	// 	bzero((char *) &response_pkt, sizeof(response_pkt));
	// 	response_pkt.type = 2;

	// 	/* Attempt to send packets */
	// 	for (itr = 0; itr < WINDOW_SIZE; itr++) {
	// 		response_pkt.sequence_no = itr + 1;
	// 		response_pkt.length = fread(response_pkt.data, 1, DATA_SIZE, resrc);
	// 		WINDOW[itr] = response_pkt;
	// 		if (sendto(sockfd, &response_pkt, sizeof(int)*3+response_pkt.length, 0, (struct sockaddr_in*) &cli_addr, clilen) < 0)
	// 			error("ERROR: could not sent packet");
	// 		printf("SUCCESS: send packet number %d\n", response_pkt.sequence_no);
	// 		next_sequence_no++;
	// 		time(&response_pkt.timer);
	// 	}

	// 	// /* Selective Repeat Implementation */
	// 	// while (base <= packets_total) {
	// 	// 	/* keep checking until ACK is sent back */

	// 	// }

	// 	bzero((char *) &response_pkt, sizeof(response_pkt));
	// 	response_pkt.type = 3;
	// 	puts("Teardown");
	// 	if (sendto(sockfd, &response_pkt, sizeof(response_pkt.type) * 3, 0, (struct sockaddr_in *) &cli_addr, clilen) < 0)
	// 		error("ERROR: could not send packet");
	// 	/* we are done. close the resource */
	// 	free(WINDOW); /* since we allocated memory*/
	// 	fclose(resrc);
	// }
	// return 0;
}