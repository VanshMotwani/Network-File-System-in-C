#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_PORT 8084
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
    serverAddr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_ADDRESS, &serverAddr.sin_addr) <= 0) {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Send periodic messages
    char message[2000][2000];
    strcpy(message[0],"GETINFO ab/bc/a.txt");
    strcpy(message[1],"WRITE ab/bc/a.txt Hello Mate");
    strcpy(message[2],"READ ab/bc/a.txt");
    strcpy(message[3],"DELETEDIR ab/bc");
    strcpy(message[4],"DELETEFILE ab/de/s.txt");
    strcpy(message[5],"HelloServer6");
    strcpy(message[6],"HelloServer7");
    strcpy(message[7],"HelloServer8");
    strcpy(message[8],"HelloServer9");
    strcpy(message[9],"e");
    strcpy(message[10],"r");
    strcpy(message[11],"\n");
    strcpy(message[12]," ");
    strcpy(message[13]," ");

    int i=3;
    // while (1) {
    char buffer[1024];
        // if (i==11)
        // {
        //     break;
        // }
        
        // You can modify the message content as needed

        // Send the message
        if (send(clientSocket, message[i], strlen(message[i]), 0) == -1) {
            perror("Message sending failed");
            exit(EXIT_FAILURE);
        }

        printf("Message sent: %s\n", message[i]);
        i++;
        // sleep(MESSAGE_INTERVAL);
        recv(clientSocket , buffer , 1024 , 0);
    // }
    printf("|%s|\n" ,buffer);
    
    // Close the socket
    close(clientSocket);

    return 0;
}

