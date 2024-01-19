/*
    Assignment 1 (Part B) Submission
    Name: Anit Mangal
    Roll number: 21CS10005
    Client Program (to be executed after wordserver.c)
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <netinet/in.h>

// Macro for max buffer length
#define MAXLINE 1024

int main() {
    // Create socket
    int sockid = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockid < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set server address
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(8181);
    int err = inet_aton("127.0.0.1", &serveraddr.sin_addr);
    if (err == 0) {
        printf("Error in ip-conversion\n");
        exit(EXIT_FAILURE);
    }

    // Bind client address
    struct sockaddr_in clientaddr;
    memset(&clientaddr, 0, sizeof(clientaddr));
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_port = htons(8182);
    clientaddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockid, (const struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
        perror("CLIENT bind failed");
        exit(EXIT_FAILURE);
    }

    // Send filename to server
    char filen[100];
    printf("Enter filename: ");
    scanf("%s", filen);
    sendto(sockid, (const char *)filen, strlen(filen), 0, (const struct sockaddr *)&serveraddr, sizeof(serveraddr));

    // Receive message from server
    socklen_t len;
    char buffer[MAXLINE];
    int n = recvfrom(sockid, (char *)buffer, MAXLINE, 0, (struct sockaddr *)&serveraddr, &len);
    buffer[n] = '\0';

    char * filesucc = "HELLO";
    char * fileend = "END";
    // If file not found, print error. Otherwise, proceed.
    if (strcmp(buffer, filesucc) != 0) {
        printf("File %s Not Found\n", filen);
    }
    else {
        FILE * fp = fopen("recvfile.txt", "w");
        int i =  1; // Iterator for word number
        while (1) {
            // Send word request to server
            char wrd[104] = "WORD";
            char num[100];
            sprintf(num, "%d", i);
            strcat(wrd, num);
            i++;
            sendto(sockid, (const char *)wrd, strlen(wrd), 0, (const struct sockaddr *)&serveraddr, sizeof(serveraddr));
            
            // Receive word from server
            n = recvfrom(sockid, (char *)buffer, MAXLINE, 0, (struct sockaddr *)&serveraddr, &len);
            buffer[n] = '\0';
            if (strcmp(buffer, fileend) == 0) {
                // If END received, break
                break;
            }
            fprintf(fp, "%s\n", buffer);
        }
        fclose(fp);
    }
    close(sockid);
}