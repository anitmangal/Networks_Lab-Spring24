/*
    Assignment 1 (Part B) Submission
    Name: Anit Mangal
    Roll number: 21CS10005
    Server Program (to be executed before wordclient.c)
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

    // Bind server address
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(8181);
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockid, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("SERVER bind failed");
        exit(EXIT_FAILURE);
    }

    // Client struct
    struct sockaddr_in clientaddr;
    memset(&clientaddr, 0, sizeof(clientaddr));

    // Receive filename from client, and set client address
    socklen_t len;
    char fnamebuf[100];
    int n = recvfrom(sockid, (char *)fnamebuf, 100, 0, (struct sockaddr *)&clientaddr, &len);
    fnamebuf[n] = '\0';

    // Open file. If file not found, send NOTFOUND <filename> message to client. Otherwise, send first word to client
    FILE * fp = fopen(fnamebuf, "r");
    if (fp == NULL) {
        char filesucc[201] = "NOTFOUND ";
        strcat(filesucc, fnamebuf);
        sendto(sockid, (const char *)filesucc, strlen(filesucc), 0, (const struct sockaddr *)&clientaddr, sizeof(clientaddr));
    }
    else {
        printf("To exit the server, press CTRL+C\n");

        // Send first word to client
        char buffer[MAXLINE];
        fscanf(fp, "%s", buffer);
        sendto(sockid, (const char *)buffer, strlen(buffer), 0, (const struct sockaddr *)&clientaddr, sizeof(clientaddr));
        while(1) {
            // Receive word number from client.
            n = recvfrom(sockid, (char *)buffer, MAXLINE, 0, (struct sockaddr *)&clientaddr, &len);
            buffer[n] = '\0';
            char * wordstr = buffer+4;
            int word = atoi(wordstr);
            // Find word number in file. If found, send word to client. Otherwise, send error message to client.
            rewind(fp);
            int cnt = 0;
            while(fscanf(fp, "%99s", buffer) != EOF) {
                cnt++;
                if (cnt == word+1) {
                    sendto(sockid, (const char *)buffer, strlen(buffer), 0, (const struct sockaddr *)&clientaddr, sizeof(clientaddr));
                    break;
                }
            }
            if (cnt != word+1) {
                char * errtext = "ERROR: Word not found";
                sendto(sockid, (const char *)errtext, strlen(errtext), 0, (const struct sockaddr *)&clientaddr, sizeof(clientaddr));
            }
        }
        fclose(fp);
    }
    close(sockid);
}