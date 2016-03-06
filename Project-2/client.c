/*
 A simple client in the internet domain using UDP
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
 #define MAX_PAYLOAD_CONTENT 984

void error(char *msg)
{
    perror(msg);
    exit(1);
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
    p_t -> data = malloc(sizeof(char) * MAX_PAYLOAD_CONTENT);

    p_t -> sequence_no = atoi(seqstr);
    p_t -> length = atoi(lenstr);
    p_t -> data_size = fsize;
    int i;
    for(i = 0; i < p_t -> length; i++) {
        p_t -> data[i] = ptr[i + 16];
        printf("p_t -> data[%d] = %c\n", i, p_t -> data[i]);
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

    //PROBLEM HERE
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

        data = malloc(MAX_PAYLOAD_CONTENT);

        //Parse the given message
        p_t = charToSeg(buffer);    //CHECK THIS LINE!
        printf("After return data: %s\n", p_t -> data);

        //Copy data to allocated buffer
        memcpy(data, p_t -> data, p_t -> length);

        //null terminate for the last segment
        data[p_t -> length] = '\0';

        //First segment received
        if(!init) {
            printf("\nInitializing!\n");
            //Set the total file size on first segment receive
            printf("Filesize: %d\n", p_t -> data_size);
            fileSize = p_t -> data_size;
            allData = malloc(fileSize);   //allocate space for the whole data

            printf("Total final size: %d\n", fileSize);

            //Next, find the total # of segments expected
            totalSegmentCount = fileSize / MAX_PAYLOAD_CONTENT;    //Change the 1000 value later to a variable
            if(fileSize % MAX_PAYLOAD_CONTENT > 0) {
                totalSegmentCount++;    //Add another segment for an incomplete payload
            }

            printf("Total # of segments (max %d bytes each): %d\n", MAX_PAYLOAD_CONTENT, totalSegmentCount);

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
        pos = p_t -> sequence_no * MAX_PAYLOAD_CONTENT;

        printf("Saving data to file: %s\n", data);
        printf("Writing %d bytes to %d * %d = %d\n", p_t -> length, p_t -> sequence_no, MAX_PAYLOAD_CONTENT, pos);

        memcpy(allData + pos, data, p_t -> length);

        //Send ACK for the segment
        char seqstr[4];
        sprintf(seqstr, "%d", p_t -> sequence_no);

        // free(p_t);  //This is line is fucked up
        // free(data);   //This is line is fucked up
    }
    fwrite(allData, 1, fileSize, fp);

    free(p_t);
    free(data);
    // free(allData);
    // fclose(fp);

    sendto(sock, "done", strlen("done"), 0, (struct sockaddr_in *) &serv_addr, sizeof(serv_addr));  //Write to the socket
}

int main(int argc, char *argv[]) {

	// test();	//Testing purposes

	//
	int sockfd; //Socket descriptor
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address

    double packet_loss, packet_corruption;

    char* filename;

    //char* tail;

    socklen_t slen = sizeof(serv_addr);

    char buffer[256];
    if (argc < 4) {	//Change this to 6 after we incorporate the error handling
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //create a new socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    printf("sockfd: %i\n", sockfd);

    server = gethostbyname(argv[1]); //takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    //Assign the file name passed in through the command line
    filename = argv[3];

    //Pass in arguments for packet loss and packet corrupted

    memset((char *) &serv_addr, 0, sizeof(serv_addr));  //WHY THE FUCK IS THERE AN ERROR HERE?
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);    //THIS LINE HAS A PROBLEM TOO?
    serv_addr.sin_port = htons(portno);

    printf("TEST RECEIVER ACTION\n");
    printf("sockfd: %i\n", sockfd);
    printf("serv_addr: %i\n", serv_addr);   
    printf("filename: %s\n", filename);

    receiverAction(sockfd, serv_addr, filename, packet_loss, packet_corruption);

    close(sockfd);
    return 0;
}