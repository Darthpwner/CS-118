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

typedef struct ACK {
	int m_didReceivePacket;	//Acts as a bool
	int m_sendACK;	//Send an ACK back to the server upon receiving the packet
} ACK;

void sendACK(int received, packet p) {
	//TODO send the ACK back to the server
	printf("Type: %i\n", p.type);
	printf("Sequence_no: %i\n", p.sequence_no);
	printf("Length: %i\n", p.length);
    printf("File Size: %i\n", p.data_size);
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

packet_t charToSeg(char* c) {
    packet_t p_t = malloc(sizeof(packet));
    char seqstr[5];
    char lenstr[5];
    char fsizestr[9];
    char* ptr = c;

    strncpy(seqstr, ptr, 4);
    seqstr[4] = '\0';
    strncpy(lenstr, ptr + 4, 4);
    lenstr[4] = '\0';
    strncpy(fsizestr, ptr + 8, 8);
    fsizestr[8] = '\0';

    int fsize = atoi(fsizestr); //Converts char to int
    p_t -> data = malloc(sizeof(char) * 980);

    p_t -> sequence_no = atoi(seqstr);
    p_t -> length = atoi(lenstr);
    p_t -> data_size = fsize;
    int i;
    for(i = 0; i < p_t -> length; i++) {
        p_t -> data[i] = ptr[i + 20];
    }

    printf("\nParsing segment complete.\n");
    printf("Segment sequence #: %d\n", p_t -> sequence_no);
    printf("Segment data length: %d\n", p_t -> length);
    printf("Total file size: %d\n", p_t -> data_size);
    printf("Data payload: %s\n", p_t -> data);
    return p_t;
}

//Figure out the first two parameters
void receiverAction(int sock, struct sockaddr_in serv_addr, char* filename, double pL, double pC) {
	int fileSize = 0;

    packet_t p_t;   //Creates a packet

    char* allData;
    char buffer[1024];
    int totalSegmentCount;

    char* data;
    int sequence;

    int received[6000]; //Acts a boolean array (0 - false, 1 - true)
    int nextExpected = 0;

    FILE* fp = fopen("receive", "w");
    int pos = 0;

    int n = sendto(sock, filename, strlen(filename), 0, (struct sockaddr_in*) &serv_addr, sizeof(serv_addr));

    //BUGGY!
	if(n < 0) {
		error("ERROR writing to socket");
	}

    int init = 0, didFinish = 0;    //boolean values

    //
    while(!didFinish) {
        //Receive message from socket
        memset(buffer, 0, 1000);
        read(sock, buffer, 1000);
        printf("\nReceived a new message! Message: %s\n", buffer);

        data = malloc(980);

        //Parse the given message
        p_t = charToSeg(buffer);
        printf("After return data: %s\n", p_t -> data);

        //Copy data to allocated buffer
        memcpy(data, p_t -> data, p_t -> length);

        //null terminate for the last segment
        data[p_t -> length] = "\0";

        //First segment received
        if(!init) {
            printf("\nInitializing!\n");
            //Set the total file size on first segment receive
            printf("Filesize: %d\n", p_t -> data_size);
            fileSize = p_t -> data_size;
            allData = malloc(fileSize);   //allocate space for the whole data

            printf("Total final size: %d\n", fileSize);

            //Next, find the total # of segments expected
            totalSegmentCount = fileSize / 1000;    //Change the 1000 value later to a variable
            if(fileSize % 1000 > 0) {
                totalSegmentCount++;    //Add another segment for an incomplete payload
            }

            printf("Total # of segments (max %d bytes each): %d\n", 1000, totalSegmentCount);

            //Set initailized bool to true
            init = 1;
        }

        //Record fields of the received segment
        sequence = p_t -> sequence_no;

        //Update CWnd
        received[sequence] = 1;
        if(sequence == nextExpected) {
            printf("-----\n");
            //Shift if in order segment
            while(received[nextExpected]) {
                //Shift if we see a true value
                nextExpected++;

                printf("Shifting cwnd, next expected segment is %d\n", nextExpected);

                //If we reach the last segment, we have finished
                if(nextExpected == totalSegmentCount) {
                    printf("Reached end of the file! Last segment received.\n");

                    didFinish = 1;
                    break;
                }
            }
        } else {
            printf("-----\nReceived out of order segment. Saving...\n");
        }

        //Write to File
        pos = p_t -> sequence_no * 1000;

        printf("Saving data to file: %s\n", data);
        printf("Writing %d bytes to %d * %d = %d\n", p_t -> length, p_t -> sequence_no, 1000, pos);

        memcpy(allData + pos, data, p_t -> length);

        //Send ACK for the segment
        char seqstr[4];
        sprintf(seqstr, "%d", p_t -> sequence_no);

        free(p_t);  
        free(data);
    }

    fwrite(allData, 1, fileSize, fp);
    free(allData);
    fclose(fp);

    sendto(sock, "done", strlen("done"), 0, (struct sockaddr_in *) &serv_addr, sizeof(serv_addr));  //Write to the socket
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

    //Pass in arguments for packet loss and packet corrupted

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