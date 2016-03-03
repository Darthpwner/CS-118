#include <stdbool.h>

typedef struct sequence{
	bool* value;
	int cwnd;
	int start;
	int end;
}* sequence_t;



sequence_t makeSequence(int cwnd, int size);
	//FIXME 4. NEED TO IMPLEMENT

int advanceWindow(sequence_t seq);		//advances the window if possible
	//FIXME 5. NEED TO IMPLEMENT

