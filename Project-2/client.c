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
#include "packet.c"

#define MAX_PAYLOAD_CONTENT 984
#define MAX_SEQUENCE_NUMBER_SIZE 30000  

//charToSeg constants
#define SEQUENCE_NO_STR_SIZE 5
#define LENGTH_STR_SIZE 5
#define DATA_SIZE_STR_SIZE 9
#define HEADER_SIZE 16

//receiverAction constants
#define ONE_KB 1000

//corrupted packet constants
 // #define CORRUPTED_PACKET_SIZE 4
static const int CORRUPTED_PACKET_SIZE = 4;

void error(char *msg) {
    perror(msg);
    exit(1);
}

packet_t charToSeg(char* c) {
    packet_t p_t = malloc(sizeof(packet));
    
    //Header size will be 16 bytes (4 sequence #, 4 length, 8 data size; in that order)
    char sequence_no_str[SEQUENCE_NO_STR_SIZE];
    char length_str[LENGTH_STR_SIZE];
    char data_size_str[DATA_SIZE_STR_SIZE];
    char* ptr = c;

    //Sequence # starts off the header
    strncpy(sequence_no_str, ptr, SEQUENCE_NO_STR_SIZE - 1);
    sequence_no_str[SEQUENCE_NO_STR_SIZE - 1] = '\0';

    //Offset the length bytes by 4
    strncpy(length_str, ptr + (SEQUENCE_NO_STR_SIZE - 1), LENGTH_STR_SIZE - 1);
    length_str[LENGTH_STR_SIZE - 1] = '\0';

    //Offset the data size bytes by 8
    strncpy(data_size_str, ptr + DATA_SIZE_STR_SIZE - 1, DATA_SIZE_STR_SIZE - 1);
    data_size_str[DATA_SIZE_STR_SIZE - 1] = '\0';

    int data_size = atoi(data_size_str); //Converts char to int
    p_t -> data = malloc(sizeof(char) * MAX_PAYLOAD_CONTENT);

    p_t -> sequence_no = atoi(sequence_no_str);

    //Reset the sequence_no if it is bigger than 30,000 kBytes
    if(p_t -> sequence_no / MAX_SEQUENCE_NUMBER_SIZE > 0) {
        p_t -> sequence_no %= MAX_SEQUENCE_NUMBER_SIZE;
    }
    
    p_t -> length = atoi(length_str);
    p_t -> data_size = data_size;

    int i;
    for(i = 0; i < p_t -> length; i++) {
        p_t -> data[i] = ptr[i + 16];
        // printf("p_t -> data[%d] = %c\n", i, p_t -> data[i]);
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
	int data_size = 0;

    packet_t p_t;   //Creates a packet

    char* data;
    char buffer[ONE_KB];
    int total_packet_count;

    char* temp;
    int sequence_no;

    int received[6000]; //Acts a boolean array (0 - false, 1 - true)
    int next_expected_packet = 0;

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

        //Decide if to corrupt or lose segment
        double r = (rand() % 100) * 1.0/100.0;    
        printf("\n\nRandomly Generated r: %f\n\n", r);
        if(r >= (1.0 - pL - pC) || r > (1 - pC)) {
            printf("\nLOSS: nothing is sent back to server\n");
            continue;
        }
        else {
            //Receive message from socket
            memset(buffer, 0, ONE_KB);
            read(sock, buffer, ONE_KB);
            printf("\nReceived a new message! Message: \n%s\n", buffer);
        }

        temp = malloc(MAX_PAYLOAD_CONTENT);

        //Check if message is corrupted
        if(strlen(buffer) < 16) {
            printf("-------\nReceived corrupted packet! Discarding...\n-------\n");
            continue;
        }

        //Parse the given message
        p_t = charToSeg(buffer);    //CHECK THIS LINE!
        printf("After return temp: %s\n", p_t -> data);

        //Copy data to allocated buffer
        memcpy(temp, p_t -> data, p_t -> length);

        //null terminate for the last segment
        temp[p_t -> length] = '\0';

        //First segment received
        if(!init) {
            printf("\nInitializing!\n");
            //Set the total file size on first segment receive
            printf("data_size: %d\n", p_t -> data_size);
            data_size = p_t -> data_size;
            data = malloc(data_size);   //allocate space for the whole data

            printf("Total final size: %d\n", data_size);

            //Next, find the total # of segments expected
            total_packet_count = data_size / MAX_PAYLOAD_CONTENT;    //Change the 1000 value later to a variable
            if(data_size % MAX_PAYLOAD_CONTENT > 0) {
                total_packet_count++;    //Add another segment for an incomplete payload
            }

            printf("Total # of segments (max %d bytes each): %d\n", MAX_PAYLOAD_CONTENT, total_packet_count);

            //Set initailized bool to true
            init = 1;
        }

        //Record fields of the received segment
        sequence_no = p_t -> sequence_no;

        //Update CWnd
        received[sequence_no] = 1;
        if(sequence_no == next_expected_packet) {
            printf("-----\n");
            //Shift if in order segment
            while(received[next_expected_packet]) {
                //Shift if we see a true value
                next_expected_packet++;

                printf("Shifting cwnd, next expected packet is %d\n", next_expected_packet);

                //If we reach the last segment, we have finished
                if(next_expected_packet == total_packet_count) {
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

        printf("Saving temp to file: %s\n", temp);
        printf("Writing %d bytes to %d * %d = %d\n", p_t -> length, p_t -> sequence_no, MAX_PAYLOAD_CONTENT, pos);

        memcpy(data + pos, temp, p_t -> length);

        //Send ACK for the segment
        char seqstr[4];
        sprintf(seqstr, "%d", p_t -> sequence_no);

        if(r < (1.0 - pL - pC)) {
            int n = sendto(sock, seqstr, strlen(seqstr), 0, (struct sockaddr_in *) &serv_addr, sizeof(serv_addr)); //write to the socket
            if (n < 0) {
                error("ERROR writing to socket");
            }
        } else if (r > (1 - pC)) {
            char corruptedPacket[CORRUPTED_PACKET_SIZE];
            sprintf(corruptedPacket, "%d", -1);
            int n = sendto(sock, corruptedPacket, strlen(corruptedPacket), 0, (struct sockaddr_in *) &serv_addr, sizeof(serv_addr));    //write to the socket
            if(n < 0) {
                error("ERROR writing to socket");
            }

            printf("-----------\nPacket corrupted! Sending corrupted packet...\n-----------\n");
        } else {
            printf("-----------\nPacket lost...\n-----------\n");
        }
    }

    fwrite(data, 1, data_size, fp);

    // free(p_t);
    // free(temp);

    sendto(sock, "done", strlen("done"), 0, (struct sockaddr_in *) &serv_addr, sizeof(serv_addr));  //Write to the socket
    printf("\n-----------------------\nClient received all data, sending 'done' message to server\n-----------------------\n");
    //free(data);
    //fclose(fp);
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

    char* tail; //CHANGE THIS LATER

    socklen_t slen = sizeof(serv_addr);

    char buffer[256];
    if (argc < 6) {	//Change this to 6 after we incorporate the error handling
       fprintf(stderr,"usage %s hostname port filename packet_loss packet_corruption\n", argv[0]);
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
    
    //Assign the file name passed in through the command line
    filename = argv[3];

    //Pass in arguments for packet loss and packet corrupted
    packet_loss = strtod(argv[4], &tail);
    packet_corruption = strtod(argv[5], &tail);

    memset((char *) &serv_addr, 0, sizeof(serv_addr));  //WHY THE FUCK IS THERE AN ERROR HERE?
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);    //THIS LINE HAS A PROBLEM TOO?
    serv_addr.sin_port = htons(portno);

    receiverAction(sockfd, serv_addr, filename, packet_loss, packet_corruption);

    close(sockfd);
    return 0;
}