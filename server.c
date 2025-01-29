/******************************************************************************
* myServer.c
* 
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

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

#include "networks.h"
#include "safeUtil.h"
#include "socket_communication.h"
#include "pollLib.h"


#define MAXBUF 1024
#define DEBUG_FLAG 1

void recvFromClient(int clientSocket);
void addNewSocket(int mainSocket);
void processClient(int socket);
void serverControl(int mainSocket);
int checkArgs(int argc, char *argv[]);


int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);
	
	//Server control
	serverControl(mainServerSocket);

	//Close Main Socket
	close(mainServerSocket);
	return 0;
}


void serverControl(int mainSocket){
	//Initialize Poll
	setupPollSet();

	//Add main to poll 
	addToPollSet(mainSocket);

	while(1){
	int current_socket = pollCall(-1);
	if(current_socket == mainSocket){
		addNewSocket(current_socket);
	}else if(current_socket < 0){
		perror("fail");
		return;
	}else{
		processClient(current_socket);
	}
	}
}

void addNewSocket(int mainSocket){
	int clientSocket = tcpAccept(mainSocket, DEBUG_FLAG);
	addToPollSet(clientSocket);
}

void processClient(int socket){
	recvFromClient(socket);
}

void recvFromClient(int clientSocket)
{
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the client_socket
	if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0){
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		printf("Message received on socket %d, length: %d Data: %s\n",clientSocket, messageLen, dataBuffer);
		//Echo Client
		int sent = 0;
		sent =  sendPDU(clientSocket, dataBuffer, messageLen);
		if (sent < 0){
		perror("send call");
		exit(-1);
		}

	printf("Amount of data sent is: %d\n", sent);
	}else{
		close(clientSocket);
		removeFromPollSet(clientSocket);
		printf("Connection closed by other side\n");
	}
	
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

