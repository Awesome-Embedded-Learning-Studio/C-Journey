// src
//
#include <stdio.h>
int printMsg(const char* msg){
	printf("%s\n", msg);
}

int add(int a, int b){
	return a + b;
}

int minus(int a, int b){
	return a - b;
}

int multiply(int a, int b){
	return a * b;
}

int division(int a, int b){
	if(b == 0)
	{
		printMsg("b can not be 0");
		return -1;
	}

	return a / b;
}
