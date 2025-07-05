#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define NM_PORT 8080
#define ID 1
#define CID "#1"

#define SUPER_BUFFER_SIZE 16384
#define MAX_BUFFER_SIZE 1024

#define pnl printf("\n");

int main()
{
    // Connect to the naming server
    // Naming server can recognize that a client wants to connect because a client will
    // send #1 after connecting to the naming server

    int sock = 0;
    struct sockaddr_in server_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NM_PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address");
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Connection failed");
        return 1;
    }

    char buffer[SUPER_BUFFER_SIZE];
    char buffer2[SUPER_BUFFER_SIZE];
    char buffer_small[MAX_BUFFER_SIZE];
    snprintf(buffer_small, MAX_BUFFER_SIZE, CID);
    send(sock, buffer_small, strlen(buffer_small), 0); // sending #1 to naming server
    // CLI for the client to execute command ---------------------------------------------
    // -----------------------------------------------------------------------------------
    while (1)
    {
        printf("Enter command: ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strlen(buffer) - 1] = '\0';

        if (strcmp(buffer, "WRITE") == 0)
        {
            printf("Enter file path: ");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strlen(buffer) - 1] = '\0';
            strcpy(buffer2, buffer); // buffer2 contains the file path

            /*
                Send the file path to the naming server
                Naming server will send the PORT of storage server to the client
                Client will send the file to the storage servers
            */

            snprintf(buffer_small, MAX_BUFFER_SIZE, "%s", buffer2);
            send(sock, buffer_small, strlen(buffer_small), 0);

            // ----------------------------------------------------------------
            // Recieve the port number of SS ----------------------------------
            int bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            if (bytes_recieved <= 0)
            {
                perror("Error in receiving data from naming server");
                continue;
            }
            buffer_small[bytes_recieved] = '\0';

            int SS_PORT = atoi(buffer_small);
            // printf("PORT of storage server: %d\n", SS_PORT);
            // ----------------------------------------------------------------

            /*
                 Connect with the storage server using the PORT received from the naming server
            */

            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
            {
                perror("Socket creation failed");
                return 1;
            }

            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(SS_PORT);

            if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
            {
                perror("Invalid address");
                return 1;
            }

            if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
            {
                perror("Connection failed");
                return 1;
            }

            /*
                Send "WRITE" to the storage server to indicate that the client wants to write a file
            */

            snprintf(buffer_small, MAX_BUFFER_SIZE, "WRITE");
            send(sock, buffer_small, strlen(buffer_small), 0);

            /*
                Send the file path to the storage server
            */

            snprintf(buffer_small, MAX_BUFFER_SIZE, "%s", buffer2);
            send(sock, buffer_small, strlen(buffer_small), 0);

            printf("Enter data: ");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strlen(buffer) - 1] = '\0';

            // divide the data into chunks of size 1023
            // send the number of chunks

            int number_of_chunks = strlen(buffer) / 1023;
            if (number_of_chunks * 1023 < strlen(buffer))
            {
                number_of_chunks++;
            }

            snprintf(buffer_small, MAX_BUFFER_SIZE, "%d", number_of_chunks);
            send(sock, buffer_small, strlen(buffer_small), 0);

            for (int i = 0; i < number_of_chunks; i++)
            {
                snprintf(buffer_small, MAX_BUFFER_SIZE, "%s", buffer + i * 1023);
                send(sock, buffer_small, strlen(buffer_small), 0);
            }

            /*
                Receive the response from the storage server
            */

            bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            if (bytes_recieved <= 0)
            {
                perror("Error in receiving data from storage server");
                continue;
            }

            buffer_small[bytes_recieved] = '\0';
            if (strcmp(buffer_small, "ACK_W") == 0)
            {
                printf("File written successfully\n");
            }
            else
            {
                printf("Error in writing file\n");
            }


            // connect again to the naming server
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
            {
                perror("Socket creation failed");
                return 1;
            }

            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(NM_PORT);

            if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
            {
                perror("Invalid address");
                return 1;
            }

            if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
            {
                perror("Connection with Naming Server failed");
                return 1;
            }
        }

        else if (strcmp(buffer, "READ") == 0)
        {
            printf("Enter file path: ");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strlen(buffer) - 1] = '\0';
            strcpy(buffer2, buffer); // buffer2 contains the file path

            /*
                Send the file path to the naming server
                Naming server will send the PORT of storage server to the client
                Client will send the file to the storage servers
            */

            snprintf(buffer_small, MAX_BUFFER_SIZE, "%s", buffer2);
            send(sock, buffer_small, strlen(buffer_small), 0);

            // ----------------------------------------------------------------
            // Recieve the port number of SS ----------------------------------
            int bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            if (bytes_recieved <= 0)
            {
                perror("Error in receiving data from naming server");
                continue;
            }
            buffer_small[bytes_recieved] = '\0';

            int SS_PORT = atoi(buffer_small);
            // printf("PORT of storage server: %d\n", SS_PORT);
            // ----------------------------------------------------------------

            /*
                 Connect with the storage server using the PORT received from the naming server
            */

            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
            {
                perror("Socket creation failed");
                return 1;
            }

            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(SS_PORT);

            if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
            {
                perror("Invalid address");
                return 1;
            }

            if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
            {
                perror("Connection failed");
                return 1;
            }

            /*
                Send "READ" to the storage server to indicate that the client read to write a file
            */

            snprintf(buffer_small, MAX_BUFFER_SIZE, "READ");
            send(sock, buffer_small, strlen(buffer_small), 0);

            /*
                Send the file path to the storage server
            */

            snprintf(buffer_small, MAX_BUFFER_SIZE, "%s", buffer2);
            send(sock, buffer_small, strlen(buffer_small), 0);

            // receive the number of chunks
            bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            int number_of_chunks;
            if (bytes_recieved <= 0)
            {
                perror("Error in receiving number of chunks from storage server");
                continue;
            }
            buffer_small[bytes_recieved] = '\0';
            number_of_chunks = atoi(buffer_small); // convert string to integer

            for (int i = 0; i < number_of_chunks; i++)
            {
                bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
                if (bytes_recieved <= 0)
                {
                    perror("Error in receiving data from storage server");
                    continue;
                }
                buffer_small[bytes_recieved] = '\0';
                printf("%s", buffer_small);
            }
            pnl;

            /*
                Receive the response from the storage server
            */

            bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            if (bytes_recieved <= 0)
            {
                perror("Error in receiving data from storage server");
                continue;
            }

            buffer_small[bytes_recieved] = '\0';
            if (strcmp(buffer_small, "ACK_R") == 0)
            {
                // printf("File read successfully\n");
            }
            else
            {
                printf("Error Reading file\n");
            }


            // connect again to the naming server
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
            {
                perror("Socket creation failed");
                return 1;
            }

            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(NM_PORT);

            if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
            {
                perror("Invalid address");
                return 1;
            }

            if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
            {
                perror("Connection with Naming Server failed");
                return 1;
            }
        }

        // else if (strcmp(buffer, "DELETE") == 0)
        // {
        // }

        // else if (strcmp(buffer, "CREATE") == 0)
        // {
        // }

        // else if (strcmp(buffer, "LIST") == 0)
        // {
        // }

        else
        {
            perror("Invalid command");
        }
    }

    // -----------------------------------------------------------------------------------
    // -----------------------------------------------------------------------------------

    close(sock);
    return 0;
}
