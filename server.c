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
void process_message(uint8_t *dataBuffer);
void processMsgFlagFromClient(int socketNum, uint8_t *packet, int messageLen);

//Handle Table Functions
int add_handle(int socketNum, char* input_handle, int handle_len);
int getSocketNumber(uint8_t *handle);

//Dynamic Memory for Handle Table 
typedef struct{
	char *handle;
	int socketNum; 
}handle;

size_t handle_table_count = 0; 
handle* handle_table = NULL; 

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

void processClient(int clientSocket){
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the client_socket
	if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) < 0){
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0){
		//Process flag from the client. 
		processMsgFlagFromClient(clientSocket, dataBuffer, messageLen);

		// printf("Message received on socket %d, length: %d Data: %s\n",clientSocket, messageLen, dataBuffer);
		// //Echo Client
		// int sent = 0;
		// sent =  sendPDU(clientSocket, dataBuffer, messageLen);
		// if (sent < 0){
		// perror("send call");
		// exit(-1);
		// }

		// printf("Amount of data sent is: %d\n", sent);
	}else{
		close(clientSocket);
		removeFromPollSet(clientSocket);
		printf("Connection closed by other side\n");
	}
}

void processInitFromClient(int socketNum, uint8_t *packet, int messageLen){
	//In this function I am going to parse the function and grab the handle 
	int handle_len = packet[1];
	char handle[handle_len]; 

	//Grab handle and add null terminator
	memcpy(handle, &packet[2],handle_len);
	handle[handle_len] = '\0';
	//Add to handle table
	int output_flag = add_handle(socketNum, handle,handle_len);

	//Check return of add_handle and respond to client 
	if(output_flag == 1){ //server -> client confirmation of good handle 
		uint8_t buffer[1];
		buffer[0] = FLAG_INITIALIZE_HANDLE_CONFIMATION;
		sendPDU(socketNum, buffer, 1); 
	}else if(output_flag == 0){ //server -> Error in initial packet 
		uint8_t buffer[1];
		buffer[0] = FLAG_INITIALIZE_HANDLE_ERROR;
		sendPDU(socketNum, buffer, 1); 
	}else{
		printf("Unexpected Result from Add"); 
		exit(-1); 
	}

	printf("\n");
	printf("Current Handle Table\n");
	for(int i = 0; i < handle_table_count; i++){
		printf("Handle: %s \n", handle_table[i].handle); 
	}	
}

/*This function will send the message packet to the correct destHandler*/
void processMsgPacket(int socketNum, uint8_t *packet, int messageLen){
	//First we want to grab the handler
	//Secondly we will then check our handler_table to find the correct socket_num 

	//Parse input packet
	int handle_len = packet[1]; //Grab Handle Length

	//Grab handle
	uint8_t destHandle[handle_len + 1]; //+1 for null terminator
	memcpy(destHandle, &packet[2], handle_len); 
	destHandle[handle_len] = '\0';

	//Search for socket number with handle name
	int destSocketNumber = getSocketNumber(destHandle);

	if(destSocketNumber == -1){
		printf("Error: Handle does not exist"); 
		return; 
	}


	int sent = sendPDU(destSocketNumber, packet, messageLen);
	if (sent < 0){
		perror("send call");
		exit(-1);
	}

}

void processMultiSendPacket(int socketNum, uint8_t *packet, int messageLen){
	//Need to grab all of the handle names 
}

void processMsgFlagFromClient(int socketNum, uint8_t *packet, int messageLen){
	//Check the incoming message 
	uint8_t flag = packet[0];
	printf("\n");
	printf("Socket: %d, Packet Flag: %d, Message Len: %d \n",socketNum, flag, messageLen);
		switch(flag){
		case(FLAG_CLIENT_INITIALIZE_HANDLE):
			processInitFromClient(socketNum, packet, messageLen); 
			break; 
		case(FLAG_MESSAGE): 
			processMsgPacket(socketNum, packet, messageLen); 
			break;
		default: 
			break; 
	}	
}

/**add_handle() Returns 1 when handle is succesfully added. Returns 0 when handle
Could not be added. **/
int add_handle(int socketNum, char* input_handle, int handle_len){
	//Check if the Handle Table is empty, initialization. 
	if(handle_table == NULL){
		handle_table = (handle*)malloc(sizeof(handle));
		if(handle_table ==  NULL){
			perror("Malloc Failure");
			exit(-1);
		}
	}else{ //Expand memory to add new entry. 
		
		//Check for duplicates 
		for(int i = 0; i < handle_table_count; i++){
			if(strcmp(input_handle,handle_table[i].handle) == 0){
				printf("input handle: %s, table handle: %s\n", input_handle, handle_table[i].handle);
				printf("Error Duplicate\n");
				//Send error flag to client 
				return 0; 
				}
		}

		handle *temp = (handle*)realloc(handle_table, (handle_table_count + 1)*sizeof(handle));
		if(temp == 0){
			perror("Realloc Failed");
			exit(-1); 
		}
		handle_table = temp; 
	}

	//Add new entries into handle table
	handle_table[handle_table_count].handle = strdup(input_handle); 
	handle_table[handle_table_count].socketNum = socketNum;
	handle_table_count++; 
	printf("handle table count: %ld\n", handle_table_count); 
	return 1; 
}


/*This function takes in a handle name and returns the socket number.
Returns -1 if handle name cannot be found in the table*/
int getSocketNumber(uint8_t *handle){
	//Iterate through handle table to see if socket number exist. 
	for(int i = 0; i < handle_table_count; i++){
		if(strcmp((char*)handle, handle_table[i].handle) == 0){
			return handle_table[i].socketNum;
		}
	}
	return -1; 
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2){
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2){
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

