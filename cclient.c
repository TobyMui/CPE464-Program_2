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
	printf("----------------------------------------------------\n"); 

	while(1){
		printf("$:");
		fflush(stdout);
		int active_socket = pollCall(-1);
		if(active_socket == socketNum){
			processMsgFromServer(active_socket, clientHandle);
		}else if(active_socket == STDIN_FILENO){
			processStdin(socketNum,clientHandle); 
		}else{
			printf("Invalid Socket Number");
		}
		printf("\n"); 
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

	//Grab Source Handle
	int packet_index = 1; //Start index at handle length 
	int handle_len = input_packet[packet_index++]; //Get handle len and increment index

	//Grab Source Length 
	char srcHandle[handle_len + 1]; //Add 1 for null terminator
	memcpy(srcHandle, &input_packet[packet_index], handle_len); 
	srcHandle[handle_len] = '\0';
	packet_index += handle_len; 

	//Grab number of handles 
	int numHandles = input_packet[packet_index++];

	//Iterate over the handles and increment packet index
	for(int i = 0; i < numHandles;i++){
		packet_index += input_packet[packet_index]; 
		packet_index++; 
	}
	int message_len = packet_len - packet_index;  

	char buffer[MAXBUF];
	memset(buffer, '\0', sizeof(buffer));
	memcpy(buffer, &input_packet[packet_index], message_len); 
	printf("%s: %s\n",srcHandle, buffer);
}

void printErrorFromServer(uint8_t *input_packet, int packet_len){
	//Init
	char handle[100];
	memset(handle, '\0', sizeof(handle)); //memset so I don't have to deal with null
	int handle_len = input_packet[1]; //handle_len

	//Grab handle 
	memcpy(handle,&input_packet[2], handle_len);
	
	printf("Error: %s does not exist\n", handle); 
}

/*This function is a helper function for printListFromServer.
This function checks that the flag is 12 and prints out the handle*/
void processHandlesFromServer(uint8_t *packet, int messageLen){

	//Grab flag and check its value
	uint8_t flag = packet[0];
	if(flag != FLAG_SENDING_HANDLES){
		printf("Error: Incorrect flag or packet");
	}

	//Grab handle length
	uint8_t handle_len = packet[1];

	//Get handle
	uint8_t handle[100]; 
	memset(handle, '\0', sizeof(handle)); 
	memcpy(handle, &packet[2], handle_len); 
	
	//Print out handle
	printf("Handle: %s\n", handle); 
}

void printBroadcast(uint8_t *packet, int messageLen){
	//Get handle length 
	uint8_t handle_len = packet[1];

	//Get handle 
	uint8_t handle[100];
	memset(handle,'\0', sizeof(handle));
	memcpy(handle,&packet[2], handle_len);

	//Get message
	int packet_message_index  = 2 + handle_len;
	int packet_message_segment_len = messageLen - packet_message_index;
	uint8_t buffer[MAXBUF];
	memcpy(buffer, &packet[packet_message_index], packet_message_segment_len);

	//print
	printf("%s: %s\n", handle, buffer); 
	
}

void printListFromServer(int socketNum, uint8_t *packet){
	
	//Part 1, Flag 11, Get List length 
	uint32_t network_order_list_len = 0; 

	memcpy(&network_order_list_len,&packet[1], sizeof(network_order_list_len));
	uint32_t handle_list_len = ntohl(network_order_list_len);

	//Part 2, Flag 12, Receive packets
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;

	printf("Handle Table List\n"); 
	printf("------------------\n");

	//Iterate by handle len amount 
	for(int i = 0; i < handle_list_len; i++){
		memset(dataBuffer, '\0', sizeof(dataBuffer));//Reset buffer
		if ((messageLen = recvPDU(socketNum, dataBuffer, MAXBUF)) < 0){
			perror("recv call");
			exit(-1);
		}if (messageLen > 0){
			processHandlesFromServer(dataBuffer, messageLen);
		}else{
			close(socketNum);
			removeFromPollSet(socketNum);
			printf("\n");
			printf("Connection closed by other side\n");
			exit(-1);
		}
	}
	
	//Part 2, Flag 13
	memset(dataBuffer, '\0', sizeof(dataBuffer));//Reset buffer
	if ((messageLen = recvPDU(socketNum, dataBuffer, MAXBUF)) < 0){
			perror("recv call");
			exit(-1);
		}if (messageLen > 0){
			int flag13 = dataBuffer[0];
			if(flag13 != FLAG_LIST_FINISHED){
				return; 
			}
		}else{
			close(socketNum);
			removeFromPollSet(socketNum);
			printf("\n");
			printf("Connection closed by other side\n");
			exit(-1);
	}
}


/*In this function we take the incoming message and process the flag that
was sent by the server. This function will then call other functions
depending on the flag. */
void processFlagFromServer(int socketNum, uint8_t *packet, int messageLen, uint8_t * clientHandle){
	uint8_t flag = packet[0]; 

	switch(flag){
		case(FLAG_INITIALIZE_HANDLE_CONFIMATION):
			printf("Valid handle!\n"); 
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
		case(FLAG_UNKNOWN_HANDLE):
			printErrorFromServer(packet,messageLen);
			printf("Error message len: %d\n",messageLen );
			break; 
		case(FLAG_REQUEST_HANDLE_LIST_ACK):
			printListFromServer(socketNum,packet); 
			break;  
		case(FLAG_BROADCAST):
			printBroadcast(packet, messageLen);
			break; 
		default: 
			printf("Error: unknown flag in process_message_flag");
			exit(-1); 
	}
}

//////////////////////////////////////////Send to Server Functions//////////////////////////////////////////////////////////////

/*This is a helper function for send_client message packet 
returns 0 on error and 1 on sucess*/
void build_message_packet_header(uint8_t *packet, int *out_packet_len, uint8_t *clientHandle, uint8_t clientHandle_len, char *handle, uint8_t handle_len) {
    //Add Flag and handle length  
	packet[(*out_packet_len)++] = FLAG_MESSAGE; 
    packet[(*out_packet_len)++] = clientHandle_len; 

	//Add Client handle
    memcpy(&packet[*out_packet_len], clientHandle, clientHandle_len);
	*out_packet_len += clientHandle_len;
	
	//Add Dest handle num 
    packet[(*out_packet_len)++] = 1;

	//Add Dest handle len
    packet[(*out_packet_len)++] = handle_len;

	//Add Dest handle
    memcpy(&packet[*out_packet_len], handle, handle_len);
    *out_packet_len += handle_len;
}

/*This function implement %M*/
void send_client_message_packet(int socketNum, uint8_t *input_buffer, int inputMessageLen, uint8_t *clientHandle) {
    printf("\nClient Message Packet\n");

    uint8_t packet[200]; // Max packet size is 200
    int out_packet_len = 0; //Variable to keep track of packet length

    uint8_t clientHandle_len = strlen((char*)clientHandle);

    strtok((char*)input_buffer, " ");

    char *handle = strtok(NULL, " ");
	if(handle == NULL){
		printf("Error: Missing Handle\n"); 
		return ; 
	}

    uint8_t handle_len = strlen(handle);

    if (handle_len > 100) {
        printf("Error: Handle Length exceeds 100 characters\n");
        return; 
    }

    int message_index = handle_len + 4; // Offset to account for spaces
    int remaining_message_len = inputMessageLen - message_index;
    uint8_t *message_start = &input_buffer[message_index];
	
	//In this while loop, the message is sent in blocks if needed. 
    while (remaining_message_len > 0) {
		out_packet_len = 0; // Reset packet length for new packet
		build_message_packet_header(packet, &out_packet_len, clientHandle, clientHandle_len, handle, handle_len);
			
        //Check to see if message + header will fit in 200 byte
        int max_block_size = MAX_MESSAGE_LEN - out_packet_len - 1; //Calculate block size 
        int block_size = (remaining_message_len > max_block_size) ? max_block_size : remaining_message_len;

		//Add Message into block 
        memcpy(&packet[out_packet_len], message_start, block_size);
        out_packet_len += block_size;
        packet[out_packet_len++] = '\0'; //Add null terminator

        if (sendPDU(socketNum, packet, out_packet_len) < 0) {
            perror("send call");
            exit(-1);
        }

		//Calculate remaining message and new message start
        remaining_message_len -= block_size;
        message_start += block_size;
    }
}

/* This is a helper function for send_broadcast_packet */
void build_broadcast_packet_header(uint8_t *packet, int *out_packet_len, uint8_t *clientHandle) {
    // Add Flag and client handle length  
    packet[(*out_packet_len)++] = FLAG_BROADCAST; 
	uint8_t clientHandle_len = strlen((char*)clientHandle);
    packet[(*out_packet_len)++] = clientHandle_len; 

    // Add Client handle
    memcpy(&packet[*out_packet_len], clientHandle, clientHandle_len);
    *out_packet_len += clientHandle_len;
}

/* This function implements %B */
void send_broadcast_packet(int socketNum, uint8_t *input_buffer, int inputMessageLen, uint8_t *clientHandle) {
    printf("\nBroadcast Message Packet\n");

	//Check message len, print error if command is not valid
	if(inputMessageLen <= 2){
		printf("Error Invalid Command");
		return; 
	}

	//Init for packet building
    uint8_t packet[200]; // Max packet size is 200
    int out_packet_len = 0; // Variable to keep track of packet length

    int message_index = 3; // Offset to account for "%B " prefix
    int remaining_message_len = inputMessageLen - message_index;
    uint8_t *message_start = &input_buffer[message_index];

    // In this while loop, the message is sent in blocks if needed.
    while (remaining_message_len > 0) {
        out_packet_len = 0; // Reset packet length for new packet
        build_broadcast_packet_header(packet, &out_packet_len, clientHandle);

        // Calculate how much message data can fit in the packet
        int max_block_size = 200 - out_packet_len - 1; // Ensure packet does not exceed 200 bytes
        int block_size = (remaining_message_len > max_block_size) ? max_block_size : remaining_message_len;

        // Add Message into packet
        memcpy(&packet[out_packet_len], message_start, block_size);
        out_packet_len += block_size;
        packet[out_packet_len++] = '\0'; // Add null terminator

        if (sendPDU(socketNum, packet, out_packet_len) < 0) {
            perror("send call");
            exit(-1);
        }

        // Update remaining message length and pointer to next message chunk
        remaining_message_len -= block_size;
        message_start += block_size;
    }
}


int build_multicast_packet_header(uint8_t *input_packet, uint8_t *packet, int *out_packet_len, uint8_t *clientHandle, int *input_message_indexer) {
    // Add Flag and handle length
    packet[(*out_packet_len)++] = FLAG_MULTICAST;
    uint8_t clientHandle_len = strlen((char*)clientHandle);
    packet[(*out_packet_len)++] = clientHandle_len;

    // Add Client handle
    memcpy(&packet[*out_packet_len], clientHandle, clientHandle_len);
    *out_packet_len += clientHandle_len;


	//Get num handles 
	int num_handles = input_packet[3] - '0';//Convert to int %c 
	if(num_handles < 2 || num_handles > 9) {
        printf("Error: invalid number of handles\n");
        return 0; 
    }

    // Now parse the input_packet for the number of handles and handle names
    char *token = strtok((char*)input_packet, " "); // Grabs %C
    strtok(NULL, " "); // Skip over num handles


    // Add number of handles to packet
    packet[(*out_packet_len)++] = (uint8_t)num_handles;

    *input_message_indexer += num_handles; // Increment message index by the number of handles (to process the next part)

    // Create buffer to hold a handle
    char handle[100];
    memset(handle, '\0', sizeof(handle));
	
    // Grab handles and add to packet
    for (int i = 0; i < num_handles; i++) {
        memset(handle, '\0', sizeof(handle)); // Reset buffer
        token = strtok(NULL, " "); // Get handle name
        if (token == NULL) {
            printf("Error: Insufficient handles in input\n");
            return 0; 
        }
        strcpy(handle, token); // Copy handle name into buffer
        int len = strlen(handle); // Get length of handle

		//Check Handle length
		if( len > 100){
			printf("Error: Handle name is greater than 100 characters");
			return 0; 
		}

        packet[(*out_packet_len)++] = len; // Add handle length to packet 
        memcpy(&packet[*out_packet_len], handle, len); 
        *out_packet_len += len;

        // Update input_message_indexer with the length of this handle
        *input_message_indexer += len;
    }
	return 1; 
}

void send_multicast_packet(int socketNum, uint8_t *input_packet, int inputMessageLen, uint8_t *clientHandle){
    printf("\nClient Multicast Packet\n");
	printf("input length: %d\n", inputMessageLen);
	//Check message len, print error if command is not valid

	if(inputMessageLen <= 3){
		printf("Error Invalid Command");
		return; 
	}

    uint8_t header_packet[200]; 
    uint8_t packet[200]; // packet that will be exported
    int packet_len = 0; // indexer for output buffer 
    int input_message_indexer = 5; // Offset of 5 for flags and spaces. 

    // Build header packet, exit function if input is invalid
    if(0 == build_multicast_packet_header(input_packet, header_packet, &packet_len, clientHandle, &input_message_indexer)){
		return; 
	}
    
    int remaining_message_len = inputMessageLen - input_message_indexer;
    uint8_t *message_start = &input_packet[input_message_indexer];
    int header_packet_len = packet_len; 

    // In this while loop, the message is sent in blocks if needed. 
    while (remaining_message_len > 0) {
        // Clear the packet buffer only once
        memset(packet, '\0', sizeof(packet)); 

        // Copy header into the packet
        memcpy(packet, header_packet, header_packet_len);

        // Calculate maximum block size for message
        int max_block_size = MAX_MESSAGE_LEN - header_packet_len - 1; // Calculate block size considering header length
        int block_size = (remaining_message_len > max_block_size) ? max_block_size : remaining_message_len;

        // Add message data into packet
        memcpy(&packet[header_packet_len], message_start, block_size);
        packet_len = header_packet_len + block_size; // Update the packet_len after adding message
        packet[packet_len++] = '\0'; // Add null terminator

        // Send the packet
        if (sendPDU(socketNum, packet, packet_len) < 0) {
            perror("send call");
            exit(-1);
        }

        // Update remaining message length and message start position
        remaining_message_len -= block_size;
        message_start += block_size;
    }
}

/*This function sends a request to the server for the handle list*/
void send_listhandles_packet(int socketNum){
	uint8_t packet[1]; 
	packet[0] = FLAG_REQUEST_HANDLE_LIST;
	int sent = sendPDU(socketNum,packet,1); 
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

	if(buffer[2] != ' ' && !(flag == 'l' || flag == 'L')){
		return 0; 
	}


	switch(flag){
		case('M'):
		case('m'):
			//Message user
			send_client_message_packet(socketNum, buffer, messageLen, clientHandle);
			break; 
		case('L'):
		case('l'):
			//Handles 
			send_listhandles_packet(socketNum);
			break; 
		case('C'):
		case('c'):
			//Multicast Mode
			send_multicast_packet(socketNum, buffer, messageLen, clientHandle);
			break; 
		case('B'):
		case('b'):
			//Broadcast Mode
			send_broadcast_packet(socketNum, buffer, messageLen, clientHandle); 
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
