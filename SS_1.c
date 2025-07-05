#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h> // For directory handling
#include "headers.h"

// #define NM_PORT 8080
#define ID 1
#define DID "$1"

#define CLIENT_PORT (8080 + 2 * ID)
#define NEW_PORT (8080 + 2 * ID - 1)
#define MAX_CLIENTS 20
#define MAX_BUFFER_SIZE 1024
#define SUPER_BUFFER_SIZE 16384

int NM_periodic_FD;
fd_set client_fds;

int writer;
struct call_stuff
{
    int sd;
    char *call_data;
};

void copyFile(const char *srcPath, const char *destPath)
{
    FILE *srcFile, *destFile;
    char buffer[1024];
    size_t bytesRead;

    srcFile = fopen(srcPath, "rb");
    if (srcFile == NULL)
    {
        perror("Error opening source file");
        exit(EXIT_FAILURE);
    }

    destFile = fopen(destPath, "wb");
    if (destFile == NULL)
    {
        fclose(srcFile);
        perror("Error opening destination file");
        exit(EXIT_FAILURE);
    }

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), srcFile)) > 0)
    {
        fwrite(buffer, 1, bytesRead, destFile);
    }

    fclose(srcFile);
    fclose(destFile);
}

void copyDirectory(const char *source, const char *destination)
{
    DIR *dir;
    struct dirent *entry;
    struct stat statBuf;
    if (stat(destination, &statBuf) == -1)
    {
        mkdir(destination, 0777);
    }
    // Open the source directory
    if (!(dir = opendir(source)))
    {
        perror("opendir");
        // exit(EXIT_FAILURE);
    }

    // Create the destination directory if it doesn't exist

    // Traverse the source directory
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Construct the full path of the current entry in the source directory
        char sourcePath[PATH_MAX];
        snprintf(sourcePath, sizeof(sourcePath), "%s/%s", source, entry->d_name);

        // Construct the full path of the corresponding entry in the destination directory
        char destinationPath[PATH_MAX];
        snprintf(destinationPath, sizeof(destinationPath), "%s/%s", destination, entry->d_name);

        // Check if the current entry is a directory
        if (stat(sourcePath, &statBuf) == 0 && S_ISDIR(statBuf.st_mode))
        {
            // Recursively copy the subdirectory
            copyDirectory(sourcePath, destinationPath);
        }
        else
        {
            // Copy the file
            copyFile(sourcePath, destinationPath);
        }
    }

    // Close the source directory
    closedir(dir);
}

// CHATGPT prompt : If i want to delete an entire directory using C code, knowing its path, how?
int remove_directory(const char *path)
{
    DIR *d = opendir(path);
    size_t path_len = strlen(path);
    int r = -1;

    if (d)
    {
        struct dirent *p;
        r = 0;
        while (!r && (p = readdir(d)))
        {
            int r2 = -1;
            char *buf;
            size_t len;

            // Skip current and parent directories
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
            {
                continue;
            }

            len = path_len + strlen(p->d_name) + 2;
            buf = malloc(len);

            if (buf)
            {
                struct stat statbuf;
                snprintf(buf, len, "%s/%s", path, p->d_name);

                if (!stat(buf, &statbuf))
                {
                    if (S_ISDIR(statbuf.st_mode))
                    {
                        r2 = remove_directory(buf);
                    }
                    else
                    {
                        r2 = unlink(buf);
                    }
                }
                free(buf);
            }
            r = r2;
        }
        closedir(d);
    }

    if (!r)
    {
        r = rmdir(path);
    }
    return r;
}

void *readFunction(void *read_stuff)
{

    struct call_stuff *stuff = read_stuff;
    char *ptr = stuff->call_data + 5;
    char path[MAX_BUFFER_SIZE];
    char buffer[MAX_BUFFER_SIZE];
    strcpy(path, ptr);
    printf("Chalu %s\n", path);
    // printf("%d sd\n", stuff->sd);

    // sleep(5);
    printf("PATH:|%s|\n", path);
    FILE *file = fopen(path, "rb");

    if (!file)
    {
        perror("Error opening file");
        return 0;
    }
    int bytesRead, count = 0;
    bytesRead = fread(buffer, 1, MAX_BUFFER_SIZE, file);
    // {
    buffer[bytesRead] = '\0';

    if (send(stuff->sd, buffer, bytesRead, 0) < 0)
    {
        perror("Error sending data to server");
        count = 1;
    }
    if (count == 1)
    {
        strcpy(buffer, "FAILED");
    }
    else
    {
        strcpy(buffer, "ACK_R");
    }
    sleep(1);
    printf("SENDING |%s|", buffer);
    fflush(stdout);
    send(stuff->sd, buffer, MAX_BUFFER_SIZE, 0);
}

void *writeFunction(void *write_stuff)
{
    struct call_stuff *stuff = write_stuff;

    printf("in write\n");
    if (writer == 0)
    {
        writer = 1;

        char *path;
        char buffer[MAX_BUFFER_SIZE] = "ACK_W";
        char content[MAX_BUFFER_SIZE];

        for (int i = 6; i < MAX_BUFFER_SIZE; i++)
        {
            if (stuff->call_data[i] == ' ')
            {
                strcpy(content, &(stuff->call_data[i + 1]));
                break;
            }
        }

        strtok(stuff->call_data, " ");
        path = strtok(NULL, " ");

        // *ptr2 = '\0';
        // strcpy(path, ptr);
        printf("PATH  |%s|  %ld\n", path, strlen(path));
        FILE *file = fopen(path, "a+");
        if (!file)
        {
            perror("Error opening file");
            // exit(EXIT_FAILURE);
            return 0;
        }

        printf("CONTENT: %s", content);

        fprintf(file, "%s", content);
        fflush(file);
        content[sizeof(content)] = '\0';

        if (send(stuff->sd, buffer, MAX_BUFFER_SIZE, 0) == -1)
        {
            perror("Error sending file content");
        }
        writer = 0;
    }
    else
    {
        char buffer[MAX_BUFFER_SIZE] = "Someone Is Already Writing";
        if (send(stuff->sd, buffer, MAX_BUFFER_SIZE, 0) == -1)
        {
            perror("Error sending file content");
        }
    }
}

void *deleteDirFunction(void *dir_stuff)
{
    struct call_stuff *stuff = dir_stuff;
    char buffer[MAX_BUFFER_SIZE] = "Failed!";
    char *path;

    strtok(stuff->call_data, " ");
    path = strtok(NULL, " ");

    if (remove_directory(path) == 0)
    {
        strcpy(buffer, "SUCCESSFULLY REMOVED! LESSGO");
    }

    if (send(stuff->sd, buffer, MAX_BUFFER_SIZE, 0) == -1)
    {
        perror("Error sending file content");
    }
}

void *deleteFileFunction(void *fileStuff)
{
    struct call_stuff *stuff = fileStuff;
    char buffer[MAX_BUFFER_SIZE] = "Failed!";
    char *path;

    strtok(stuff->call_data, " ");
    path = strtok(NULL, " ");

    if (remove(path) == 0)
    {
        strcpy(buffer, "SUCCESS!");
    }

    if (send(stuff->sd, buffer, MAX_BUFFER_SIZE, 0) == -1)
    {
        perror("Error sending file content");
    }
}

void *createFileFunction(void *filestuff)
{
    printf("InCreateF\n");

    struct call_stuff *stuff = filestuff;
    char *path;
    char buffer[MAX_BUFFER_SIZE] = "WRITTEN SUCCESSFULLY";
    char content[MAX_BUFFER_SIZE];

    for (int i = 11; i < MAX_BUFFER_SIZE; i++)
    {
        if (stuff->call_data[i] == ' ')
        {
            while (stuff->call_data[i] == ' ')
            {
                i++;
            }

            strcpy(content, &(stuff->call_data[i]));
            break;
        }
    }

    strtok(stuff->call_data, " ");
    path = strtok(NULL, " ");

    // *ptr2 = '\0';
    // strcpy(path, ptr);
    printf("PATH  |%s|  %ld\n", path, strlen(path));
    FILE *file = fopen(path, "a");
    if (!file)
    {
        perror("Error opening file");
        // exit(EXIT_FAILURE);
        return 0;
    }
    printf("CONTENT  :   |%s|\n", content);
    char *ptr = strstr(content, "~~~");
    if (ptr)
        *ptr = '\0';
    if (content[0] != '@')
    {
        fprintf(file, "%s", content);
        fflush(file);
    }
    fclose(file);
}

void *createDirFunction(void *dirStuff)
{
    printf("in Cret\n");
    struct call_stuff *stuff = dirStuff;

    strtok(stuff->call_data, " ");
    char *path = strtok(NULL, " ");

    printf("%s\n", path);
    if (mkdir(path, 0777) != 0)
    {
        printf("FAILED");
    }

    else
    {
        printf("SUCCEDED\n");
    }
}

void *readNMFunction(void *read_stuff)
{
    struct call_stuff *stuff = (struct call_stuff *)read_stuff;
    char path[MAX_BUFFER_SIZE];
    char buffer[MAX_BUFFER_SIZE];

    strtok(stuff->call_data, " ");
    char *path1 = strtok(NULL, " ");
    char *path2 = strtok(NULL, " ");

    printf("PATH1: %s\n", path1);
    printf("PATH2: %s\n", path2);

    FILE *file = fopen(path1, "rb");
    if (!file)
    {
        perror("Error opening file");
        // exit(EXIT_FAILURE);
        return 0;
    }
    strcpy(path, path2);
    for (int i = strlen(path2); i < MAX_BUFFER_SIZE; i++)
    {
        path[i] = '~';
    }

    if (send(stuff->sd, path, MAX_BUFFER_SIZE, 0) == -1)
    {
        perror("Error sending file content");
    }

    int bytesrecvd, count = 0;

    while ((bytesrecvd = fread(buffer, 1, MAX_BUFFER_SIZE, file)) > 0)
    {
        // printf("%d sd\n", stuff->sd);
        for (int i = strlen(buffer); i < MAX_BUFFER_SIZE; i++)
        {
            buffer[i] = '~';
        }

        if (send(stuff->sd, buffer, MAX_BUFFER_SIZE, 0) == -1)
        {
            // perror("Error sending file content");
            count = 0;
            break;
        }
        count++;
    }

    sleep(1);
    if (count == 0)
    {
        strcpy(buffer, "~FAILED");
    }
    else
    {

        strcpy(buffer, "~DONE");
    }

    if (send(stuff->sd, buffer, MAX_BUFFER_SIZE, 0) == -1)
    {
        perror("Error sending file content");
        // exit(EXIT_FAILURE);
    }
}

void *getInfoFunction(void *info_stuff)
{
    struct call_stuff *stuff = info_stuff;
    strtok(stuff->call_data, " ");
    char *path = strtok(NULL, " ");
    struct stat fileStat;
    char buffer[1024];
    int flag = 0;
    if (stat(path, &fileStat) == 0)
    {
        sprintf(buffer, "%lld,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%s,%s",
                (long long)fileStat.st_size,
                (S_ISDIR(fileStat.st_mode)) ? "d" : "-",
                (fileStat.st_mode & S_IRUSR) ? 1 : 0,
                (fileStat.st_mode & S_IWUSR) ? 1 : 0,
                (fileStat.st_mode & S_IXUSR) ? 1 : 0,
                (fileStat.st_mode & S_IRGRP) ? 1 : 0,
                (fileStat.st_mode & S_IWGRP) ? 1 : 0,
                (fileStat.st_mode & S_IXGRP) ? 1 : 0,
                (fileStat.st_mode & S_IROTH) ? 1 : 0,
                (fileStat.st_mode & S_IWOTH) ? 1 : 0,
                (fileStat.st_mode & S_IXOTH) ? 1 : 0,
                ctime(&fileStat.st_mtime),
                ctime(&fileStat.st_atime),
                ctime(&fileStat.st_ctime));
    }
    else
    {
        perror("Error getting file information");
        strcpy(buffer, "FAILED! AT GETTING INFO");
        // {

        //     perror("Error sending file content");
        // }
        flag = 1;
    }

    if (send(stuff->sd, buffer, 1024, 0) == -1)
    {
    }

    sleep(1);
    strcpy(buffer, "ACK_G");
    if (flag == 1)
    {
        strcpy(buffer, "FAILED! AT GETTING INFO");
    }
    // printf("%s",buffer);
    if (send(stuff->sd, buffer, 1024, 0) == -1)
    {
    }
}

void *copydirFunction(void *copyDirStuff)
{
    // printf("In Copy\n");
    struct call_stuff *stuff = copyDirStuff;
    char dest[1024], src[1024];
    strtok(stuff->call_data, " ");
    char *ptr = strtok(NULL, " ");
    strcpy(dest, ptr);
    ptr = strtok(NULL, " ");
    strcpy(src, ptr);
    printf("SRC:  |%s|\n", src);
    printf("DEST : |%s|\n", dest);
    int index;
    if (strchr(src, '/') == NULL)
    {
        strcat(dest, "/");
        strcat(dest, src);
    }
    else
    {
        for (int i = strlen(src) - 1; i > 0; i--)
        {
            if (src[i] == '/')
            {
                index = i;
                break;
            }
        }
        strcat(dest, src + index);
    }
    printf("|%s|\n", dest);
    copyDirectory(src, dest);
}

void *copyFileFunction(void *copyFileStuff)
{
    printf("IN FILE");
    struct call_stuff *stuff = copyFileStuff;
    char dest[1024], src[1024];
    strtok(stuff->call_data, " ");
    char *ptr = strtok(NULL, " ");
    strcpy(dest, ptr);
    ptr = strtok(NULL, " ");
    strcpy(src, ptr);
    printf("SRC:  |%s|\n", src);
    printf("DEST : |%s|\n", dest);
    int index;
    if (strchr(src, '/') == NULL)
    {
        strcat(dest, "/");
        strcat(dest, src);
    }
    else
    {
        for (int i = strlen(src) - 1; i > 0; i--)
        {
            if (src[i] == '/')
            {
                index = i;
                break;
            }
        }
        strcat(dest, src + index);
    }
    copyFile(src, dest);
}

// listens to messages from NM and responds accordingly
void *NM_listener_thread(void *arg)
{
    int listen_fd, conn_fd;
    struct sockaddr_in server_addr;

    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket creation failed");
        return NULL;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(NEW_PORT);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        printf("NM_listener_thread\n");
        perror("Binding failed");
        return NULL;
    }

    if (listen(listen_fd, MAX_CLIENTS) == -1)
    {
        perror("Listening failed");
        return NULL;
    }

    printf("NM_listener_thread is waiting for connections...\n");

    int i_call_threads = 0;
    pthread_t call_threads[1000];
    struct call_stuff call_arr[1000];

    while (1)
    {
        conn_fd = accept(listen_fd, (struct sockaddr *)NULL, NULL);
        if (conn_fd < 0)
        {
            perror("Accepting connection failed");
            return NULL;
        }

        char buffer[MAX_BUFFER_SIZE];
        int valread = read(conn_fd, buffer, MAX_BUFFER_SIZE);
        if (valread > 0)
        {
            buffer[valread] = '\0';
            printf("Client is going to come for me: %s\n", buffer);
            // Process the received message as needed

            if (strncmp(buffer, "DELETEDIR ", 9) == 0)
            {
                call_arr[i_call_threads].call_data = (char *)malloc(sizeof(char) * strlen(buffer) + 2);
                strcpy(call_arr[i_call_threads].call_data, buffer);
                call_arr[i_call_threads].sd = conn_fd;
                pthread_create(&call_threads[i_call_threads], NULL, deleteDirFunction, &call_arr[i_call_threads]);
                i_call_threads = (i_call_threads + 1) % 1000;
            }
            else if (strncmp(buffer, "DELETEFILE ", 11) == 0)
            {
                call_arr[i_call_threads].call_data = (char *)malloc(sizeof(char) * strlen(buffer) + 2);
                strcpy(call_arr[i_call_threads].call_data, buffer);
                call_arr[i_call_threads].sd = conn_fd;
                pthread_create(&call_threads[i_call_threads], NULL, deleteFileFunction, &call_arr[i_call_threads]);
                i_call_threads = (i_call_threads + 1) % 1000;
            }
            else if (strncmp(buffer, "CREATEFILE ", 11) == 0)
            {
                call_arr[i_call_threads].call_data = (char *)malloc(sizeof(char) * strlen(buffer) + 2);
                strcpy(call_arr[i_call_threads].call_data, buffer);
                call_arr[i_call_threads].sd = conn_fd;
                pthread_create(&call_threads[i_call_threads], NULL, createFileFunction, &call_arr[i_call_threads]);
                i_call_threads = (i_call_threads + 1) % 1000;
            }
            else if (strncmp(buffer, "CREATEDIR ", 10) == 0)
            {
                call_arr[i_call_threads].call_data = (char *)malloc(sizeof(char) * strlen(buffer) + 2);
                strcpy(call_arr[i_call_threads].call_data, buffer);
                call_arr[i_call_threads].sd = conn_fd;
                pthread_create(&call_threads[i_call_threads], NULL, createDirFunction, &call_arr[i_call_threads]);
                i_call_threads = (i_call_threads + 1) % 1000;
            }
            else if (strncmp(buffer, "READ ", 5) == 0)
            {
                printf("READING\n");
                call_arr[i_call_threads].call_data = (char *)malloc(sizeof(char) * strlen(buffer) + 2);
                strcpy(call_arr[i_call_threads].call_data, buffer);
                // printf("%s\n", call_arr[i_call_threads].call_data);
                call_arr[i_call_threads].sd = conn_fd;
                call_arr[i_call_threads].sd = NM_periodic_FD;
                // printf("%d      prev %d",call_arr[i_call_threads].sd , sd);
                // printf("%d\n", call_arr[i_call_threads].sd);
                // if (i_call_threads == 1)
                // {
                //     printf("curr %d   prev %d", call_arr[i_call_threads].sd, call_arr[i_call_threads - 1].sd);
                // }
                printf("PATH FOR READING: %s\n", buffer);
                // fflush(stdout);
                pthread_create(&call_threads[i_call_threads], NULL, readNMFunction, &call_arr[i_call_threads]);
                i_call_threads = (i_call_threads + 1) % 1000;
            }
            else if (strncmp(buffer, "COPYDIR ", 8) == 0)
            {
                call_arr[i_call_threads].call_data = (char *)malloc(sizeof(char) * strlen(buffer) + 2);
                strcpy(call_arr[i_call_threads].call_data, buffer);
                call_arr[i_call_threads].sd = conn_fd;
                pthread_create(&call_threads[i_call_threads], NULL, copydirFunction, &call_arr[i_call_threads]);
                i_call_threads = (i_call_threads + 1) % 1000;
            }
            else if (strncmp(buffer, "COPYFILE ", 8) == 0)
            {
                call_arr[i_call_threads].call_data = (char *)malloc(sizeof(char) * strlen(buffer) + 2);
                strcpy(call_arr[i_call_threads].call_data, buffer);
                call_arr[i_call_threads].sd = conn_fd;
                pthread_create(&call_threads[i_call_threads], NULL, copyFileFunction, &call_arr[i_call_threads]);
                i_call_threads = (i_call_threads + 1) % 1000;
            }
        }

        close(conn_fd);
    }

    return NULL;
}

// Listens to messages from clients and responds accordingly
void *client_handler(void *arg)
{
    int main_fd;
    struct sockaddr_in server_addr;
    pthread_t thread;

    if ((main_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket creation failed");
        return 0;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(CLIENT_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(main_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        printf("CLient hanlder\n");
        perror("Binding failed");
        return 0;
    }

    listen(main_fd, MAX_CLIENTS);

    int client_socket[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_socket[i] = 0;
    }

    char buffer[MAX_BUFFER_SIZE];
    int i_call_threads = 0;

    while (1)
    {
        FD_ZERO(&client_fds);

        FD_SET(main_fd, &client_fds);
        int max_sd = main_fd;

        int sd;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_socket[i];
            if (sd > 0)
                FD_SET(sd, &client_fds);

            if (sd > max_sd)
            {
                max_sd = sd;
            }
        }

        // wait indefinitely for activity on one of the FDs
        int activity = select(max_sd + 1, &client_fds, NULL, NULL, NULL);

        if ((activity < 0))
        {
            printf("select error");
        }

        int new_socket;
        int addrlen = sizeof(server_addr);
        // If something happened on the master socket, then its an incoming connection
        if (FD_ISSET(main_fd, &client_fds))
        {
            if ((new_socket = accept(main_fd, (struct sockaddr *)&server_addr, (socklen_t *)&addrlen)) < 0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            printf("New connection , socket fd is %d\n", new_socket);

            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    break;
                }
            }
        }
        pthread_t call_threads[1000];
        struct call_stuff call_arr[1000];

        int valread;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_socket[i];
            if (FD_ISSET(sd, &client_fds))
            {
                // activity detected
                if ((valread = read(sd, buffer, MAX_BUFFER_SIZE)) == 0)
                {
                    // disconnected
                    printf("Client with fd %d disconnected\n", sd);
                    client_socket[i] = 0;
                    close(sd);
                }

                else
                {
                    buffer[valread] = '\0';
                    printf("MESSAGE by Client with fd %d: %s\n", sd, buffer);
                    // fflush(stdout);
                    if (strncmp(buffer, "READ ", 5) == 0)
                    {
                        printf("READING\n");
                        call_arr[i_call_threads].call_data = (char *)malloc(sizeof(char) * strlen(buffer) + 2);
                        strcpy(call_arr[i_call_threads].call_data, buffer);
                        // printf("%s\n", call_arr[i_call_threads].call_data);
                        call_arr[i_call_threads].sd = sd;
                        // printf("%d      prev %d",call_arr[i_call_threads].sd , sd);
                        // printf("%d\n", call_arr[i_call_threads].sd);
                        // if (i_call_threads == 1)
                        // {
                        //     printf("curr %d   prev %d", call_arr[i_call_threads].sd, call_arr[i_call_threads - 1].sd);
                        // }

                        pthread_create(&call_threads[i_call_threads], NULL, readFunction, &call_arr[i_call_threads]);
                        i_call_threads = (i_call_threads + 1) % 1000;
                    }

                    else if (strncmp(buffer, "WRITE ", 6) == 0)
                    {
                        call_arr[i_call_threads].call_data = (char *)malloc(sizeof(char) * strlen(buffer) + 2);
                        strcpy(call_arr[i_call_threads].call_data, buffer);
                        call_arr[i_call_threads].sd = sd;
                        pthread_create(&call_threads[i_call_threads], NULL, writeFunction, &call_arr[i_call_threads]);
                        i_call_threads = (i_call_threads + 1) % 1000;
                    }
                    else if (strncmp(buffer, "GETINFO ", 8) == 0)
                    {
                        call_arr[i_call_threads].call_data = (char *)malloc(sizeof(char) * strlen(buffer) + 2);
                        strcpy(call_arr[i_call_threads].call_data, buffer);
                        call_arr[i_call_threads].sd = sd;
                        pthread_create(&call_threads[i_call_threads], NULL, getInfoFunction, &call_arr[i_call_threads]);
                        i_call_threads = (i_call_threads + 1) % 1000;
                    }

                    // else if (strncmp(buffer, "CREATEDIR ", 10) == 0)
                    // {
                    //     call_arr[i_call_threads].call_data = (char *)malloc(sizeof(char) * strlen(buffer) + 2);
                    //     strcpy(call_arr[i_call_threads].call_data, buffer);
                    //     call_arr[i_call_threads].sd = sd;
                    //     pthread_create(&call_threads[i_call_threads], NULL, createDirFunction, &call_arr[i_call_threads]);
                    //     i_call_threads = (i_call_threads + 1) % 1000;
                    // }

                    // else if (SEND)
                    // {
                    //     /* code */
                    // }

                    // CREATEDIR pathname/dirToMake

                    // CREATEFILE PATHNAME  '@'

                    //

                    // COPY pathname pathname

                    //
                }
            }
        }
    }
}

void *periodic_messenger_thread(void *arg)
{
    int *sock = (int *)arg;
    int i = 0;
    char buffer[MAX_BUFFER_SIZE];
    snprintf(buffer, MAX_BUFFER_SIZE, "PERIODIC$1");
    NM_periodic_FD = *sock;
    while (1)
    {
        send(*sock, buffer, MAX_BUFFER_SIZE, 0);
        sleep(5); // Wait for 5 seconds
        i++;
        // printf("Message %d sent to Naming Server by SS_%d\n", i, ID);
    }
}

int main()
{
    int sock = 0;
    struct sockaddr_in server_addr;
    char buffer1[MAX_BUFFER_SIZE];

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

    // send $ID to naming server to indicate that it is Storage server with id 1
    snprintf(buffer1, 1024, "%s\n%s\n%d\n?folderX *folderX/file1.txt ?folderX/folderXX *folderX/folderXX/file.txt ?folderY *folderY/file2.txt", DID, "127.0.0.1", NEW_PORT);
    send(sock, buffer1, strlen(buffer1), 0);

    pthread_t periodic_messenger_ID;
    pthread_t client_connection_ID;
    pthread_t SS_tempID;
    pthread_create(&client_connection_ID, NULL, client_handler, NULL);
    pthread_create(&periodic_messenger_ID, NULL, periodic_messenger_thread, (void *)&sock);
    pthread_create(&SS_tempID, NULL, NM_listener_thread, NULL);

    pthread_join(periodic_messenger_ID, NULL); // Waits forever

    printf("DONE\n");
    close(sock);
    return 0;
}
