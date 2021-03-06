/* Struct Data Structure for Packets
   - type: indicate type of packet
   		0 - request
   		1 - ACK
   		2 - data
   		3 - teardown
   - sequence_no: denotes the current packet's sequence number
   - length: length of the data
   - data: the actual data  */

#include <stdbool.h>

#define DATA_SIZE 1024
#define MAX_SEQUENCE_NUMBER_SIZE 2400000  //30,000 bits * 8

typedef struct {
	int type;
	int sequence_no;
	int length;
    int data_size;
	char* data;
	time_t timer;
} packet, *packet_t;

packet_t charToSeg(char* c);  //Takes in char* and returns a packet_t