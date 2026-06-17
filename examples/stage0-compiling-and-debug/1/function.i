# 1 "function.c"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "function.c"
# 1 "function.h" 1
       
# 10 "function.h"
float add_and_multiply(float x, float y);
# 2 "function.c" 2
int nCompletionStatus = 0;
float add(float x, float y)
{
 float z = x + y;
 return z;
}

float add_and_multiply(float x, float y)
{
 float z = add(x, y);
 z *= (3.0);
 return z;
}
