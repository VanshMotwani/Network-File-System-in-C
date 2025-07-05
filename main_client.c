#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "headers.h"

// #define NM_PORT 8090

#define SUPER_BUFFER_SIZE 1024
#define MAX_BUFFER_SIZE 1024

#define pnl printf("\n");

int main()
{
    // Connect to the naming server
    // Naming server can recognize that a client wants to connect because server sends $ on connecting but client doesn't

    int sock = 0;
    struct sockaddr_in server_addr;

    char buffer[SUPER_BUFFER_SIZE];
    char buffer2[SUPER_BUFFER_SIZE];
    char buffer_small[MAX_BUFFER_SIZE];

    // CLI for the client to execute command ---------------------------------------------
    // -----------------------------------------------------------------------------------
    while (1)
    {
        printf("~> ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strlen(buffer) - 1] = '\0';

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

        if (strncmp(buffer, "WRITE", 5) == 0)
        {
            // printf("In write\n");
            char *token;
            char rest[1024];
            strcpy(rest, buffer);
            int token_count = 0;
            char *file_path;
            char *content;

            token = strtok(rest, " ");
            file_path = strtok(NULL, " ");
            // printf("File path: |%s|\n", file_path);
            file_path = strtok(file_path, " ");
            // printf("File path: |%s|\n", file_path);
            snprintf(buffer_small, MAX_BUFFER_SIZE, "WRITE %s", file_path);
            send(sock, buffer_small, strlen(buffer_small), 0);
            int bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            if (bytes_recieved <= 0)
            {
                printf("ERROR 101: Error in receiving data from naming server");
                continue;
            }
            buffer_small[bytes_recieved] = '\0';
            int SS_PORT = atoi(buffer_small);
            if (SS_PORT == -1)
            {
                printf("ERROR 102: File not found\n");
                continue;
            }
            //printf("PORT of storage server: %d\n", SS_PORT);
            // close the connection with the naming server
            close(sock);
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
                Send "WRITE <file_path> <content>" to the storage server to indicate that the client read to write a file
            */

            // printf("sending |%s|\n ", buffer);
            send(sock, buffer, strlen(buffer), 0);
            // sleep(1);
            bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            buffer_small[bytes_recieved] = '\0';
            //printf("|%s|", buffer_small);
            fflush(stdout);
            if (strcmp(buffer_small, "ACK_W") == 0)
            {
                printf("RECIVED\n");
                fflush(stdout);
            }
            else if (!strcmp(buffer_small, "Someone Is Already Writing"))
            {
                printf("ERROR 103: Someone Is Already Writing\n");
            }
            else
            {
                printf("ERROR 104: ERROR WRITING FILE\n");
            }
            close(sock);
        }

        else if (strncmp(buffer, "READ", 4) == 0)
        {
            // printf("In read\n");
            send(sock, buffer, strlen(buffer), 0);
            // ----------------------------------------------------------------
            // Recieve the port number of SS ----------------------------------
            int bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            if (bytes_recieved <= 0)
            {
                printf("ERROR 201: Error in receiving data from naming server");
                continue;
            }
            buffer_small[bytes_recieved] = '\0';
            int SS_PORT = atoi(buffer_small);
            if (SS_PORT == -1)
            {
                printf("ERROR 202: File not found\n");
                continue;
            }
            // printf("PORT of storage server: %d\n", SS_PORT);
            //  close the connection with the naming server
            close(sock);
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
                Send "READ <file_path>" to the storage server to indicate that the client read to write a file
            */
            send(sock, buffer, strlen(buffer), 0);
            bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            if (bytes_recieved <= 0)
            {
                printf("ERROR 203: Error in receiving data from storage server");
                continue;
            }
            buffer_small[bytes_recieved] = '\0';
            printf("%s\n", buffer_small);
            sleep(1);
            bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            buffer_small[bytes_recieved] = '\0';
            fflush(stdout);
            if (strcmp(buffer_small, "ACK_R") == 0)
            {
                // printf("RECIVED\n");
                fflush(stdout);
            }
            else
            {
                printf("ERROR 204: Error READING FILE\n");
            }
            close(sock);
        }

        else if (strncmp(buffer, "GETINFO", 7) == 0)
        {

            // printf("In GETINFO\n");
            /*
                Send the file path to the naming server
                Naming server will send the PORT of storage server to the client
                Client will send the file to the storage servers
            */
            send(sock, buffer, strlen(buffer), 0);

            // ----------------------------------------------------------------
            // Recieve the port number of SS ----------------------------------
            int bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            if (bytes_recieved <= 0)
            {
                printf("ERROR 301: Error in receiving data from naming server");
                continue;
            }
            buffer_small[bytes_recieved] = '\0';
            int SS_PORT = atoi(buffer_small);
            if (SS_PORT == -1)
            {
                printf("ERROR 302: File not found\n");
                continue;
            }
            // printf("PORT of storage server: %d\n", SS_PORT);
            // close the connection with the naming server
            close(sock);
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
                Send "READ <file_path>" to the storage server to indicate that the client read to write a file
            */
            send(sock, buffer, strlen(buffer), 0);
            bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            if (bytes_recieved <= 0)
            {
                printf("ERROR 303: Error in receiving data from storage server");
                continue;
            }
            buffer_small[bytes_recieved] = '\0';
            // Parse the received data
            long long fileSize;
            char fileType;
            int userRead, userWrite, userExecute;
            int groupRead, groupWrite, groupExecute;
            int otherRead, otherWrite, otherExecute;
            char mtime[50], atime[50], ctime[50];

            sscanf(buffer_small, "%lld,%c,%d,%d,%d,%d,%d,%d,%d,%d,%d,%[^,],%[^,],%[^,]",
                   &fileSize,
                   &fileType,
                   &userRead, &userWrite, &userExecute,
                   &groupRead, &groupWrite, &groupExecute,
                   &otherRead, &otherWrite, &otherExecute,
                   mtime, atime, ctime);

            // Format permissions so they look sexy
            char permissions[11];
            sprintf(permissions, "%c%c%c%c%c%c%c%c%c%c",
                    fileType == 'd' ? 'd' : '-',
                    userRead ? 'r' : '-', userWrite ? 'w' : '-', userExecute ? 'x' : '-',
                    groupRead ? 'r' : '-', groupWrite ? 'w' : '-', groupExecute ? 'x' : '-',
                    otherRead ? 'r' : '-', otherWrite ? 'w' : '-', otherExecute ? 'x' : '-');

            printf("%s %lld %s", permissions, fileSize, mtime);
            printf("Last access time: %s", atime);
            printf("Creation time: %s", ctime);
            sleep(1);
            bytes_recieved = recv(sock, buffer_small, MAX_BUFFER_SIZE, 0);
            buffer_small[bytes_recieved] = '\0';
            if (strcmp(buffer_small, "ACK_G") == 0)
            {
                // printf("RECIVED\n");
                fflush(stdout);
            }
            else
            {
                printf("ERROR 304: ERROR GETTING FILE INFO\n");
            }
            close(sock);
        }

        else if (strncmp(buffer, "CREATEDIR", 9) == 0)
        {
            send(sock, buffer, strlen(buffer), 0);
            close(sock);
        }

        else if (strncmp(buffer, "CREATEFILE", 10) == 0)
        {
            send(sock, buffer, strlen(buffer), 0);
            close(sock);
        }

        else if (strncmp(buffer, "DELETEDIR", 9) == 0)
        {
            send(sock, buffer, strlen(buffer), 0);
            close(sock);
        }

        else if (strncmp(buffer, "DELETEFILE", 10) == 0)
        {
            send(sock, buffer, strlen(buffer), 0);
            close(sock);
        }

        else if (strncmp(buffer, "COPY", 4) == 0)
        {
            send(sock, buffer, strlen(buffer), 0);
            close(sock);
        }

        else
        {
            close(sock);
            printf("Invalid command\n");
            printf("Valid commands are:\n");
            printf("WRITE <file_path> <content>\n");
            printf("READ <file_path>\n");
            printf("GETINFO <file_path>\n");
            printf("CREATEDIR <dir_path>\n");
            printf("CREATEFILE <file_path>\n");
            printf("DELETEDIR <dir_path>\n");
            printf("DELETEFILE <file_path>\n");
            printf("COPY <file_path> <new_file_path>\n");
        }
    }
    //     // -----------------------------------------------------------------------------------
    //     // -----------------------------------------------------------------------------------
    return 0;
}
