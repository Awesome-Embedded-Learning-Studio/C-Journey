#include "function.h"
extern int nCompletionStatus;
int main()
{
	float x = 1.0;
	float y = 0.5;
	float z;

	z = add_and_multiply(x, y);
	nCompletionStatus = 1;
	return 0;
}
