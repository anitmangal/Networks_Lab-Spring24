/*
    Assignment 2 Submission
    Name: Anit Mangal
    Roll number: 21CS10005
    Server program
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_BUFF_LENGTH 100     // Max chunk size to be sent/received at a time
#define MAX_FILENAME_LEN 100
#define END '#'

int main() {
    // Create socket and bind to port
    int sockid  = socket(AF_INET, SOCK_STREAM, 0);
    if (sockid < 0) {
        perror("Error creating socket.\n");
        exit(0);
    }
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(2100);
    if (bind(sockid, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("Error binding.\n");
        exit(0);
    }

    // Listen for connections
    listen(sockid, 5);

    struct sockaddr_in cliaddr;
    int newsockid;

    // Iterative server
    while(1) {
        // Accept connection
        int clilen = sizeof(cliaddr);
        newsockid = accept(sockid, (struct sockaddr*)&cliaddr, &clilen);
        if (newsockid < 0) {
            perror("Accept error.\n");
            exit(0);
        }
        // For each connection, fork a child process to handle it
        if (fork() == 0) {
            close(sockid);
            // Form filename as IP.port.txt
            char filename[MAX_FILENAME_LEN];
            inet_ntop(AF_INET, &(cliaddr.sin_addr), filename, INET_ADDRSTRLEN);
            strcat(filename, ".");
            int portno = ntohs(cliaddr.sin_port);
            sprintf(filename+strlen(filename), "%d", portno);
            strcat(filename, ".txt");
            int f = open(filename, O_WRONLY | O_CREAT, 0666);
            if (f < 0) {
                perror("Error opening file.\n");
                exit(0);
            }

            // Receive key
            char buf[MAX_BUFF_LENGTH+1];
            buf[0] = '\0';
            int recvbytes;
            recvbytes = recv(newsockid, buf, MAX_BUFF_LENGTH, 0);
            char keystr[10];
            int cnt = 0;
            while (buf[cnt] != '\0') {
                keystr[cnt] = buf[cnt];
                cnt++;
            }
            keystr[cnt] = '\0';
            int keylen = atoi(keystr);
            cnt++;
            int filestart = cnt;
            cnt = 0;
            while (filestart+cnt < recvbytes && buf[filestart+cnt] != END) cnt++;
            write(f, buf+filestart, cnt);

            // Receive file until EOF is received
            while(buf[cnt] != END) {
                recvbytes = recv(newsockid, buf, MAX_BUFF_LENGTH, 0);
                cnt = 0;
                while (cnt < recvbytes && buf[cnt] != END) cnt++;
                write(f, buf, cnt);
            }
            close(f);
            f = open(filename, O_RDONLY);

            // Write and send encrypted file
            int newf;
            strcat(filename, ".enc");
            newf = open(filename, O_WRONLY | O_CREAT, 0666);
            int readbytes;
            while((readbytes = read(f, buf, MAX_BUFF_LENGTH)) > 0) {
                    for (int i = 0; i < readbytes; i++) {
                        if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] = (char)('a'+(((buf[i]-'a')+keylen)%26));
                        else if (buf[i] >= 'A' && buf[i] <= 'Z') buf[i] = (char)('A'+(((buf[i]-'A')+keylen)%26));\
                    }
                    write(newf, buf, readbytes);
                    send(newsockid, buf, readbytes, 0);
            }
            char endstr[1];
            endstr[0] = END;
            send(newsockid, endstr, 1, 0);
            close(newf);
            close(newsockid);
            exit(0);
        }
        close(newsockid);
    }
}