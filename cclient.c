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
void clientControl(int socket);
void processStdin(int socket);
void processMsgFromServer(int socket);
void checkArgs(int argc, char * argv[]);
void sendHandleToServer(int socketNum, uint8_t *handle); 
void process_message_flag(int socketNum, uint8_t *packet, int messageLen);
void client_message_packet(uint8_t *input_buffer, int inputMessageLen);

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	checkArgs(argc, argv);     //Check arguments

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	sendHandleToServer(socketNum, (uint8_t*)argv[1]);
	clientControl(socketNum);

	close(socketNum);
	return 0;
}

void clientControl(int socket){
	//Initialize Poll
	setupPollSet();
	
	//Add to pollset
	addToPollSet(socket);
	addToPollSet(STDIN_FILENO);

	printf("Enter data:");
	fflush(stdout);

	while(1){
		int active_socket = pollCall(-1);
		if(active_socket == socket){
			processMsgFromServer(active_socket);
		}else if(active_socket == STDIN_FILENO){
			processStdin(socket); 
		}else{
			printf("Invalid Socket Number");
		}
	}
}

void processMsgFromServer(int socket){
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
	//now get the data from the client_socket
	if ((messageLen = recvPDU(socket, dataBuffer, MAXBUF)) < 0){
		perror("recv call");
		exit(-1);
	}if (messageLen > 0){
		process_message_flag(socket,dataBuffer, messageLen); 
		printf("Message received from server length: %d Data: %s\n", messageLen, dataBuffer);
		fflush(stdout);
	}else{
		close(socket);
		removeFromPollSet(socket);
		printf("\n");
		printf("Connection closed by other side\n");
		exit(-1);
	}
} 

/*In this function we take the incoming message and process the flag that
was sent by the server. This function will then call other functions
depending on the flag. */
void process_message_flag(int socketNum, uint8_t *packet, int messageLen){
	uint8_t flag = packet[0]; 
	
	switch(flag){
		case(FLAG_INITIALIZE_HANDLE_CONFIMATION):
			printf("Good handle\n"); 
			break; 
		case(FLAG_INITIALIZE_HANDLE_ERROR): 
			printf("Error on initial packet, please check your handle name\n");
			exit(-1); 
			break; 
		default: 
			printf("Error: unknown flag in process_message_flag");
			exit(-1); 
	}
}

// Implementation of %M 
//I want to output the buffer in this format. 3 byte chat header, 1 byte containing the length of sending clients handle 
//We must extract the handle, handle length, and the message. 
void send_client_message_packet(int socketNum, uint8_t *input_buffer, int inputMessageLen){
	printf("Client Message Packet\n");
	uint8_t packet[MAXBUF]; //packet that will exported
	int packet_len = 0; //indexer for building packet 
	//Add flag to packet
	packet[packet_len++] = FLAG_MESSAGE; 
	
	//Get handle from the input_buffer 
	strtok((char*)input_buffer, " "); 
	char* handle = strtok(NULL,  " "); 

	//Calculate handle length
	uint8_t handle_len = strlen(handle); //Im going to hope to god that memcpy works here and doesn't add the null
	if(handle_len > 100){ //Check handle_len
		printf("Error: Handle Length is great than 100 characters");
		exit(-1);
	}
	printf("Le outbound handle: %s, Le Length: %d\n",handle, handle_len); 

	//Build packet to have handle length + handle(no null) 
	packet[packet_len++] = handle_len;
	memcpy(&packet[packet_len], handle, handle_len); 
	packet_len += handle_len;//increment packet indexer by handle length
	printf("Packet Length: %d\n", packet_len);


	printf("Input message len: %d\n", inputMessageLen); 
	//Add the rest of the message
	int message_index = packet_len + 2; //+2 Offset to account for spaces from input string 
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

void client_multibroadcast_packet(){
	printf("Client multi packet");
}

//I want to manage the flags that the clients input in the stdin
//Depending on the flag the buffer will change, %M, %C
//This function will return 0 on failure and 1 on success. 
int process_client_input(int socketNum, uint8_t *buffer, int messageLen){
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
			send_client_message_packet(socketNum, buffer, messageLen);
			break; 
		case('L'):
		case('l'):
			//Handles 
			client_listhandles_packet();
			break; 
		case('C'):
		case('c'):
			//Multicast Mode
			client_multibroadcast_packet();
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
void sendHandleToServer(int socketNum, uint8_t *handle){
	uint8_t packet[MAXBUF]; 
	int packet_len = 0; 
	uint8_t flag = 1; 
	
	//Add flag to packet
	packet[packet_len++] = flag; 

	//Add handle length 
	int handle_len = strlen( (char*) handle); 
	packet[packet_len++] = handle_len;
	
	//Check if handle name is too long 
	memcpy(&packet[2],handle, handle_len);
	printf("Packet Flag: %d , Handle Length: %d, Handle: %s\n", packet[0], packet[1], packet);	
	packet_len += handle_len; 
	int sent = sendPDU(socketNum,packet, packet_len); 
	if (sent < 0){
		perror("send call");
		exit(-1);
	}

	//Check Response from server
	processMsgFromServer(socketNum); 

} 

void processStdin(int socketNum){
	uint8_t sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;        //amount of data to send
	int sent = 0;            //actual amount of data sent/* get the data and send it   */
	
	sendLen = readFromStdin(sendBuf);

	if(process_client_input(socketNum, sendBuf, sendLen) == 0){
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
