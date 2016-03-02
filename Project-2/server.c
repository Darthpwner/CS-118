/* server 
   ------
   -establish a connection with the client 
   -send packets to it
   -receives ACK
   -retransmits packets for which negative acknowledgement is received
   
   We use UDP connection for process */

#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */

/* additionally added libraries */
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h> /* need this for inet_ntoa */
#include <sys/stat.h>

#include "packet.c"

#define TIMEOUT 5000

int WINDOW_SIZE = 5;

packet *WINDOW = NULL;

void error(char *message) {
	perror(message);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
	/* set up the sockets variables */
	int sockfd = 0;
	int mode = 0;
	struct sockaddr_in server_addr, cli_addr;
	int pid, clilen, portnumber, base, next_sequence_no;
	packet require_pkt, response_pkt;
	struct stat st;
	FILE *resrc;

	if (argc != 3) {
		fprintf(stderr, "ERROR on number of arguments");
		exit(EXIT_FAILURE);
	}

	/* create socket */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR while tryin to open socket");
	memset(&server_addr, '0', sizeof(server_addr));

	portnumber = atoi(argv[1]);
	WINDOW_SIZE = atoi(argv[2]);
	// WINDOW = (struct packet *)malloc(sizeof(WINDOW_SIZE)); /* not sure if right */
	WINDOW = malloc(WINDOW_SIZE*sizeof *WINDOW);
	server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(portnumber);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    	error("ERROR on binding");

    clilen = sizeof(cli_addr);

	/* we want to run an infinite loop so that server is always running */
	while (1) {

		/* divide up the packet */ 
		if (recvfrom(sockfd, &require_pkt, sizeof(require_pkt), 0, (struct sockaddr*) &cli_addr, (socklen_t*) &clilen) < 0)
			error("ERROR: can't receive anything from client");
		printf("SUCCESS: Server received %d bytes from %s: %s\n", require_pkt.length, inet_ntoa(cli_addr.sin_addr), require_pkt.data);

		base = 1;
		next_sequence_no = 1;
		/* clear window*/
		bzero((char *) WINDOW, sizeof(WINDOW));

		/* check if we can open our data */
		resrc = fopen(require_pkt.data, 'rb');
		if (resrc == NULL)
			error("ERROR: can't open resource");

		int itr, packets_total;

		/* split the data into packets */
		stat(require_pkt.data, &st);
		packets_total = st.st_size / DATA_SIZE;
		if (st.st_size % DATA_SIZE != 0)
			packets_total++;
		printf("SPLITTING PACKETS: we have total packets = %d\n", packets_total);

		bzero((char *) &response_pkt, sizeof(response_pkt));
		response_pkt.type = 2;

		/* Attempt to send packets */
		for (itr = 0; itr < WINDOW_SIZE; itr++) {
			response_pkt.sequence_no = itr + 1;
			response_pkt.length = fread(response_pkt.data, 1, DATA_SIZE, resrc);
			WINDOW[itr] = response_pkt;
			if (sendto(sockfd, &response_pkt, sizeof(int)*3+response_pkt.length, 0, (struct sockaddr*) &cli_addr, clilen) < 0)
				error("ERROR: could not sent packet");
			printf("SUCCESS: send packet number %d\n", response_pkt.sequence_no);
			next_sequence_no++;
			time(&response_pkt.timer);
		}

		// /* Selective Repeat Implementation */
		// while (base <= packets_total) {
		// 	/* keep checking until ACK is sent back */

		// }

		bzero((char *) &response_pkt, sizeof(response_pkt));
		response_pkt.type = 3;
		puts("Teardown");
		if (sendto(sockfd, &response_pkt, sizeof(response_pkt.type) * 3, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
			error("ERROR: could not send packet");
		/* we are done. close the resource */
		free(WINDOW); /* since we allocated memory*/
		fclose(resrc);
	}
	return 0;
}