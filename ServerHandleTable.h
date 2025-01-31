#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 

typedef struct{
    char* handle; 
    int socket;
    struct Node *next; 
}Node;


//Returns 0 when false, 1 when true 
int add( );
int lookup();
int remove();
void free(); 




