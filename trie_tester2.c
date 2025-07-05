#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALPHABET_SIZE 39 // 26 alphabets, 1 '/', 1 '.', 10 digits, 1 '$'
#define MAX_BUFFER_SIZE 1024

struct SS_Connection_Args
{
    int SS_number;
    int client_fd_ID;
    int reestablished_Connection;
    int command;
    int returnToClient_FD;
    char cmd_buffer[MAX_BUFFER_SIZE];
    char cmd_buffer2[MAX_BUFFER_SIZE];
};

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
void delete_node(char key[]);
int search(char key[]);
char **finder(char key[]);
char **finderByData(int data);
struct node *root = NULL;
// --------------------------------------------------------------------------
// --------------------------------------------------------------------------

int main()
{
    insert_node("a", 1, 0);
    insert_node("a/b", 1, 0);
    insert_node("a/b/c", 1, 0);
    insert_node("a/b/c/d", 1, 0);
    insert_node("a/b/c/e", 1, 0);
    insert_node("a/b/c/d/f.txt", 1, 1);
    insert_node("a/b/c/e/g.txt", 1, 1);
    insert_node("folder1", 2, 0);
    insert_node("folder1/study", 2, 0);
    insert_node("folder1/study/material.txt", 2, 1);
    insert_node("folder1/study/group", 2, 0);
    insert_node("folder1/study/group/more.txt", 2, 1);
    insert_node("folder3", 2, 0);
    insert_node("folder2/study", 2, 0);
    insert_node("folder2/study/collab.txt", 2, 1);
    insert_node("folder3/new", 2, 0);
    insert_node("$1", 2, 0);
    
    // char **result = finder("a/b/c");
    // int resultLen = atoi(result[0]);

    // printf("Number of paths found: %d\n", resultLen);
    // COPY a/b/c folder1/study
    char argscmd_buffer2[1024];
    strcpy(argscmd_buffer2, "folder1/study");
    char argscmd_buffer[1024];
    strcpy(argscmd_buffer, "a/b");
    // snprintf(argscmd_buffer, 1024, "$%d", 1);

    int SS_from = search(argscmd_buffer2);
    int SS_to = search(argscmd_buffer);
    printf("%d %d\n", SS_from, SS_to);

    int index = -1;
    int x = strlen(argscmd_buffer2);

    for (int i=x-1; i>=0; i--){
        if (argscmd_buffer2[i] == '/'){
            index = i;
        }
    }
    index += 2;
    // printf("I%d\n", index);
    char** paths = finder(argscmd_buffer2);
    // char** paths = finderByData(1);
    int num_paths = atoi(paths[0]);
    char* newpath = (char*)malloc(MAX_BUFFER_SIZE*sizeof(char));
    char* newpath_copy = (char*)malloc(MAX_BUFFER_SIZE*sizeof(char));
    strcpy(newpath, argscmd_buffer);
    strcat(newpath, "/");
    strcpy(newpath_copy, newpath);
    // printf("TO SS_%d:\n", SS_to);
    for (int i=1; i<=num_paths; i++){
        strcat(newpath, &(paths[i][index]));
        // printf("%s\n", newpath);
        // printf("%s\n", paths[i]);

        if (paths[i][0] == '?'){
            struct SS_Connection_Args newargs;
            newargs.command = 2;
            newargs.SS_number = SS_to;
            strcpy(newargs.cmd_buffer, "CREATEDIR ");
            strcat(newargs.cmd_buffer, newpath);
            printf("SEND: %s\n", newargs.cmd_buffer);
        }
        else if (paths[i][0] == '*'){
            // struct SS_Connection_Args newargs;
            // newargs.command = 2;
            // newargs.SS_number = SS_to;
            // strcpy(newargs.cmd_buffer, "CREATEFILE ");
            // strcat(newargs.cmd_buffer, newpath);
            // printf("SEND: %s\n", newargs.cmd_buffer);
            // strcpy(newargs.cmd_buffer, "READ ");
            // strcat(newargs.cmd_buffer, &(paths[i][1]));
            // printf("SEND: %s\n", newargs.cmd_buffer);
            struct SS_Connection_Args newargs_1;
            newargs_1.command = 2;
            newargs_1.SS_number = SS_from;
            strcpy(newargs_1.cmd_buffer, "READ ");
            strcat(newargs_1.cmd_buffer, &(paths[i][1]));
            strcat(newargs_1.cmd_buffer, " ");
            strcat(newargs_1.cmd_buffer, newpath); // continue here
            printf("SEND: %s\n", newargs_1.cmd_buffer);
            // recieve in a while loop:
            
        }

        if (paths[i][0] == '?'){
            // insert_node(newpath, search(argscmd_buffer), 0);
        }
        else if (paths[i][0] == '*'){
            // insert_node(newpath, search(argscmd_buffer), 1);
        }
        strcpy(newpath, newpath_copy);
        // mujhe wo paths chahiye honge na 
    }    
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
        if (q->link[index] != NULL)
            q = q->link[index];
        else
            break;
    }
    if (key[level] == '\0' && q->data != -1)
        return q->data;
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

void delete_node(char key[])
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
        if (q->link[index] != NULL)
            q = q->link[index];
        else
            break;
    }
    if (key[level] == '\0' && q->data != -1)
    {
        mark_as_deleted(q);
    }
}
// --------------------------------------------------------------------------
