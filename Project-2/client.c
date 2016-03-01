/*
 A simple client in the internet domain using TCP
 Usage: ./client hostname port (./client 192.168.0.151 10000)
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>

//Added libraries
#include <arpa/inet.h>
//#include "server.c"
#include "packet.c"

 #define BUFLEN 512

typedef struct {
	int m_didReceivePacket;	//Acts as a bool
	int m_sendACK;	//Send an ACK back to the server upon receiving the packet
} ACK;

void sendACK(int received, packet p) {
	//TODO send the ACK back to the server
	printf("Type: %i\n", p.type);
	printf("Sequence_no: %i\n", p.sequence_no);
	printf("Length: %i\n", p.length);

	printf("Data: ");
	int i;
	for(i = 0; i < DATA_SIZE; i++) {
		printf("%c\n", p.data[i]);	
	}
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void test() {
	ACK a;
	a.m_didReceivePacket = 1;

	printf("a's value for didReceivePacket: %i\n", a.m_didReceivePacket);

	packet p;
	p.type = 0;
	p.sequence_no = 1;
	p.length = 2;

	printf("p's value for type: %i\n", p.type);
	printf("p's value for sequence_no: %i\n", p.sequence_no);
	printf("p's value for length: %i\n", p.length);

	// if(a.sendACK(m_didReceivePacket)) {
	// 	printf("SUCCESS\n");
	// } else {
	// 	printf("FAIL\n");
	// }
}

int main(int argc, char *argv[]) {

	test();	//Testing purposes

	//
	int sockfd; //Socket descriptor
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address

    char* filename;

    socklen_t slen = sizeof(serv_addr);

    char buffer[256];
    if (argc < 3) {	//Change this to 6 after we incorporate the error handling
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0); //create a new socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    server = gethostbyname(argv[1]); //takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    //Assign the file name passed in through the command line
    filename = argv[3];

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) //establish a connection to the server
        error("ERROR connecting");

    //This part changes
    while(1) {
    	printf("\nEnter data to send(Type exit and press enter to exit) : ");
    	scanf("%[^\n]", buffer);
    	getchar();
    	if(strcmp(buffer, "exit") == 0) {
    		exit(0);
    	}
    	
    	if(sendto(sockfd, buffer, BUFLEN, 0, (struct sockaddr*)&serv_addr, slen) == -1) {
    		err("sendto()");
    	}
    }


    close(sockfd);
    return 0;
}