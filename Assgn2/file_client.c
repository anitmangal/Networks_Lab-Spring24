/*
    Assignment 2 Submission
    Name: Anit Mangal
    Roll number: 21CS10005
    Client Program
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

#define MAX_FILENAME_LEN 100
#define MAX_BUFF_LENGTH 60     // Max chunk size to be sent/received at a time
#define END '#'

int main() {
    // Main loop
    while(1) {
        // Get filename and open file
        char filename[MAX_FILENAME_LEN];
        int f;
        printf("Enter filename: ");
        scanf("%s", filename);
        while ((f = open(filename, O_RDONLY)) < 0) {
            perror("File not found.\n");
            printf("Enter filename: ");
            scanf("%s", filename);
        }

        // Get key
        int keylength = 0;
        printf("Enter key: ");
        scanf("%d", &keylength);

        // Create socket and connect to server
        int sockid = socket(AF_INET, SOCK_STREAM, 0);
        if (sockid < 0) {
            perror("Error creating socket.\n");
            exit(0);
        }
        struct sockaddr_in serveraddr;
        memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        inet_aton("127.0.0.1", &(serveraddr.sin_addr));
        serveraddr.sin_port = htons(2100);
        if (connect(sockid, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
            perror("Error connecting to server.\n");
            exit(0);
        }

        char endstr[1];
        endstr[0] = END;
        // Send key along with NULL to indicate end of key
        char buf[MAX_BUFF_LENGTH+1];
        sprintf(buf, "%d", keylength);
        send(sockid, buf, strlen(buf)+1, 0);
        // Send file
        int readbytes;
        while ((readbytes = read(f, buf, MAX_BUFF_LENGTH)) > 0) {
            send(sockid, buf, readbytes, 0);
        }
        send(sockid, endstr, 1, 0);               // Send delimiter to indicate end of file
        close(f);

        // Receive encrypted file and store in filename.enc until EOF is received.
        char newfilename[MAX_FILENAME_LEN+5];
        strcpy(newfilename, filename);
        strcat(newfilename, ".enc");
        int newf;
        int bytesrecv;
        newf = open(newfilename, O_WRONLY | O_CREAT, 0666);
        // Receive file until END is received
        buf[0] = '\0';
        int cnt = 0;
        while (buf[cnt] != END) {
            bytesrecv = recv(sockid, buf, MAX_BUFF_LENGTH, 0);
            cnt = 0;
            while(cnt < bytesrecv && buf[cnt] != END) cnt++;
            write(newf, buf, cnt);
        }
        close(newf);
        printf("File is encrypted.\n");
        printf("Original filename: %s\n", filename);
        printf("Encrypted filename: %s\n\n\n", newfilename);
        close(sockid);
        // Connection closed. Loop back to start.
    }
}