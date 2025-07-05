#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_PORT 8084
#define SERVER_ADDRESS "127.0.0.1"
#define MESSAGE_INTERVAL 5 // seconds

int main()
{
    int clientSocket;
    struct sockaddr_in serverAddr;

    // Create socket
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address struct
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_ADDRESS, &serverAddr.sin_addr) <= 0)
    {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Send periodic messages
    char message[2000][20];
    strcpy(message[0], "READ ab/lm.txt");
    strcpy(message[1], "BAKAYARO2");
    strcpy(message[2], "BAKAYARO3");
    strcpy(message[3], "BAKAYARO4");
    strcpy(message[4], "BAKAYARO5");
    strcpy(message[5], "S");
    strcpy(message[6], "e");
    strcpy(message[7], "r");
    strcpy(message[8], "v");
    strcpy(message[9], "e");
    strcpy(message[10], "r");
    strcpy(message[11], "\n");
    strcpy(message[12], " ");
    strcpy(message[13], " ");

    int i = 0;
    // while (1) {

    // You can modify the message content as needed

    // Send the message
    char buffer[1024];
    if (send(clientSocket, message[i], strlen(message[i]), 0) == -1)
    {
        perror("Message sending failed");
        exit(EXIT_FAILURE);
    }

    printf("Message sent: %s\n", message[i]);
    recv(clientSocket, buffer, 1024, 0);

    //     i++;
    //     sleep(MESSAGE_INTERVAL);
    // }
    printf("|%s|\n", buffer);

    // Close the socket
    close(clientSocket);

    return 0;
}
