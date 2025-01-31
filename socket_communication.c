#include "socket_communication.h"


int sendPDU(int socketNumber, uint8_t * dataBuffer, int lengthOfData){
    //Calculate size of buffer
    uint16_t buffer_size = lengthOfData + 2; 

    //Create buffer for PDU send
    char send_buffer[buffer_size];  

    //Add length to buffer in network order  
    uint16_t PDU_length_host_order = htons(lengthOfData + 2);
    memcpy(send_buffer, &PDU_length_host_order, 2);

    //Add payload to the buffer
    memcpy(send_buffer + 2, dataBuffer, lengthOfData);    

    //Send PDU
    if(send(socketNumber, send_buffer, buffer_size, 0) == -1){
        perror("Send failed");
        exit(-1);
    }
    return lengthOfData;
}

int recvPDU(int clientSocket, uint8_t * dataBuffer, int bufferSize){
    //First recv() call, get PDU length
    size_t bytes = recv(clientSocket, dataBuffer, 2, MSG_WAITALL);
    if(bytes == 0 ){
        return 0; 
    }else if(bytes < 0){
        return -1; 
    }

    uint16_t pdu_length = ntohs(*(uint16_t*)dataBuffer) - 2;
    
    //Check bufferSize with PDU length 
    if(bufferSize < (pdu_length)){
        perror("PDU_length is greater than bufferSize");
        return -1; 
    }

    bytes = recv(clientSocket, dataBuffer, pdu_length, MSG_WAITALL);

    if(bytes < 0){
        if (errno == ECONNRESET)
        {
            return 0; 
        }
            perror("recv call");
            exit(-1);
    }

    return bytes; 
}
