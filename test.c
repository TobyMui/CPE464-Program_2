#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>

#include "socket_communication.h"
#include "pollLib.h"
#include "networks.h"

#define MAX_HANDLE_LEN 100
#define MAX_CLIENTS 10000 // Number of clients to simulate
#define SERVER_PORT "12345" // Replace with your server's port
#define SERVER_IP "localhost" // Replace with your server's IP

#define MAXBUF 200
#define DEBUG_FLAG 1

// Function prototypes
void register_handle(int socketNum, const char *handle);
void send_initial_packet(int socketNum, const char *handle);
int wait_for_server_response(int socketNum);

int main() {
    int sockets[MAX_CLIENTS];
    char handleName[MAX_HANDLE_LEN];
    int i;

    // Initialize the poll set
    setupPollSet();

    // Simulate 300 clients connecting to the server
    for (i = 0; i < MAX_CLIENTS; i++) {
        // Create a new socket and connect to the server
        sockets[i] = tcpClientSetup(SERVER_IP, SERVER_PORT, DEBUG_FLAG);
        if (sockets[i] < 0) {
            perror("Failed to connect to server");
            exit(1);
        }

        // Generate a unique handle name
        snprintf(handleName, MAX_HANDLE_LEN, "test%d", i);

        // Register the handle with the server
        register_handle(sockets[i], handleName);

        // Add the socket to the poll set
        addToPollSet(sockets[i]);

        printf("Registered handle: %s\n", handleName);
    }

    // Block indefinitely to keep the connections alive
    printf("All handles registered. Blocking to keep connections alive...\n");
    while (1) {
        int readySocket = pollCall(-1); // Block indefinitely
        if (readySocket < 0) {
            perror("Poll failed");
            exit(1);
        }

        // Handle any incoming data (though this tester doesn't expect any)
        uint8_t buffer[MAXBUF];
        int msg_len = recvPDU(readySocket, buffer, MAXBUF);
        if (msg_len <= 0) {
            if (msg_len == 0) {
                printf("Server closed connection for socket %d\n", readySocket);
            } else {
                perror("recvPDU failed");
            }
        }
    }

    // Close all sockets (this will never be reached due to the infinite loop)
    for (i = 0; i < MAX_CLIENTS; i++) {
        close(sockets[i]);
    }

    return 0;
}

// Register a handle with the server
void register_handle(int socketNum, const char *handle) {
    send_initial_packet(socketNum, handle);
    if (wait_for_server_response(socketNum) != 0) {
        printf("Failed to register handle: %s\n", handle);
        close(socketNum);
        exit(1);
    }
}

// Send the initial packet to the server with the client's handle
void send_initial_packet(int socketNum, const char *handle) {
    size_t handle_len = strlen(handle);
    uint8_t payload[1 + 1 + handle_len]; // Flag + handle_len + handle
    payload[0] = 0x01; // Flag for initial packet
    payload[1] = (uint8_t)handle_len; // Length of the handle
    memcpy(payload + 2, handle, handle_len); // Copy the handle into the payload
    if (sendPDU(socketNum, payload, 1 + 1 + handle_len) < 0) {
        perror("Error sending initial packet");
        exit(1);
    }
}

// Wait for a response from the server after sending the initial packet
int wait_for_server_response(int socketNum) {
    uint8_t buffer[MAXBUF];
    int msg_len = recvPDU(socketNum, buffer, MAXBUF);
    if (msg_len < 0) {
        perror("Error receiving server response");
        return -1;
    }
    if (msg_len < 1) return -1;
    uint8_t flag = buffer[0];
    if (flag == 0x02) return 0; // Handle accepted
    else if (flag == 0x03) return -1; // Handle already in use
    else {
        printf("Invalid server response (flag: %d)\n", flag);
        return -1;
    }
}