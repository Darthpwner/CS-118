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