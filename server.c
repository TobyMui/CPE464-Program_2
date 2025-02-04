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
void processMultiSendErrorPacket(int socketNum,uint8_t *handle);

//Handle Table Functions
int add_handle(int socketNum, char* input_handle, int handle_len);
int getSocketNumber(uint8_t *handle);
int remove_handle(int clientSocket); 

//Dynamic Memory for Handle Table 
typedef struct{
	char *handle;
	int socketNum; 
}handle;

uint32_t handle_table_count = 0; 
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
	}else{
		remove_handle(clientSocket); 
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
	
	int packet_index = 1; //Start at handle length byte

	//Parse input packet
	int srcHandle_len = packet[packet_index++]; //Grab Handle Length
	packet_index += srcHandle_len; 

	//Grab Dest Handle Len
	packet_index++; //Increment over num handle
	int destHandle_len = packet[packet_index++]; 

	uint8_t destHandle[100]; 
	memcpy(destHandle, &packet[packet_index],destHandle_len); 
	destHandle[destHandle_len] = '\0'; 

	//Search for socket number with handle name
	int destSocketNumber = getSocketNumber(destHandle);

	if(destSocketNumber == -1){
		printf("Error: Handle does not exist"); 
		processMultiSendErrorPacket(socketNum,destHandle);
		return; 
	}
	
	int sent = sendPDU(destSocketNumber, packet, messageLen);
	if (sent < 0){
		perror("send call");
		exit(-1);
	}

}
/*This function sends an error to the client if there is an error with the destination handle*/
void processMultiSendErrorPacket(int socketNum, uint8_t *destHandle){
	//Init for packet building 
	uint8_t packet[MAXBUF];
	int packet_len = 0;  

	//Add flag 
	packet[packet_len++] = FLAG_UNKNOWN_HANDLE;

	//Add handle_len 
	int handle_len = strlen((char*)destHandle);
	packet[packet_len++] = handle_len;
	
	
	//Add handle
	memcpy(&packet[packet_len], destHandle, handle_len); 	
	packet_len += handle_len; 

	printf("Error packet length: %d\n", packet_len );


	//Send packet to client
	int sent = sendPDU(socketNum, packet, packet_len + 1); //+1 for the null character
	if (sent < 0){
	perror("send call");
	exit(-1);
	}
}

void processMultiSendPacket(int socketNum, uint8_t *packet, int messageLen){
	//Need to grab all of the handle names 	
	//Grab Source Handle
	int packet_index = 1; //Start index at handle length 
	int handle_len = packet[packet_index++]; 

	//Grab Source Length 
	char srcHandle[packet_index + 1]; //Add 1 for null terminator
	memcpy(srcHandle, &packet[packet_index], handle_len); 
	srcHandle[handle_len] = '\0';
	packet_index += handle_len; 

	//Grab number of handles 
	int num_Handles = packet[packet_index++];
	printf("Number of handles: %d \n", num_Handles); 

	//Create temp buffer to store handles
	uint8_t handle[100];
	memset(handle,'\0', sizeof(handle));

	printf("current index: %d\n", packet_index);

	for(int i = 0; i < num_Handles;i++){
		handle_len = packet[packet_index]; //Get handle length
		memcpy(handle, &packet[packet_index + 1], handle_len); //Copy handle into buffer
		packet_index += handle_len + 1;  //increment packet index, handle_len + length byte
		printf("packet_index: %d String: %s \n", packet_index, handle);
		printf("Message size: %d\n", messageLen);

		// Search if handle exist in handle_table and send 
		// Search for socket number with handle name
		int destSocketNumber = getSocketNumber((uint8_t*)handle);
		if(destSocketNumber != -1){
			int sent = sendPDU(destSocketNumber, packet, messageLen);
			if (sent < 0){
			perror("send call");
			exit(-1);
			}
		}else{
			processMultiSendErrorPacket(socketNum,handle);
		}
		memset(handle,'\0', sizeof(handle)); //Reset buffer
	}
}

/*This function processes a broadcast request from the client*/
void processBroadCastPacket(int socketNum, uint8_t *input_packet, int messageLen){
	for(int i = 0; i < handle_table_count; i++){
		if(handle_table[i].socketNum != socketNum){
			int sent = sendPDU(handle_table[i].socketNum, input_packet, messageLen);
			if (sent < 0){
			perror("send call");
			exit(-1);
		}
		}
	} 
}


/*This function processes the List Handle Table Request */
void processListRequestPacket(int socketNum){
	//Part 1, Flag 11, Send length of handle table. 
	uint32_t length = htonl(handle_table_count);
	uint8_t handle_len_packet[5]; 
	handle_len_packet[0] = FLAG_REQUEST_HANDLE_LIST_ACK;
	memcpy(&handle_len_packet[1], &length, 4); 

	int sent1 = sendPDU(socketNum, handle_len_packet, sizeof(handle_len_packet));
			if (sent1 < 0){
			perror("send call");
			exit(-1);
	}
	printf("sent1: %d\n", sent1); 
	

	//Part 2, Flag 12, Send handles
	uint8_t send_buffer[100]; 
	int handle_len = 0; 
	memset(send_buffer, '\0', sizeof(send_buffer)); 
	for(int i = 0 ; i < handle_table_count;i++){
		//Add flag 
		send_buffer[0] = FLAG_SENDING_HANDLES; 
		
		//Add handle length
		handle_len = strlen(handle_table[i].handle);
		send_buffer[1] = handle_len;

		//Add handle
		memcpy(&send_buffer[2], handle_table[i].handle, handle_len + 1); 

		//Send Packet 
		int sent2 = sendPDU(socketNum, send_buffer, handle_len + 2);
			if (sent2 < 0){
			perror("send call");
			exit(-1);
		}

		//reset buffer
		memset(send_buffer, '\0', sizeof(send_buffer)); 
	}

	//Part 3: Flag 13
	uint8_t finished_packet[1];
	finished_packet[0] = FLAG_LIST_FINISHED; 
	int sent3 = sendPDU(socketNum, finished_packet, 1);
			if (sent3 < 0){
			perror("send call");
			exit(-1);
	}
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
		case(FLAG_MULTICAST):
			processMultiSendPacket(socketNum, packet, messageLen);
			break; 
		case(FLAG_REQUEST_HANDLE_LIST):
			printf("Request Handle List\n"); 
			processListRequestPacket(socketNum);
			break; 
		case(FLAG_BROADCAST):
			processBroadCastPacket(socketNum, packet,messageLen); 
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
	printf("handle table count: %u\n", handle_table_count); 
	return 1; 
}

/* This function takes in a handle name and removes it
function ruturns 1 on success and 0 on failure.*/
int remove_handle(int socketNum){
	//Make sure that the function is called wtih an empty table
	if(handle_table == NULL || handle_table_count == 0 ){
		printf("Handle table is emtpy. \n");
		return 0; 
	}

	int index = -1; //Variable for storing index of handle to remove

	//Find index of the handle  
	for(int i = 0; i < handle_table_count; i++){
		if(socketNum == handle_table[i].socketNum){
			index = i;
			break; 
		}
	}	

	//Exit function and throw error if handle isn't found 
	if(index == -1){
		printf("Socket %d not found\n",socketNum ); 
		return 0; 
	}

	free(handle_table[index].handle);
	
	//Shift handle table to fill removed handle 
	for(int i = index; i < handle_table_count - 1; i++){
		handle_table[i] = handle_table[i+1]; 
	}

	handle_table_count--;

	//Reallocate Memory
	if(handle_table_count == 0){
		free(handle_table);
		handle_table = NULL; 
	}else{
		handle *temp = (handle*)realloc(handle_table, handle_table_count * sizeof(handle));
		if(temp == NULL){
			perror("Realloc failed");
			exit(-1); 
		}
		handle_table = temp; 
	}
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

