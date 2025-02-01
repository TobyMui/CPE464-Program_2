/******************************************************************************
* myClient.c
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
#include "pollLib.h"
#include <string.h>

#include "networks.h"
#include "safeUtil.h"
#include "socket_communication.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1

void sendToServer(int socketNum);
int readFromStdin(uint8_t * buffer);
void clientControl(int socketNum , uint8_t* clientHandle);
void processStdin(int socketNum, uint8_t *clientHandle);
void processMsgFromServer(int active_socket, uint8_t*clientHandle);
void checkArgs(int argc, char * argv[]);
void processFlagFromServer(int socketNum, uint8_t *packet, int messageLen, uint8_t * clientHandle);

//Send Functions
void sendHandleToServer(int socketNum, uint8_t *handle); 
void send_client_message_packet(int socketNum, uint8_t *input_buffer, int inputMessageLen, uint8_t *clientHandle);

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	checkArgs(argc, argv);     //Check arguments

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	sendHandleToServer(socketNum, (uint8_t*)argv[1]);
	clientControl( socketNum , (uint8_t*)argv[1]);

	close(socketNum);
	return 0;
}

void clientControl(int socketNum ,uint8_t* clientHandle){
	//Initialize Poll
	setupPollSet();
	
	//Add to pollset
	addToPollSet(socketNum);
	addToPollSet(STDIN_FILENO);

	printf("Enter data:");
	fflush(stdout);

	while(1){
		int active_socket = pollCall(-1);
		if(active_socket == socketNum){
			processMsgFromServer(active_socket, clientHandle);
		}else if(active_socket == STDIN_FILENO){
			processStdin(socketNum,clientHandle); 
		}else{
			printf("Invalid Socket Number");
		}
	}
}

//////////////////////////////////////////Receiving from Server Functions//////////////////////////////////////////////

//This Function calls recvPDU then calls process_message_flag
void processMsgFromServer(int socket, uint8_t *clientHandle){
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the client_socket
	if ((messageLen = recvPDU(socket, dataBuffer, MAXBUF)) < 0){
		perror("recv call");
		exit(-1);
	}if (messageLen > 0){
		processFlagFromServer(socket,dataBuffer, messageLen, clientHandle); 
		fflush(stdout);
	}else{
		close(socket);
		removeFromPollSet(socket);
		printf("\n");
		printf("Connection closed by other side\n");
		exit(-1);
	}
} 

void printMsgFromServer(uint8_t *input_packet, int packet_len){
	//Grab Source Handle
	int header_len = input_packet[1];

	//Grab Source Length 
	char srcHandle[header_len + 1]; //Add 1 for null terminator
	memcpy(srcHandle, &input_packet[2], header_len); 
	srcHandle[header_len] = '\0';

	//Grab Dest Handle Length
	int destHeader_len = input_packet[header_len + 3];//Add 3 to skip flags
	printf("destHeader_len: %d", destHeader_len); 


	//Grab Message
	int message_index = header_len + 4 + destHeader_len; //Add 4 because flag and header len
	int message_len = packet_len - message_index;  
	char message_buffer[MAXBUF];
	memcpy(message_buffer, &input_packet[message_index],message_len); 
	
	printf("\n");
	printf("%s: %s\n",srcHandle, message_buffer); 
}

//Function for printing multicast from server
void printMultiCastFromServer(uint8_t *input_packet, int packet_len){
	printf("print multicast packet\n");
	printf("packet len: %d\n", packet_len); 

	//Grab Source Handle
	int packet_index = 1; //Start index at handle length 
	int handle_len = input_packet[packet_index++]; //Get handle len and increment index

	//Grab Source Length 
	char srcHandle[handle_len + 1]; //Add 1 for null terminator
	memcpy(srcHandle, &input_packet[packet_index], handle_len); 
	srcHandle[handle_len] = '\0';
	packet_index += handle_len; 
	printf("packet_index: %d, \n", packet_index); 

	//Grab number of handles 
	int numHandles = input_packet[packet_index++];



	//Iterate over the handles and increment packet index
	for(int i = 0; i < numHandles;i++){
		packet_index += input_packet[packet_index]; 
		packet_index++; 
		printf("packet_index: %d, \n", packet_index); 
	}
	int message_len = packet_len - packet_index; 
	printf("packet len: %d\n", packet_len); 

	char buffer[MAXBUF];
	memset(buffer, '\0', sizeof(buffer));
	memcpy(buffer, &input_packet[packet_index], 6); 
	printf("%s: %s\n", srcHandle, buffer);
}

/*In this function we take the incoming message and process the flag that
was sent by the server. This function will then call other functions
depending on the flag. */
void processFlagFromServer(int socketNum, uint8_t *packet, int messageLen, uint8_t * clientHandle){
	uint8_t flag = packet[0]; 
	
	switch(flag){
		case(FLAG_INITIALIZE_HANDLE_CONFIMATION):
			printf("Good Initial handle\n"); 
			break; 
		case(FLAG_INITIALIZE_HANDLE_ERROR): 
			printf("Error on initial packet, please check your handle name\n");
			exit(-1); 
			break; 
		case(FLAG_MESSAGE):
			printMsgFromServer(packet, messageLen);
			break;
		case(FLAG_MULTICAST):
			printMultiCastFromServer(packet,messageLen);
			break;
		default: 
			printf("Error: unknown flag in process_message_flag");
			exit(-1); 
	}
}



//////////////////////////////////////////Send to Server Functions//////////////////////////////////////////////////////////////

// Implementation of %M 
//I want to output the buffer in this format. 3 byte chat header, 1 byte containing the length of sending clients handle 
//We must extract the handle, handle length, and the message. 
void send_client_message_packet(int socketNum, uint8_t *input_buffer, int inputMessageLen, uint8_t *clientHandle){
	printf("\n");
	printf("Client Message Packet\n");
	uint8_t packet[MAXBUF]; //packet that will exported
	int packet_len = 0; //indexer for building packet 
	
	//Add flag to packet
	packet[packet_len++] = FLAG_MESSAGE; 

	//Add client handle len then client handle to the packet
	uint8_t clientHandle_len = strlen((char*)clientHandle);
	packet[packet_len++] = clientHandle_len; 
	memcpy(&packet[packet_len],clientHandle, clientHandle_len);
	packet_len += clientHandle_len;

	//Add Num of handles
	packet[packet_len++] = 1; 
	
	//Get handle from the input_buffer 
	strtok((char*)input_buffer, " "); 
	char* handle = strtok(NULL,  " "); 

	//Calculate destHandle length
	uint8_t handle_len = strlen(handle); 
	if(handle_len > 100){ //Check handle_len
		printf("Error: Handle Length is great than 100 characters");
		exit(-1);
	}
	printf("Le outbound handle: %s, Le Length: %d\n",handle, handle_len); 

	//Build packet to have destHandle length + destHandle(no null) 
	packet[packet_len++] = handle_len;
	memcpy(&packet[packet_len], handle, handle_len); 
	packet_len += handle_len;//increment packet indexer by handle length
	printf("Packet Length: %d\n", packet_len);


	printf("Input message len: %d\n", inputMessageLen); 
	//Add the rest of the message
	int message_index = handle_len + 4; //+4 Offset to account for spaces from input string 
	printf("Message Index: %d\n", message_index); 
	int message_len = inputMessageLen - message_index; //Length of the rest of the message
	memcpy(&packet[packet_len], &input_buffer[message_index], message_len); 

	packet_len += message_len; 

	int sent =  sendPDU(socketNum, packet, packet_len );
	if (sent < 0){
		perror("send call");
		exit(-1);
	}	
}

void client_broadcast_packet(){
	printf("Client broadcast Packet\n");
}

void client_listhandles_packet(){
	printf("Client list handles Packet\n");
}

void client_multicast_packet(int socketNum, uint8_t *input_buffer, int inputMessageLen, uint8_t *clientHandle){
	printf("\n");
	printf("Client Multicast Packet\n");
	uint8_t packet[MAXBUF]; //packet that will exported
	int packet_len = 0; //indexer for output buffer 
	int message_index = 5; //Offset of 4 for flags and spaces. 

	//Add flag to packet
	packet[packet_len++] = FLAG_MULTICAST; 

	//Add client handle len then client handle to the packet
	uint8_t clientHandle_len = strlen((char*)clientHandle);
	packet[packet_len++] = clientHandle_len; 
	memcpy(&packet[packet_len],clientHandle, clientHandle_len);
	packet_len += clientHandle_len;

	//Get Number of Handles
	printf("Index: %d\n", packet_len); 
	strtok((char*)input_buffer," "); //Grabs %C
	int num_handles = atoi(strtok(NULL, " ")); //Grabs the number of handles
	message_index += num_handles;  //num_handle tells us the number of spaces for the handles

	//Add number of handles to packet
	packet[packet_len++] = (char)num_handles; 
	printf("Num of handles: %d\n", num_handles);

	// Check if num_handles is between 2 and 9 (inclusive)
    if (!(num_handles >= 2) && !(num_handles <= 9)) {
		printf("Error: invalid number of handles");
		return; 
	}
	
	//Create 2d array to store the handles
	char handle_list[num_handles][100];
	memset(handle_list,'\0', sizeof(handle_list));

	//Grab handles and put in 2d array 
	for(int i = 0; i < num_handles; i++){
		strcpy(handle_list[i], strtok(NULL," ")); //get handle
		int len = strlen(handle_list[i]); //Get length of handle
		packet[packet_len++] = len; //add handle length to packet 
		memcpy(&packet[packet_len], handle_list[i], len); 
		packet_len += len;
		message_index += len; 
		printf("Handle name: %s and its length: %d \n" ,handle_list[i], len);
	}

	//Add message onto packet 
	int messageLen  = inputMessageLen - message_index; //index of the start of the message of the input buffer
	memcpy(&packet[packet_len], &input_buffer[message_index], messageLen);
	printf("package details: %s\n", packet); 
	//Send complete packet
	int sent =  sendPDU(socketNum, packet, packet_len);
	if (sent < 0){
		perror("send call");
		exit(-1);
	}	
}

//I want to manage the flags that the clients input in the stdin
//Depending on the flag the buffer will change, %M, %C
//This function will return 0 on failure and 1 on success. 
int process_client_input(int socketNum, uint8_t *buffer, int messageLen, uint8_t *clientHandle){
	//Check that the client input starts with a %
	char percent_checker = '\0';
	memcpy(&percent_checker, buffer, 1);
	if( percent_checker != '%'){
		return 0;
	}

	//Flag Checker %M, %C, %B, %L. Grab second byte.
	char flag = '\0';
	memcpy(&flag, buffer + 1, 1);
	switch(flag){
		case('M'):
		case('m'):
			//Message user
			send_client_message_packet(socketNum, buffer, messageLen, clientHandle);
			break; 
		case('L'):
		case('l'):
			//Handles 
			client_listhandles_packet();
			break; 
		case('C'):
		case('c'):
			//Multicast Mode
			client_multicast_packet(socketNum, buffer, messageLen, clientHandle);
			break; 
		case('B'):
		case('b'):
			//Broadcast Mode
			client_broadcast_packet(); 
			break; 
		default: 
			return 0; 
			break; 
	}
	return 1; 
}

//In this function I want to be able to my initial handle to the server 
// Format for PDU 3 byte chat header, 1 byte handle length, then handle. 
void sendHandleToServer(int socketNum, uint8_t *clientHandle){
	uint8_t packet[MAXBUF]; 
	int packet_len = 0; 
	uint8_t flag = 1; 
	
	//Add flag to packet
	packet[packet_len++] = flag; 

	//Add handle length 
	int handle_len = strlen( (char*) clientHandle); 
	packet[packet_len++] = handle_len;
	
	//Check if handle name is too long 
	memcpy(&packet[2],clientHandle, handle_len);
	printf("Packet Flag: %d , Handle Length: %d, Handle: %s\n", packet[0], packet[1], packet);	
	packet_len += handle_len; 
	int sent = sendPDU(socketNum,packet, packet_len); 
	if (sent < 0){
		perror("send call");
		exit(-1);
	}

	//Check Response from server
	processMsgFromServer(socketNum, clientHandle); 
} 

void processStdin(int socketNum, uint8_t *clientHandle){
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;        //amount of data to send
	// int sent = 0;            //actual amount of data sent/* get the data and send it   */
	
	sendLen = readFromStdin(sendBuf);

	if(process_client_input(socketNum, sendBuf, sendLen, clientHandle) == 0){
		printf("Invalid Input Try Again!\n");
		printf("$:");
		fflush(stdout);
		return;
	}

	// printf("read: %s string len: %d (including null)\n", sendBuf, sendLen);
	
	// sent =  sendPDU(socketNum, sendBuf, sendLen);
	// if (sent < 0){
	// 	perror("send call");
	// 	exit(-1);
	// }
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	while (inputLen < (MAXBUF - 1) && aChar != '\n'){
		aChar = getchar();
		if (aChar != '\n'){
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}

	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4){
		printf("usage: %s handle host-name port-number \n", argv[0]);
		exit(1);
	}
}
