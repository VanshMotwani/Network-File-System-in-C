#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <pthread.h>
#include <semaphore.h>
#include "headers.h"
#include <dirent.h>
#include <sys/stat.h>

// #define NM_PORT 8080
#define MAX_CONNECTIONS 200
#define MAX_BUFFER_SIZE 1024
#define MAX_STORAGE_SERVERS 1000
#define MAX_PATHS_IN_ONE_SERVER 1000
#define ALPHABET_SIZE 65 // 26 alphabets, 1 '/', 1 '.', 10 digits, 1 '$', 26 UPPER CASE alphabets

#define MAX_CLIENTS 20

char SS_IP_ADDRs[MAX_STORAGE_SERVERS][MAX_BUFFER_SIZE];
int SS_PORTS[MAX_STORAGE_SERVERS];
int STORAGE_SERVER_NUM[MAX_STORAGE_SERVERS] = {0};
int NUM_STORAGE_SERVERS = 0;
int is_SS_DOWN[MAX_STORAGE_SERVERS] = {-1};
pthread_t STORAGE_SERVER_THREAD_IDS[MAX_STORAGE_SERVERS];
char SS_PATHS[MAX_STORAGE_SERVERS][MAX_PATHS_IN_ONE_SERVER][MAX_BUFFER_SIZE];
int SS_PATH_COUNT[MAX_STORAGE_SERVERS];

int client_fds[MAX_CONNECTIONS] = {-1};
fd_set readfds;
fd_set client_fds_thread;

int Active_SS_count = 0;

int Has_Been_Backed_Up[MAX_STORAGE_SERVERS][2];

sem_t GlobalSemaphore;

/* trie --------------------------------------------------------------------
---------------------------------------------------------------------------
    trie source code from
    https://www.sanfoundry.com/c-program-implement-trie/
    with suitable modifications
*/
struct node
{
    int data; // id of storage server
    int type; // 0 for folder, 1 for file
    struct node *link[ALPHABET_SIZE];
};
struct node *create_node();
void insert_node(char key[], int data, int type);
int delete_node(char key[]); // returns 0 if successful, -1 if not
int search(char key[]);
int search2(char key[]);
char **finder(char key[]);
char **finderByData(int data);
struct node *root = NULL;
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------

// LRU CACHE ----------------------------------------------------------------
struct LRU_node
{
    char *path;
    int data;
    int type;
    struct LRU_node *next;
    struct LRU_node *prev;
};
struct LRU_node *head = NULL;
struct LRU_node *tail = NULL;
int LRU_size = 0;
int LRU_capacity = 10;
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------

struct BackupArgs{
    int SS_number;
    int SS_to;
};

struct ThreadArgs
{
    int master_fd;
};

struct SS_Connection_Args
{
    int SS_number;
    int client_fd_ID;
    int reestablished_Connection;
    int command;
    int returnToClient_FD;
    char cmd_buffer[MAX_BUFFER_SIZE];
    char cmd_buffer2[MAX_BUFFER_SIZE];
    char write_buffer[2*MAX_BUFFER_SIZE];
};

FILE* fp;

void *storage_server_send(void *arg)
{
    struct SS_Connection_Args args;
    args = *((struct SS_Connection_Args *)arg);
    int SS_sock = 0;
    struct sockaddr_in server_addr;
    int command = args.command;
    char* cmd_buffer = args.cmd_buffer; 

    if (command != 1){
        if ((SS_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            perror("Socket creation failed");
            return 0;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SS_PORTS[args.SS_number]);
        printf("PORT: %d %d\n", args.SS_number, SS_PORTS[args.SS_number]);
        fp = fopen("logs.txt", "a+");
        fprintf(fp, "PORT: %d %d\n", args.SS_number, SS_PORTS[args.SS_number]);
        fclose(fp);

        if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
        {
            perror("Invalid address");
            return 0;
        }

        if (connect(SS_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        {
            // printf("%s\n", cmd_buffer);
            perror("NOW Connection failed");
            return 0;
        }
    }

    // if command == 1: copy from SS_x to SS_y
    // if command == 2: to the SS, send the recieved cmd_buffer
    // if command == 3: send as it is

    // COMMUNICATE WITH SS HERE, GIVE EACH COMMAND A CODE
    if (command == 3){
        if (send(SS_sock, cmd_buffer, MAX_BUFFER_SIZE, 0) == -1){
            perror("Error sending to SS");
            return 0;
        }

        // LOG
        char* tempCHAR_3 = strstr(cmd_buffer, "~~");
        if (tempCHAR_3 != NULL){
            *tempCHAR_3 = '\0';
        }
        printf("Sent %s to SS_%d\n", cmd_buffer, args.SS_number);
        fp = fopen("logs.txt", "a+");
        fprintf(fp, "Sent %s to SS_%d\n", cmd_buffer, args.SS_number);
        fclose(fp);
        if (tempCHAR_3 != NULL){
            *tempCHAR_3 = '~';
        }
        // LOG END

    }
    else if (command == 2){

        if (send(SS_sock, cmd_buffer, MAX_BUFFER_SIZE, 0) == -1){
            perror("Error sending to SS");
            send(args.returnToClient_FD, "Failed to copy, try again", MAX_BUFFER_SIZE, 0);
            return 0;
        }

        // LOG
        char* tempCHAR_4 = strstr(cmd_buffer, "~~");
        if (tempCHAR_4 != NULL){
            *tempCHAR_4 = '\0';
        }
        printf("Sent %s to SS_%d\n", cmd_buffer, args.SS_number);
        fp = fopen("logs.txt", "a+");
        fprintf(fp, "Sent %s to SS_%d\n", cmd_buffer, args.SS_number);
        fclose(fp);
        if (tempCHAR_4 != NULL){
            *tempCHAR_4 = '~';
        }
        // LOG END

        if (!(strncmp(cmd_buffer, "CREATEDIR ", 10))){
            insert_node(&(cmd_buffer[10]), args.SS_number, 0);
            // LOG
            printf("A. Inserting |%s| into SS_%d's Trie\n", &(cmd_buffer[10]), args.SS_number);
            fp = fopen("logs.txt", "a+");
            fprintf(fp, "A. Inserting |%s| into SS_%d's Trie\n", &(cmd_buffer[10]), args.SS_number);
            fclose(fp);
            // LOG END
        } 
        else if (!(strncmp(cmd_buffer, "CREATEFILE ", 11))){
            int cnt = 0, l = 0;
            for (l=0; l<strlen(cmd_buffer); l++){
                if (cmd_buffer[l] == ' '){
                    cnt++;
                    if (cnt > 1){
                        break;
                    }
                }
            }
            cmd_buffer[l] = '\0';
            insert_node(&(cmd_buffer[11]), args.SS_number, 1);
            // LOG
            printf("B. Inserting |%s| in SS_%d's Trie\n", &(cmd_buffer[11]), args.SS_number);
            fp = fopen("logs.txt", "a+");
            fprintf(fp, "B. Inserting |%s| in SS_%d's Trie\n", &(cmd_buffer[11]), args.SS_number);
            fclose(fp);
            cmd_buffer[l] = ' ';
            // LOG END
        }
        else if (!(strncmp(cmd_buffer, "DELETEDIR ", 10))){
            delete_node(&(cmd_buffer[10]));   
            // LOG         
            printf("Deleting |%s| from SS_%d's Trie\n", &(cmd_buffer[10]), args.SS_number);
            
            // LOG END
        } 
        else if (!(strncmp(cmd_buffer, "DELETEFILE ", 11))){
            delete_node(&(cmd_buffer[11]));
            // LOG
            printf("Deleting %s from SS_%d's Trie\n", &(cmd_buffer[11]), args.SS_number);
            fp = fopen("logs.txt", "a+");
            fprintf(fp, "Deleting %s from SS_%d's Trie\n", &(cmd_buffer[11]), args.SS_number);
            fclose(fp);
            // LOG END
        }
    }    
    else if (command == 1){
        // GIVEN: DEST PATH: args.cmd_buffer | SRC PATH: args.cmd_buffer2 | clientFD to return to | SS to get from and send to
        // Paths are in the form of: 
        int SS_from = search(args.cmd_buffer2);
        int SS_to = search(args.cmd_buffer);

        // LOG
        printf("Executing: COPY |%s| |%s|\n", args.cmd_buffer, args.cmd_buffer2);
        fp = fopen("logs.txt", "a+");
        fprintf(fp, "Executing: COPY |%s| |%s|\n", args.cmd_buffer, args.cmd_buffer2);
        fclose(fp);
        // LOG END
        // example: COPY a/b c/d/e    OR      COPY a/b r/f/k.txt
        // insert "a/b""/""e""/""other paths"   OR    "a/b""/""k.txt"
        int index = -1;
        for (int i=strlen(args.cmd_buffer2)-1; i>=0; i--){
            if (args.cmd_buffer2[i] == '/'){
                index = i;
            }
        }
        index += 2;
        char** paths = finder(args.cmd_buffer2);
        int num_paths = atoi(paths[0]);
        for (int i=1; i<=num_paths; i++){
            printf("%d: %s\n", i, paths[i]);
        }
        char* newpath = (char*)malloc(MAX_BUFFER_SIZE*sizeof(char));
        char* newpath_copy = (char*)malloc(MAX_BUFFER_SIZE*sizeof(char));
        strcpy(newpath, args.cmd_buffer);
        strcat(newpath, "/");
        strcpy(newpath_copy, newpath);
        for (int i=1; i<=num_paths; i++){
            strcat(newpath, &(paths[i][index]));
            // printf("|%s %s %s|\n", args.cmd_buffer, &(paths[i][index]) ,newpath);
            // Make changes physically by sending msg to concerned SS
            if (paths[i][0] == '?'){
                struct SS_Connection_Args newargs;
                newargs.command = 2;
                newargs.SS_number = SS_to;
                strcpy(newargs.cmd_buffer, "CREATEDIR ");
                strcat(newargs.cmd_buffer, newpath); // continue here
                pthread_t createdir_thread_ID;
                
                // LOG
                char* tempCHAR_2 = strstr(newargs.cmd_buffer, "~~");
                if (tempCHAR_2 != NULL){
                    *tempCHAR_2 = '\0';
                }
                printf("COMMAND TO SS_%d: %s\n", newargs.SS_number, newargs.cmd_buffer);
                fp = fopen("logs.txt", "a+");
                fprintf(fp, "COMMAND TO SS_%d: %s\n", newargs.SS_number, newargs.cmd_buffer);
                fclose(fp);
                if (tempCHAR_2 != NULL){
                    *tempCHAR_2 = '~';
                }
                // LOG END

                pthread_create(&createdir_thread_ID, NULL, storage_server_send, (void*)&newargs);
            }
            else if (paths[i][0] == '*'){
                struct SS_Connection_Args newargs_1;
                newargs_1.command = 2;
                newargs_1.SS_number = SS_from;
                strcpy(newargs_1.cmd_buffer, "READ ");
                strcat(newargs_1.cmd_buffer, &(paths[i][1]));
                strcat(newargs_1.cmd_buffer, " ");
                strcat(newargs_1.cmd_buffer, newpath); // continue here

                pthread_t createdir_thread_ID;

                // LOG
                char* tempCHAR_1 = strstr(newargs_1.cmd_buffer, "~~");
                if (tempCHAR_1 != NULL){
                    *tempCHAR_1 = '\0';
                }
                printf("COMMAND TO SS_%d: %s\n", newargs_1.SS_number, newargs_1.cmd_buffer);
                fp = fopen("logs.txt", "a+");
                fprintf(fp, "COMMAND TO SS_%d: %s\n", newargs_1.SS_number, newargs_1.cmd_buffer);
                fclose(fp);
                if (tempCHAR_1 != NULL){
                    *tempCHAR_1 = '~';
                }
                // LOG END

                pthread_create(&createdir_thread_ID, NULL, storage_server_send, (void*)&newargs_1);
            }

            // No need to make changes locally in the TRIE, command == 2 will do it

            strcpy(newpath, newpath_copy);

            usleep(500000); // CHANGE SLEEP HERE
        }

    }
}

void* backup_creator(void* arg);

// recieves messages from the storage server
void *storage_server_recieve(void *arg)
{
    sem_post(&GlobalSemaphore);

    pthread_t ptid;
    pthread_create(&ptid, NULL, backup_creator, NULL);

    struct SS_Connection_Args args;
    args = *((struct SS_Connection_Args *)arg);

    if (args.reestablished_Connection)
    {
        is_SS_DOWN[args.SS_number] = 0; // server is not down now, it has come back
        // Code it

    }
    else
    {
        Active_SS_count++;
        // LOG
        printf("New connection with SS_%d has been established\n", args.SS_number);
        fp = fopen("logs.txt", "a+");
        fprintf(fp, "New connection with SS_%d has been established\n", args.SS_number);
        fclose(fp);
        // LOG END
        is_SS_DOWN[args.SS_number] = 0; // it is not down
    }

    while (1)
    {
        if (client_fds[args.client_fd_ID] != -1 && FD_ISSET(client_fds[args.client_fd_ID], &readfds))
        {
            char buffer[MAX_BUFFER_SIZE];
            int bytes_received = recv(client_fds[args.client_fd_ID], buffer, sizeof(buffer), 0);
            if (bytes_received > 0)
            {
                buffer[bytes_received] = '\0'; // Null-terminate the received data
                printf("\nReceived message from SS_%d: %s\n", args.SS_number, buffer);
                if (strncmp(buffer, "PERIODIC$", 9) == 0){
                    // periodic message
                }
                else{
                    if (strstr(buffer, "~~") != NULL){
                        // OUTPUT OF CALLING READ IS TRIGGERED HERE
                        char* write_path = (char*)malloc(2*MAX_BUFFER_SIZE*sizeof(char));
                        char* tilde_index = strstr(buffer, "~~");
                        *tilde_index = '\0';

                        char* path = (char*)malloc(MAX_BUFFER_SIZE*sizeof(char));
                        strcpy(write_path, buffer);

                        int SS_sock = 0;
                        // struct sockaddr_in server_addr;
                        // if ((SS_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
                        //     perror("Socket creation failed in storage server recieve thread");
                        //     return 0;
                        // }
                        // server_addr.sin_family = AF_INET;
                        char tempChar; 
                        int flag = 0;
                        int tempIndex = -1;
                        for (int i=strlen(write_path)-1; i>=0; i--){
                            if (write_path[i] == '/'){
                                tempChar = write_path[i];
                                tempIndex = i;
                                write_path[i] = '\0';
                                flag = 1;
                                break;
                            }
                        }

                        printf("1. Searching for %s\n", write_path);
                        int x = search(write_path);
                        printf("PORT Index: %d\n", x);
                        printf("PORT GIVEN: %d\n", SS_PORTS[x]);
                        // server_addr.sin_port = htons(SS_PORTS[x]);

                        if (flag){
                            write_path[tempIndex] = tempChar;
                        }

                        strcpy(path, write_path);
                        printf("path: %s\n", write_path);

                        // if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0){
                        //     perror("Invalid address");
                        //     return 0;
                        // }

                        // if (connect(SS_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1){
                        //     perror("Connection failed");
                        //     return 0;
                        // }
                        
                        bytes_received = MAX_BUFFER_SIZE;
                        while (1){
                            if (bytes_received < MAX_BUFFER_SIZE){
                                break;
                            }
                            bytes_received = recv(client_fds[args.client_fd_ID], buffer, sizeof(buffer), 0);
                            if (bytes_received == 0){
                                printf("SS_%d disconnected\n", args.SS_number);
                                is_SS_DOWN[args.SS_number] = 1; // SS is down
                                client_fds[args.client_fd_ID] = -1;
                                close(client_fds[args.client_fd_ID]);
                                break;
                            }
                            
                            if (strstr(buffer, "~~") == NULL){
                                bytes_received = MAX_BUFFER_SIZE;
                                continue;
                            }

                            // *strstr(buffer, "~~") = '\0';
                            // write the content into the buffer
                            strcpy(write_path , "CREATEFILE ");
                            strcat(write_path, path); // adding path where writing will happen
                            strcat(write_path, " ");
                            strcat(write_path, buffer); // content
                            
                            int k = strlen(write_path);
                            for (int i=0; i<2*MAX_BUFFER_SIZE-k; i++){
                                write_path[k + i] = '~';
                            }

                            // if (send(SS_sock, write_path, MAX_BUFFER_SIZE, 0) == -1){
                            //     perror("Error sending to SS");
                            //     return 0;
                            // }
                            pthread_t pthread_ID;
                            struct SS_Connection_Args argu;
                            strcpy(argu.cmd_buffer, write_path);
                            argu.command = 2;
                            argu.SS_number = x;
                            // printf("3. Creating SS_send with SS_%d\n", argu.SS_number);
                            // LOG
                            char* tempCHAR = strstr(argu.cmd_buffer, "~~");
                            if (tempCHAR != NULL){
                                *tempCHAR = '\0';
                            }
                            printf("Writing into file %s this content\n|%s|\n", path, argu.cmd_buffer);
                            fp = fopen("logs.txt", "a+");
                            fprintf(fp, "Writing into file %s this content\n|%s|\n", path, argu.cmd_buffer);
                            fclose(fp);
                            if (tempCHAR != NULL){
                                *tempCHAR = '~';
                            }
                            // LOG END

                            pthread_create(&pthread_ID, NULL, storage_server_send, (void*)&argu);

                            int cnt = 0;
                            int o;
                            for (o=0; o<MAX_BUFFER_SIZE; o++){
                                if (argu.cmd_buffer[o] == ' '){
                                    cnt++;
                                    if (cnt > 1){
                                        break;
                                    }
                                }
                            }
                            char* charINDEX = strstr(argu.cmd_buffer, "~~");
                            if (charINDEX != NULL){
                                *charINDEX = '\0';
                            }
                            bytes_received = strlen(argu.cmd_buffer) - o;
                            *charINDEX = '~';
                        }
                        
                        free(write_path);
                        free(path);
                    }
                }
            }
            else if (bytes_received == 0)
            {
                printf("SS_%d disconnected\n", args.SS_number);
                is_SS_DOWN[args.SS_number] = 1; // SS is down
                client_fds[args.client_fd_ID] = -1;
                close(client_fds[args.client_fd_ID]);
                break;
            }

            // HANDLE SERVER REQUESTS HERE
        }
    }
}


void* client_handler(void* arg){
    int main_fd;
    struct sockaddr_in server_addr;
    pthread_t thread;

    if ((main_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket creation failed");
        return 0;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NM_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(main_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Binding failed");
        return 0;
    }

    listen(main_fd, MAX_CLIENTS);

    int client_socket[MAX_CLIENTS];
    for (int i=0; i<MAX_CLIENTS; i++){
        client_socket[i] = 0;
    }

    char buffer[MAX_BUFFER_SIZE];

    while(1){   
        FD_ZERO(&client_fds_thread);   

        FD_SET(main_fd, &client_fds_thread);   
        int max_sd = main_fd;   
             
        int sd;  
        for (int i=0 ; i<MAX_CLIENTS ; i++)   
        {   
            sd = client_socket[i];            
            if(sd > 0)   
                FD_SET(sd , &client_fds_thread);   
                 
            if(sd > max_sd){
                max_sd = sd;   
            }   
        }   
     
        // wait indefinitely for activity on one of the FDs
        int activity = select(max_sd+1 , &client_fds_thread , NULL , NULL , NULL);   
       
        if ((activity < 0))   
        {   
            printf("select error");   
        }   

        int new_socket;
        int addrlen = sizeof(server_addr);
        //If something happened on the master socket, then its an incoming connection  
        if (FD_ISSET(main_fd, &client_fds_thread))   
        {   
            if ((new_socket = accept(main_fd, (struct sockaddr *)&server_addr, (socklen_t*)&addrlen))<0)   {   
                perror("accept");   
                exit(EXIT_FAILURE);   
            }   
             
            printf("New connection , socket fd is %d\n", new_socket);     
                                 
            for (int i=0; i<MAX_CLIENTS; i++){   
                if( client_socket[i] == 0 )   
                {   
                    client_socket[i] = new_socket;   
                    break;   
                }   
            }   
        }
             
        int valread;
        for (int i=0; i<MAX_CLIENTS; i++)   
        {   
            sd = client_socket[i];   
            if (FD_ISSET(sd , &client_fds_thread))   
            {   
                // activity detected
                if ((valread = read( sd , buffer, MAX_BUFFER_SIZE)) == 0)   
                {   
                    // disconnected
                    printf("Client with fd %d disconnected\n", sd);           
                    client_socket[i] = 0;   
                    close(sd);   
                }   
                     
                else {   
                    buffer[valread] = '\0';
                    printf("MESSAGE by Client with fd %d: %s\n", sd, buffer); 
                    // fflush(stdout);
                }   
            }
        }   
    }   
}

void* backup_Handler(void* arg){
    // Handle the backup
    struct BackupArgs* bckargs = (struct BackupArgs*)arg;
    int SS_number = bckargs->SS_number; // NEEDED
    int SS_to = bckargs->SS_to; // NEEDED

    char argscmd_buffer2[MAX_BUFFER_SIZE];
    strcpy(argscmd_buffer2, "");
    char argscmd_buffer[MAX_BUFFER_SIZE];
    strcpy(argscmd_buffer, "$");
    snprintf(argscmd_buffer, MAX_BUFFER_SIZE, "$%d%d", SS_number, SS_to);

    insert_node(argscmd_buffer, SS_to, 0);
    
    char** paths = finderByData(SS_number);
    int num_paths = atoi(paths[0]);
    char* newpath = (char*)malloc(MAX_BUFFER_SIZE*sizeof(char));
    char* newpath_copy = (char*)malloc(MAX_BUFFER_SIZE*sizeof(char));
    strcpy(newpath, argscmd_buffer);
    strcat(newpath, "/");
    strcpy(newpath_copy, newpath);
    strcpy(argscmd_buffer2, "CREATEDIR ");
    strcat(argscmd_buffer2, argscmd_buffer);
    struct SS_Connection_Args newarg;
    newarg.command = 2;
    newarg.SS_number = SS_to;
    strcpy(newarg.cmd_buffer, argscmd_buffer2);
    pthread_t pthread_ID;
    printf("TO SS_%d:\n", newarg.SS_number);
    printf("SEND: %s\n", newarg.cmd_buffer);
    printf("1. %s\n", newarg.cmd_buffer);
    pthread_create(&pthread_ID, NULL, storage_server_send, (void*)&newarg);    

    sleep(1);

    for (int i=1; i<=num_paths; i++){
        strcat(newpath, &(paths[i][1]));

        if (paths[i][0] == '?'){
            struct SS_Connection_Args newargs;
            newargs.command = 2;
            newargs.SS_number = SS_to;
            strcpy(newargs.cmd_buffer, "CREATEDIR ");
            strcat(newargs.cmd_buffer, newpath);
            printf("SEND: %s\n", newargs.cmd_buffer);
            pthread_t tid_1;
            printf("2. %s\n", newargs.cmd_buffer);
            pthread_create(&tid_1, NULL, storage_server_send, (void*)&newargs);
        }
        else if (paths[i][0] == '*'){
            struct SS_Connection_Args newargs_1;
            newargs_1.command = 2;
            newargs_1.SS_number = SS_number;
            strcpy(newargs_1.cmd_buffer, "READ ");
            strcat(newargs_1.cmd_buffer, &(paths[i][1]));
            strcat(newargs_1.cmd_buffer, " ");
            strcat(newargs_1.cmd_buffer, newpath); // continue here
            // strcat(newargs_1.cmd_buffer, );
            pthread_t createdir_thread_ID;

            // LOG
            char* tempCHAR_1 = strstr(newargs_1.cmd_buffer, "~~");
            if (tempCHAR_1 != NULL){
                *tempCHAR_1 = '\0';
            }
            printf("COMMAND TO SS_%d: %s\n", newargs_1.SS_number, newargs_1.cmd_buffer);
            fp = fopen("logs.txt", "a+");
            fprintf(fp, "COMMAND TO SS_%d: %s\n", newargs_1.SS_number, newargs_1.cmd_buffer);
            fclose(fp);
            if (tempCHAR_1 != NULL){
                *tempCHAR_1 = '~';
            }
            // LOG END

            pthread_create(&createdir_thread_ID, NULL, storage_server_send, (void*)&newargs_1);

            // struct SS_Connection_Args newargs;
            // newargs.command = 2;
            // newargs.SS_number = SS_to;
            // strcpy(newargs.cmd_buffer, "CREATEFILE ");
            // strcat(newargs.cmd_buffer, newpath);
            // printf("SEND: %s\n", newargs.cmd_buffer);
            // pthread_t tid_2;
            // pthread_create(&tid_2, NULL, storage_server_send, (void*)&newargs);
        }

        strcpy(newpath, newpath_copy);
        sleep(1);
    }
}

void* backup_creator(void* arg){
    if (Active_SS_count < 3){
        return 0;
    }

    int SS_to = -1;
    int SS_from = -1;
    int tempIndex;
    for (int i=0; i<Active_SS_count; i++){
        if (Has_Been_Backed_Up[STORAGE_SERVER_NUM[i]][0] && Has_Been_Backed_Up[STORAGE_SERVER_NUM[i]][1]){
            continue;
        }

        if (!(Has_Been_Backed_Up[STORAGE_SERVER_NUM[i]][0])){
            SS_from = STORAGE_SERVER_NUM[i];
            tempIndex = i-1;
            if (tempIndex < 0){
                tempIndex+=Active_SS_count;
            }
            SS_to = STORAGE_SERVER_NUM[tempIndex];

            pthread_t tid;
            struct BackupArgs bckargs;
            bckargs.SS_number = SS_from;
            bckargs.SS_to = SS_to;
            pthread_create(&tid, NULL, backup_Handler, (void*)&bckargs);
            Has_Been_Backed_Up[STORAGE_SERVER_NUM[i]][0] = SS_to;
            pthread_join(tid, NULL);
        }

        if (!(Has_Been_Backed_Up[STORAGE_SERVER_NUM[i]][1])){
            SS_from = STORAGE_SERVER_NUM[i];
            tempIndex = i-2;
            if (tempIndex < 0){
                tempIndex+=Active_SS_count;
            }
            SS_to = STORAGE_SERVER_NUM[tempIndex];

            pthread_t tid;
            struct BackupArgs bckargs;
            bckargs.SS_number = SS_from;
            bckargs.SS_to = SS_to;
            pthread_create(&tid, NULL, backup_Handler, (void*)&bckargs);
            Has_Been_Backed_Up[STORAGE_SERVER_NUM[i]][1] = SS_to;
            pthread_join(tid, NULL);
        }
    }    
}

void *handleConnections(void *arg)
{
    struct ThreadArgs *args = (struct ThreadArgs *)arg;
    int master_fd = args->master_fd;

    int max_fd = master_fd;

    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        client_fds[i] = -1; // Initialize client socket array
    }

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(master_fd, &readfds);

        for (int i = 0; i < MAX_CONNECTIONS; i++)
        {
            if (client_fds[i] != -1)
            {
                FD_SET(client_fds[i], &readfds);
                if (client_fds[i] > max_fd)
                {
                    max_fd = client_fds[i];
                }
            }
        }

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0)
        {
            perror("Select error");
            break;
        }

        if (FD_ISSET(master_fd, &readfds))
        {
            int new_fd;
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            new_fd = accept(master_fd, (struct sockaddr *)&client_addr, &addr_len);

            int initiallyLostConnection = 0;
            int new_connection = -1;
            if (new_fd != -1)
            {
                for (int i = 0; i < MAX_CONNECTIONS; i++)
                {
                    if (client_fds[i] == -1)
                    {
                        client_fds[i] = new_fd;
                        if (client_fds[i] > max_fd)
                        {           
                            max_fd = client_fds[i];
                        }
                        new_connection = i;
                        break;
                    }
                }
            }

            pthread_t ephemeral_client_thread;
            if (new_connection != -1)
            {
                if (client_fds[new_connection] != -1)
                {
                    char *buffer = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
                    int bytes_received = recv(client_fds[new_connection], buffer, MAX_BUFFER_SIZE, 0);
                    // fflush(stdout);
                    if (bytes_received > 0)
                    {
                        buffer[bytes_received] = '\0';
                        fp = fopen("logs.txt", "a+");
                        printf("MESSAGE BY CLIENT/NEW_SS: %s\n", buffer);
                        fprintf(fp, "MESSAGE BY CLIENT/NEW_SS: %s\n", buffer);
                        fclose(fp);
                        if (buffer[0] == '$')
                        {
                            // create new SS thread
                            // printf("BUFFER: %s\n", buffer);
                            // printf("%s\n", buffer);
                            // fflush(stdout);
                            char *token = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
                            token = strtok(buffer, "\n");
                            // printf("START:\n%s\n", token);
                            struct SS_Connection_Args SS_CArgs;
                            SS_CArgs.SS_number = atoi(&token[1]);

                            token = strtok(NULL, "\n");
                            // printf("%s\n", token);
                            // fflush(stdout);
                            if (token != NULL)
                            {
                                strcpy(SS_IP_ADDRs[SS_CArgs.SS_number], token);
                            }
                            token = strtok(NULL, "\n");
                            if (token != NULL){
                                SS_PORTS[SS_CArgs.SS_number] = atoi(token);
                                // LOG
                                printf("SS_PORTS[%d] = %d\n", SS_CArgs.SS_number, atoi(token));
                                fp = fopen("logs.txt", "a+");
                                fprintf(fp, "SS_PORTS[%d] = %d\n", SS_CArgs.SS_number, atoi(token));
                                fclose(fp);
                                // LOG END
                            }
                            token = strtok(NULL, "\n");
                            char* newtoken;
                            if (token != NULL){
                                newtoken = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
                                SS_PATH_COUNT[SS_CArgs.SS_number] = 0;
                                newtoken = strtok(token, " ");
                                strcpy(SS_PATHS[SS_CArgs.SS_number][SS_PATH_COUNT[SS_CArgs.SS_number]], &(newtoken[1]));
                                SS_PATH_COUNT[SS_CArgs.SS_number]++;
                                if (newtoken[0] == '*'){
                                    insert_node(&(newtoken[1]), SS_CArgs.SS_number, 1);
                                    // LOG
                                    printf("C. Inserting |%s|\n", newtoken);
                                    // LOG END
                                }
                                else if (newtoken[0] == '?'){
                                    insert_node(&(newtoken[1]), SS_CArgs.SS_number, 0);
                                    // LOG
                                    printf("D. Inserting |%s|\n", newtoken);
                                    // LOG END
                                }
                                while (1)
                                {
                                    newtoken = strtok(NULL, " ");
                                    if (newtoken == NULL)
                                    {
                                        break;
                                    }
                                    strcpy(SS_PATHS[SS_CArgs.SS_number][SS_PATH_COUNT[SS_CArgs.SS_number]], &(newtoken[1]));
                                    SS_PATH_COUNT[SS_CArgs.SS_number]++;
                                    // ADD PATHS TO TRIE HERE
                                    if (newtoken[0] == '*'){
                                        insert_node(&(newtoken[1]), SS_CArgs.SS_number, 1);
                                        printf("E. Inserting |%s|\n", newtoken);
                                    
                                    }
                                    else if (newtoken[0] == '?'){
                                        insert_node(&(newtoken[1]), SS_CArgs.SS_number, 0);
                                        printf("F. Inserting |%s|\n", newtoken);
                                    }
                                }
                            }

                            int isReestablished = 0;
                            for (int i = 0; i < NUM_STORAGE_SERVERS; i++)
                            {
                                if (STORAGE_SERVER_NUM[i] == SS_CArgs.SS_number)
                                {
                                    printf("Connection with SS_%d has been reestablished\n", SS_CArgs.SS_number);
                                    fp = fopen("logs.txt", "a+");
                                    fprintf(fp, "Connection with SS_%d has been reestablished\n", SS_CArgs.SS_number);
                                    fclose(fp);
                                    isReestablished = 1;
                                    break;
                                }
                            }
                            if (isReestablished)
                            {
                                SS_CArgs.reestablished_Connection = 1;
                                SS_CArgs.client_fd_ID = new_connection;
                                // printf("Creating SS_recieve for %d\n", SS_CArgs.SS_number);
                                pthread_create(&(STORAGE_SERVER_THREAD_IDS[SS_CArgs.SS_number]), NULL, storage_server_recieve, (void *)&SS_CArgs);
                            }
                            else
                            {
                                STORAGE_SERVER_NUM[NUM_STORAGE_SERVERS] = SS_CArgs.SS_number;
                                NUM_STORAGE_SERVERS++;
                                SS_CArgs.reestablished_Connection = 0;
                                SS_CArgs.client_fd_ID = new_connection;
                                // printf("Creating SS_recieve for %d\n", SS_CArgs.SS_number);
                                pthread_create(&(STORAGE_SERVER_THREAD_IDS[SS_CArgs.SS_number]), NULL, storage_server_recieve, (void *)&SS_CArgs);
                            }
                        }
                        else if (!(strncmp(buffer, "READ", 4)) || !(strncmp(buffer, "WRITE", 4)) || !(strncmp(buffer, "GETINFO", 4)))
                        {
                            // client call, give it the path
                            char *path;
                            strtok(buffer , " ");
                            path = strtok(NULL , " ");

                            char *returnPath = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
                            printf("2. Searching for |%s|  |%d|\n", path , search(path));
                            snprintf(returnPath, MAX_BUFFER_SIZE, "%d", SS_PORTS[search(path)] + 1);
                            send(client_fds[new_connection], returnPath, MAX_BUFFER_SIZE, 0);
                            // pthread_create(&ephemeral_client_thread, NULL, client_handler, );
                            free(returnPath);
                        }
                        else if (!(strncmp(buffer, "CREATEDIR ", 10)) || !(strncmp(buffer, "DELETEDIR ", 10)) || !(strncmp(buffer, "DELETEFILE ", 11)) || !(strncmp(buffer, "CREATEFILE ", 11)))
                        {
                            // A CLIENT WANTS TO CREATE/DELETE A FILE/DIRECTORY
                            char *path;
                            struct SS_Connection_Args ssAr;
                            strcpy(ssAr.cmd_buffer, buffer);
                            ssAr.command = 2;
                            ssAr.returnToClient_FD = client_fds[new_connection];
                            if (buffer[0] == 'D' && buffer[6] == 'D')
                            {
                                printf("3. Searching for %s\n", &(buffer[10]));
                                ssAr.SS_number = search(&buffer[10]);
                                ssAr.command = 2;
                            }
                            else if (buffer[0] == 'D' && buffer[6] == 'F')
                            {
                                printf("3. Searching for %s\n", &(buffer[11]));
                                ssAr.SS_number = search(&buffer[11]);
                                ssAr.command = 2;
                            }
                            else if (buffer[0] == 'C' && buffer[6] == 'F')
                            {

                                char str[2048];
                                strcpy(str, buffer);
                                strtok(str, " ");
                                path = strtok(NULL, " ");
                                for (int l = strlen(path) - 1; l > 0; l--)
                                {
                                    if (path[l] == '/')
                                    {
                                        path[l] = '\0';
                                        break;
                                    }
                                }
                                printf("4. Searching for %s\n", path);
                                ssAr.SS_number = search(path);
                                ssAr.command = 2;
                                strcat(ssAr.cmd_buffer, " @");
                            }
                            else if (buffer[0] == 'C', buffer[6] == 'D')
                            {
                                for (int l = strlen(buffer) - 1; l > 0; l--)
                                {
                                    if (buffer[l] == '/')
                                    {
                                        buffer[l] = '\0';
                                        break;
                                    }
                                }
                                printf("4. Searching for %s\n", buffer);
                                ssAr.SS_number = search(buffer + 10);
                                ssAr.command = 2;
                            }

                            if (ssAr.SS_number == -1)
                            {
                                printf("Failed to find %s\n", path);
                            }
                            else
                            {
                                pthread_t cmd_2_threadID;
                                pthread_create(&cmd_2_threadID, NULL, storage_server_send, (void *)&ssAr);
                            }
                        }
                        else if (!(strncmp(buffer, "COPY ", 5))){
                            char *path1, *path2;
                            // Allocate memory for path1 and path2 using the updated MAX_BUFFER_SIZE macro
                            path1 = (char *)malloc(1024 * sizeof(char));
                            path2 = (char *)malloc(1024 * sizeof(char));
                            int SS_num1, SS_num2;
                            // Use sscanf to extract paths after the "copy" token and allocate memory dynamically
                            if (sscanf(buffer, "COPY %1023s %1023s", path1, path2) == 2) {
                                // printf("5. Searching for |%s|\n", path1);
                                SS_num1 = search(path1);
                                // printf("6. Searching for %s\n", path2);
                                SS_num2 = search(path2);

                                if (SS_num1 == -1 || SS_num2 == -1){
                                    printf("Invalid paths given. Please try again.\n");
                                }
                                else if (SS_num1 == SS_num2){
                                    struct SS_Connection_Args ssArgues;
                                    ssArgues.SS_number = SS_num1;
                                    ssArgues.command = 3;
                                    ssArgues.returnToClient_FD = client_fds[new_connection];
                                    strcpy(ssArgues.cmd_buffer, path1);
                                    strcpy(ssArgues.cmd_buffer2, path2);
                                    pthread_t copy_dir_threadID;
                                    int v = search2(path2);
                                    if (v == 0){
                                        strcpy(buffer, "COPYDIR ");
                                        strcat(buffer, path1);
                                        strcat(buffer, " ");
                                        strcat(buffer, path2);
                                    }
                                    else if (v == 1){
                                        strcpy(buffer, "COPYFILE ");
                                        strcat(buffer, path1);
                                        strcat(buffer, " ");
                                        strcat(buffer, path2);
                                    }
                                    strcpy(ssArgues.cmd_buffer, buffer);
                                    // printf("5. Creating SS_send with SS_%d\n", ssArgues.SS_number);
                                    pthread_create(&copy_dir_threadID, NULL, storage_server_send, (void*)&ssArgues);
                                }
                                else if (SS_num1 != SS_num2){
                                    if (is_SS_DOWN[SS_num1] || is_SS_DOWN[SS_num2]){
                                        // either one or both servers are down
                                        printf("Atleast one SS is down right now, try again later.\n");
                                    }
                                    else if (is_SS_DOWN[SS_num1] == 0 && is_SS_DOWN[SS_num2] == 0){
                                        // do the procedure
                                        char* tempBUF = (char*)malloc(MAX_BUFFER_SIZE*sizeof(char));
                                        struct SS_Connection_Args ssArgue;
                                        ssArgue.returnToClient_FD = client_fds[new_connection];
                                        pthread_t copy_cross_platform_ID;
                                        strcpy(ssArgue.cmd_buffer, path1); // destination
                                        strcpy(ssArgue.cmd_buffer2, path2); // source
                                        ssArgue.command = 1;
                                        // ssArgue.SS_number = // HEREBOI
                                        // file uthani hai, non blocking
                                        // printf("6. Creating SS_send with SS_%d\n", ssArgue.SS_number);
                                        pthread_create(&copy_cross_platform_ID, NULL, storage_server_send, (void*)&ssArgue);
                                        free(tempBUF);
                                    }
                                }
                            } 
                            else{
                                printf("Invalid command format.\n");
                            }
                        }
                    }
                    else if (bytes_received == 0)
                    {
                        printf("Failed to establish connection\n");
                        close(client_fds[new_connection]);
                        client_fds[new_connection] = -1;
                    }
                    free(buffer);
                }
            }
        }
    }

    close(master_fd);
    return NULL;
}

int main()
{
    fp = fopen("logs.txt", "w");
    fprintf(fp, "LOGS START\n");
    fclose(fp);
    int master_fd;
    struct sockaddr_in server_addr;
    pthread_t thread;


    if ((master_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NM_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(master_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Binding failed");
        return 1;
    }

    listen(master_fd, MAX_CONNECTIONS);

    struct ThreadArgs thread_args;
    thread_args.master_fd = master_fd;

    if (pthread_create(&thread, NULL, handleConnections, &thread_args) != 0)
    {
        perror("Thread creation failed");
        return 1;
    }

    char buff[MAX_BUFFER_SIZE];
    
    for (int i=0; i<MAX_STORAGE_SERVERS; i++){
        Has_Been_Backed_Up[i][0] = 0;
        Has_Been_Backed_Up[i][1] = 0;
    }
    // while (1){
    //     printf("Enter path to search for SS Number: ");
    //     scanf("%s", buff);
    //     // printf("Present in SS_%d\n\n", search(buff));
    //     pthread_t pid;
    //     struct SS_Connection_Args ssArgs;
    //     printf("7. Searching for %s\n", buff);
    //     ssArgs.SS_number = search(buff); 
    //     printf("7. Creating SS_send with SS_%d\n", ssArgs.SS_number);
    //     pthread_create(&pid, NULL, storage_server_send, (void*)&ssArgs);
    //     sleep(2);
    // }
    // sleep(10);

    // struct BackupArgs bckarg;
    // bckarg.SS_number = 2;
    // bckarg.SS_to = 1;
    // pthread_t bck_pid;
    // pthread_create(&bck_pid, NULL, backup_creator, (void*)&bckarg);

    pthread_join(thread, NULL);

    return 0;
}

// trie --------------------------------------------------------------------

struct node *create_node()
{
    struct node *q = (struct node *)malloc(sizeof(struct node));
    int x;
    for (x = 0; x < ALPHABET_SIZE; x++)
        q->link[x] = NULL;
    q->data = -1;
    q->type = -1;
    return q;
}

void insert_node(char key[], int data, int type)
{
    int length = strlen(key);
    int index;
    int level = 0;
    if (root == NULL)
        root = create_node();
    struct node *q = root; // For insertion of each String key, we will start from the root

    for (; level < length; level++)
    {
        if (key[level] >= 'a' && key[level] <= 'z')
        {
            index = key[level] - 'a';
        }
        else if (key[level] >= '0' && key[level] <= '9')
        {
            index = key[level] - '0' + 26;
        }
        else if (key[level] == '/')
        {
            index = 36;
        }
        else if (key[level] == '.')
        {
            index = 37;
        }
        else if (key[level] == '$')
        {
            index = 38;
        }
        else if (key[level] >= 'A' && key[level] <= 'Z')
        {
            index = key[level] - 'A' + 39;
        }
        if (q->link[index] == NULL)
        {
            q->link[index] = create_node(); // which is : struct node *p = create_node(); q->link[index] = p;
        }

        q = q->link[index];
    }
    q->data = data;
    q->type = type;
}

int search(char key[])
{
    // check if the path is in LRU cache
    printf("Checking in LRU cache...\n");
    struct LRU_node *ptr = head;
    while (ptr != NULL)
    {
        if (strcmp(ptr->path, key) == 0)
        {
            // move the node to the front of the LRU cache
            if (ptr != head)
            {
                if (ptr == tail)
                {
                    tail = ptr->prev;
                }
                ptr->prev->next = ptr->next;
                if (ptr->next != NULL)
                {
                    ptr->next->prev = ptr->prev;
                }
                ptr->next = head;
                ptr->prev = NULL;
                head->prev = ptr;
                head = ptr;
            }
            printf("LRU cache Hit\n");
            return ptr->data;
        }
        ptr = ptr->next;
    }
    printf("LRU cache Miss\n");

    if(root == NULL)
        return -1;

    struct node *q = root;
    int length = strlen(key);
    int level = 0;
    for (; level < length; level++)
    {
        int index;
        if (key[level] >= 'a' && key[level] <= 'z')
        {
            index = key[level] - 'a';
        }
        else if (key[level] >= '0' && key[level] <= '9')
        {
            index = key[level] - '0' + 26;
        }
        else if (key[level] == '/')
        {
            index = 36;
        }
        else if (key[level] == '.')
        {
            index = 37;
        }
        else if (key[level] == '$')
        {
            index = 38;
        }
        else if (key[level] >= 'A' && key[level] <= 'Z')
        {
            index = key[level] - 'A' + 39;
        }
        if (q->link[index] != NULL)
            q = q->link[index];
        else
            break;
    }
    if (key[level] == '\0' && q->data != -1)
    {
        // add the path to the start of LRU cache
        // if the LRU cache is full, remove the last node
        if (LRU_size == LRU_capacity)
        {
            struct LRU_node *temp = tail;
            tail = tail->prev;
            tail->next = NULL;
            free(temp);
            LRU_size--;
        }
        struct LRU_node *newNode = malloc(sizeof(struct LRU_node));
        newNode->path = malloc((strlen(key) + 1) * sizeof(char));
        strcpy(newNode->path, key);
        newNode->data = q->data;
        newNode->type = q->type;
        newNode->next = head;
        newNode->prev = NULL;
        if (head != NULL)
        {
            head->prev = newNode;
        }
        head = newNode;
        if (tail == NULL)
        {
            tail = newNode;
        }
        LRU_size++;
        return q->data;
    }
    return -1;
}


int search2(char key[])
{
    // check if the path is in LRU cache
    printf("Checking in LRU cache...\n");
    struct LRU_node *ptr = head;
    while (ptr != NULL)
    {
        if (strcmp(ptr->path, key) == 0)
        {
            // move the node to the front of the LRU cache
            if (ptr != head)
            {
                if (ptr == tail)
                {
                    tail = ptr->prev;
                }
                ptr->prev->next = ptr->next;
                if (ptr->next != NULL)
                {
                    ptr->next->prev = ptr->prev;
                }
                ptr->next = head;
                ptr->prev = NULL;
                head->prev = ptr;
                head = ptr;
            }
            printf("LRU cache Hit\n");
            return ptr->type;
        }
        ptr = ptr->next;
    }
    printf("LRU cache Miss\n");

    if(root == NULL)
        return -1;

    struct node *q = root;
    int length = strlen(key);
    int level = 0;
    for (; level < length; level++)
    {
        int index;
        if (key[level] >= 'a' && key[level] <= 'z')
        {
            index = key[level] - 'a';
        }
        else if (key[level] >= '0' && key[level] <= '9')
        {
            index = key[level] - '0' + 26;
        }
        else if (key[level] == '/')
        {
            index = 36;
        }
        else if (key[level] == '.')
        {
            index = 37;
        }
        else if (key[level] == '$')
        {
            index = 38;
        }
        else if (key[level] >= 'A' && key[level] <= 'Z')
        {
            index = key[level] - 'A' + 39;
        }
        if (q->link[index] != NULL)
            q = q->link[index];
        else
            break;
    }
    if (key[level] == '\0' && q->data != -1)
    {
        // add the path to the start of LRU cache
        // if the LRU cache is full, remove the last node
        if (LRU_size == LRU_capacity)
        {
            struct LRU_node *temp = tail;
            tail = tail->prev;
            tail->next = NULL;
            free(temp);
            LRU_size--;
        }
        struct LRU_node *newNode = malloc(sizeof(struct LRU_node));
        newNode->path = malloc((strlen(key) + 1) * sizeof(char));
        strcpy(newNode->path, key);
        newNode->data = q->data;
        newNode->type = q->type;
        newNode->next = head;
        newNode->prev = NULL;
        if (head != NULL)
        {
            head->prev = newNode;
        }
        head = newNode;
        if (tail == NULL)
        {
            tail = newNode;
        }
        LRU_size++;
        return q->type;
    }
    return -1;
}

// given a path it should return all directories and files present in it
// preceed the file name by *
// preceed the folder name by ?
void findAllPaths(struct node *ptr, char *path, int pathLen, char **result, int *resultLen)
{
    if (ptr == NULL)
    {
        return;
    }

    if (ptr->data != -1)
    {
        path[pathLen] = '\0';
        char *prefix = (ptr->type == 0) ? "?" : "*";
        char *fullPath = malloc((strlen(path) + 2) * sizeof(char));
        strcpy(fullPath, prefix);
        strcat(fullPath, path);
        result[*resultLen] = fullPath;
        (*resultLen)++;
    }

    int i;
    for (i = 0; i < ALPHABET_SIZE; i++)
    {
        if (ptr->link[i] != NULL)
        {
            if (i < 26)
            {
                path[pathLen] = i + 'a';
            }
            else if (i < 36)
            {
                path[pathLen] = i - 26 + '0';
            }
            else if (i == 36)
            {
                path[pathLen] = '/';
            }
            else if (i == 37)
            {
                path[pathLen] = '.';
            }
            else if (i == 38)
            {
                path[pathLen] = '$';
            }
            else if (i >= 39 && i < 65)
        {
            path[pathLen] = i - 39 + 'A';
        }
            findAllPaths(ptr->link[i], path, pathLen + 1, result, resultLen);
        }
    }
}

int count_slashes(const char *str)
{
    int count = 0;
    while (*str)
    {
        if (*str == '/')
        {
            count++;
        }
        str++;
    }
    return count;
}

int compare_by_slash(const void *a, const void *b)
{
    char *strA = *(char **)a;
    char *strB = *(char **)b;
    int countA = count_slashes(strA);
    int countB = count_slashes(strB);
    return countA - countB;
}

char **finder(char key[])
{
    char path[100]; // assuming max path length is 100
    strncpy(path, key, strlen(key));
    char **result = malloc(101 * sizeof(char *)); // assuming max 100 results + 1 for the count
    int resultLen = 0;

    struct node *ptr = root;
    int length = strlen(key);
    int level = 0;
    for (; level < length; level++)
    {
        int index;
        if (key[level] >= 'a' && key[level] <= 'z')
        {
            index = key[level] - 'a';
        }
        else if (key[level] >= '0' && key[level] <= '9')
        {
            index = key[level] - '0' + 26;
        }
        else if (key[level] == '/')
        {
            index = 36;
        }
        else if (key[level] == '.')
        {
            index = 37;
        }
        else if (key[level] == '$')
        {
            index = 38;
        }
        else if (key[level] >= 'A' && key[level] <= 'Z')
        {
            index = key[level] - 'A' + 39;
        }
        if (ptr->link[index] != NULL)
            ptr = ptr->link[index];
        else
            break;
    }

    findAllPaths(ptr, path, strlen(key), result, &resultLen);

    // Add the number of paths found as the first string in the result array
    char *numPaths = malloc(10 * sizeof(char)); // assuming max 999999999 paths
    sprintf(numPaths, "%d", resultLen);
    for (int i = resultLen; i > 0; i--)
    {
        result[i] = result[i - 1];
    }
    result[0] = numPaths;
    qsort(result + 1, resultLen, sizeof(char *), compare_by_slash);
    return result;
}

void findAllPathsByData(struct node *ptr, char *path, int pathLen, int data, char **result, int *resultLen)
{
    if (ptr == NULL)
    {
        return;
    }

    if (ptr->data == data)
    {
        path[pathLen] = '\0';
        char *prefix = (ptr->type == 0) ? "?" : "*";
        char *fullPath = malloc((strlen(path) + 2) * sizeof(char));
        strcpy(fullPath, prefix);
        strcat(fullPath, path);
        result[*resultLen] = fullPath;
        (*resultLen)++;
    }

    int i;
    for (i = 0; i < ALPHABET_SIZE - 1; i++)
    {
        if (ptr->link[i] != NULL)
        {
            if (i < 26)
            {
                path[pathLen] = i + 'a';
            }
            else if (i < 36)
            {
                path[pathLen] = i - 26 + '0';
            }
            else if (i == 36)
            {
                path[pathLen] = '/';
            }
            else if (i == 37)
            {
                path[pathLen] = '.';
            }
            // else if (i == 38)
            // {
            //     path[pathLen] = '$';
            // }
            else if (i >= 39 && i < 65)
            {
                path[pathLen] = i - 39 + 'A';
            }
            findAllPathsByData(ptr->link[i], path, pathLen + 1, data, result, resultLen);
        }
    }
}

char **finderByData(int data)
{
    char path[100]; // assuming max path length is 100
    char **result = malloc(101 * sizeof(char *)); // assuming max 100 results + 1 for the count
    int resultLen = 0;

    findAllPathsByData(root, path, 0, data, result, &resultLen);

    // Add the number of paths found as the first string in the result array
    char *numPaths = malloc(10 * sizeof(char)); // assuming max 999999999 paths
    sprintf(numPaths, "%d", resultLen);
    for (int i = resultLen; i > 0; i--)
    {
        result[i] = result[i - 1];
    }
    result[0] = numPaths;
    qsort(result + 1, resultLen, sizeof(char *), compare_by_slash);
    return result;
}

void mark_as_deleted(struct node *ptr)
{
    int i;
    for (i = 0; i < ALPHABET_SIZE; i++)
    {
        if (ptr->link[i] != NULL)
        {
            mark_as_deleted(ptr->link[i]);
        }
    }
    ptr->data = -1;
    ptr->type = -1;
}

int delete_node(char key[])
{
    struct node *q = root;
    int length = strlen(key);
    int level = 0;
    for (; level < length; level++)
    {
        int index;
        if (key[level] >= 'a' && key[level] <= 'z')
        {
            index = key[level] - 'a';
        }
        else if (key[level] >= '0' && key[level] <= '9')
        {
            index = key[level] - '0' + 26;
        }
        else if (key[level] == '/')
        {
            index = 36;
        }
        else if (key[level] == '.')
        {
            index = 37;
        }
        else if (key[level] == '$')
        {
            index = 38;
        }
        else if (key[level] >= 'A' && key[level] <= 'Z')
        {
            index = key[level] - 'A' + 39;
        }
        if (q->link[index] != NULL)
            q = q->link[index];
        else
            break;
    }
    if (key[level] == '\0' && q->data != -1)
    {
        mark_as_deleted(q);
        return 0;
    }
    return -1;
}
// --------------------------------------------------------------------------