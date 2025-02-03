#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>
#include <errno.h>

#define FLAG_CLIENT_INITIALIZE_HANDLE 1
#define FLAG_INITIALIZE_HANDLE_CONFIMATION 2
#define FLAG_INITIALIZE_HANDLE_ERROR 3
#define FLAG_BROADCAST 4
#define FLAG_MESSAGE 5
#define FLAG_MULTICAST 6
#define FLAG_UNKNOWN_HANDLE 7
#define FLAG_REQUEST_HANDLE_LIST 10
#define FLAG_REQUEST_HANDLE_LIST_ACK 11
#define FLAG_SENDING_HANDLES 12
#define FLAG_LIST_FINISHED 13


#define MAX_MESSAGE_LEN 200
#define MAX_HANDLE_LEN 100

int sendPDU(int socketNumber, uint8_t * dataBuffer, int lengthOfData);

int recvPDU(int clientSocket, uint8_t * dataBuffer, int bufferSize);