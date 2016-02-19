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

#include "packet.c"

#define WINDOW_SIZE 5
#define TIMEOUT 5000

struct packet WINDOW[WINDOW_SIZE];

int main(int argc, char *argv[]) {
	/* set up the sockets variables */
	int sockfd = 0;
	int mode = 0;
	struct sockaddr_in server_addr, cli_addr;
	int pid, clilen, portnumber;
	struct packet require_pkt, response_pkt;
	FILE *resrc;
	time_t timer;

	if (argc != 2) {
		fprintf(stderr, "ERROR on number of arguments");
		exit(EXIT_FAILURE);
	}

	/* create socket */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR while tryin to open socket");
	memset(&server_addr, '0', sizeof(server_addr));

	portnumber = atoi(argv[1]);
	server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(portnumber);

	/* we want to run an infinite loop so taht server is always running */
	// while (1) {

	// }
	return 0;
}