
/*
 A simple client in the internet domain using TCP
 Usage: ./client hostname port (./client 192.168.0.151 10000)
 */


#include "segment.h"
#include "sequence.h"


#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>

const int MSS = 1000;
const int MAX_PAYLOAD = 984;

void doSomething( char* filename, int sock, struct sockaddr_in serv_addr, double ploss, double pcorr);

void writeToFile( FILE* file, char* buffer, int offset);

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd; //Socket descriptor
    int portno;
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address
    char* filename;	
    double ploss, pcorr;
    char* tail;

    
    if (argc < 6) {
       fprintf(stderr,"usage %s hostname port filename ploss pcorr\n", argv[0]);
       exit(0);
    }
    
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //create a new socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    server = gethostbyname(argv[1]); //takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
     filename =  argv[3];

	ploss = strtod(argv[4], &tail);
	pcorr = strtod(argv[5], &tail);

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    
    doSomething(filename, sockfd, serv_addr, ploss, pcorr);

    close(sockfd); //close socket
    
    return 0;
}

segment_t charToSeg(char* c){
	segment_t seg_t = malloc (sizeof(segment));
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

	int fsize = atoi(fsizestr);
	//char data[fsize];
	seg_t->data = malloc(sizeof(char)*984);

	//strncpy(data, ptr + 16, fsize);
	//strncpy(seg_t->data, ptr + 16, fsize);

	seg_t->seq = atoi(seqstr);
	seg_t->len = atoi(lenstr);
	seg_t->fileSize = fsize;
	int i;
	for(i=0;i<seg_t->len; i++){
		seg_t->data[i]=ptr[i+16];
	}
	//seg_t->data = data;

	printf("\nParsing segment complete.\n");
	printf("Segment sequence #: %d\n", seg_t->seq);
	printf("Segment data length: %d\n", seg_t->len);
	printf("Total file size: %d\n", seg_t->fileSize);
	printf("Data payload: %s\n", seg_t->data);
	return seg_t;

}

void doSomething(char* filename, int sock, struct sockaddr_in serv_addr, double ploss, double pcorr){
	
	int fileSize = 0;
	segment_t seg;// = malloc(sizeof(segment));
	char* allData;
	char buffer[1000];
	int totalSegmentCount;

	char* data;
	int seq;
	
	bool received[6000];
	int nextExpected = 0;

	FILE* fp = fopen("receive", "w");
	int pos = 0;

	// FIRST SEND THE FILENAME AS A REQUEST
	
	int n = sendto(sock, filename, strlen(filename), 0, (struct sockaddr_in *) &serv_addr, sizeof(serv_addr)); //write to the socket
        if (n < 0) 
             error("ERROR writing to socket");

    // PREPARE TO RECEIVE
        
	bool initialized, complete = false;

	//WHILE FILE IS NOT FULLY TRANSFERRED. 
	while(!complete) {
		// Receive message from socket.
		bzero(buffer, 1000);
		read(sock, buffer, 1000);
		printf("\nReceived new message! Message: %s\n", buffer);

		data = malloc(984); // allocate space for data

		// first check if message is corrupted
		if(strlen(buffer) < 16) {
			printf("-----------\nReceived corrupt packet! Discarding...\n-----------\n");
			continue;
		}

		// parse message
		seg = charToSeg(buffer);
		printf("After return data: %s\n", seg->data);

		// copy data to allocated buffer
		memcpy(data, seg->data, seg->len);
		// null terminate for last segment
		data[seg->len] = '\0';

		printf("After memcpy to data: %s\n", data);

		//FIRST SEGMENT RECEIVED
		if (!initialized) {
			printf("\nInitializing!\n");
			// set the total file size on first segment receive
			printf("Filesize: %d\n", seg->fileSize);
			fileSize = seg->fileSize;
			allData = malloc(fileSize); // allocate space for the whole data

			printf("Total final size: %d\n", fileSize);

			// next, find the total number of segments expected
			totalSegmentCount = fileSize / MAX_PAYLOAD;
			if ( fileSize % MAX_PAYLOAD > 0 ){
				totalSegmentCount += 1;	//adds another segment for an incomplete payload
			}

			printf("Total number of segments (max %d bytes each): %d\n", MAX_PAYLOAD, totalSegmentCount);

			// set initialized bool to true
			initialized = true;
		}

		//RECORD FIELDS OF THE RECEIVED SEGMENT
		seq = seg->seq;

		// UPDATE CWND
		received[seq] = true;
		if(seq == nextExpected) {
			printf("------\n");
			// SHIFT IF IN ORDER SEGMENT
			while(received[nextExpected] == true) {
				// shift if we see a true
				nextExpected++;

				printf("Shifting cwnd, next expected segment is %d\n", nextExpected);

				// if we reach the last segment, we're done
				if(nextExpected == totalSegmentCount) {

					printf("Reached end of file! Last segment received.\n");

					complete = true;
					break;
				}
			}
		}
		else {
			printf("------\nReceived out of order segment. Saving...\n");
		}

		// WRITE TO FILE
		pos = seg->seq * MAX_PAYLOAD;

		printf("Saving data to file: %s\n", data);
		printf("Writing %d bytes to %d * %d = %d\n", seg->len, seg->seq, MAX_PAYLOAD, pos);

		memcpy(allData + pos, data, seg->len);

		// Send ACK for the segment
		char seqstr[4];
		sprintf(seqstr, "%d", seg->seq);

		// Decide if to corrupt / lose segment

		double r = (rand() % 100) * 1.0 / 100.0;
		printf("\n\n%f\n\n", r);

		if(r < (1.0 - ploss - pcorr)) {
			int n = sendto(sock, seqstr, strlen(seqstr), 0, (struct sockaddr_in *) &serv_addr, sizeof(serv_addr)); //write to the socket
			if (n < 0)
				error("ERROR writing to socket");
			}
		else if(r > (1 - pcorr)) {
			char corruptPacket[4];
			sprintf(corruptPacket, "%d", -1);
			int n = sendto(sock, corruptPacket, strlen(corruptPacket), 0, (struct sockaddr_in *) &serv_addr, sizeof(serv_addr)); //write to the socket
			if (n < 0)
				error("ERROR writing to socket");
			printf("-----------\nPacket corrupted! Sending corrupted packet...\n-----------\n");
		}
		else {
			printf("-----------\nPacket lost...\n-----------\n");
		}

		free(seg);
		free(data);
	}

	fwrite(allData, 1, fileSize, fp);
	free(allData);
	fclose(fp);

	sendto(sock, "done", strlen("done"), 0, (struct sockaddr_in *) &serv_addr, sizeof(serv_addr)); //write to the socket
}
