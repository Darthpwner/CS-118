/* Struct Data Structure for Packets
   - type: indicate type of packet
   		0 - request
   		1 - ACK
   		2 - data
   		3 - teardown
   - sequence_no: denotes the current packet's sequence number
   - length: length of the data
   - data: the actual data  */

#define DATA_SIZE 1024

struct packet {
	int type;
	int sequence_no;
	int length;
	char data[DATA_SIZE];
};