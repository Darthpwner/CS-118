#include <stdbool.h>

typedef struct segment{
	int seq;
	int len;
	int fileSize;
	char* data;
	//char data[10];
} segment, *segment_t;



segment_t makeSegment(int seq, int len, int fileSize, char* data);	//allocates memory for segment and constructs it
	//FIXME 1. NEED TO IMPLEMENT

char* segToChar(segment_t seg);	//takes in seg, returns char representation
	//FIXME 2. NEED TO IMPLEMENT

segment_t charToSeg(char* c);	//takes in char* and returns a segment_t
	//FIXME 3. NEED TO IMPLEMENT
