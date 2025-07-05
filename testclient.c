#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "headers.h"

// #define NM_PORT 8080
#define SERVER_ADDRESS "127.0.0.1"
#define MESSAGE_INTERVAL 3 // seconds

int main() {
    int clientSocket;
    struct sockaddr_in serverAddr;

    // Create socket
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address struct
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(NM_PORT);
    if (inet_pton(AF_INET, SERVER_ADDRESS, &serverAddr.sin_addr) <= 0) {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Send initial message
    char message[2000][1024];
    // strcpy(message[0], "READ folder1/folderX/folderXX/file.txt");
    // strcpy(message[0], "READ folder2/folderY/file2.txt");
    strcpy(message[0], "COPY folderY folderX");

    int i = 0;
    // while (i < 2000) {
    //     // Send message to the server
    //     if (i!=0){
	//    continue;
	// }
    if (send(clientSocket, message[i], strlen(message[i]), 0) == -1) {
        perror("Message sending failed");
        exit(EXIT_FAILURE);
    }

    printf("Message sent: %s\n", message[i]);

    // Receive port number from server
    char serverResponse[1024];
    int bytesReceived = recv(clientSocket, serverResponse, sizeof(serverResponse), 0);
    if (bytesReceived > 0) {
        serverResponse[bytesReceived] = '\0';
        printf("Received port number from server: %s\n", serverResponse);

        // Close the current connection
        close(clientSocket);

        // Create a new socket for the received port number
        int newClientSocket;
        if ((newClientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("New socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Initialize server address struct for the received port
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(atoi(serverResponse)); // Convert string port to integer
        if (inet_pton(AF_INET, SERVER_ADDRESS, &serverAddr.sin_addr) <= 0) {
            perror("Invalid address");
            exit(EXIT_FAILURE);
        }

        // Connect to the new port
        if (connect(newClientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
            perror("Connection failed");
            exit(EXIT_FAILURE);
        }

        // Update the socket to the new socket
        clientSocket = newClientSocket;
        
        if (send(clientSocket, message[0], strlen(message[0]), 0) == -1) {
            perror("Message sending failed");
            exit(EXIT_FAILURE);
        }

        printf("Resent Message: %s\n", message[0]);

        char buffer[1024];
        recv(clientSocket, buffer, 1024, 0);

        printf("%s\n", buffer);

    } 
    else if (bytesReceived == 0) {
        printf("Server disconnected\n");
        // break;
    } 
    else {
        perror("Receive failed");
        exit(EXIT_FAILURE);
    }

    i++;
    close(clientSocket);
}
