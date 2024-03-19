#include<stdio.h>
#include"msocket.h"
#include<unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int main(int argc, char *argv[]){
    if(argc!=3){
        printf("Usage: %s <src_port> <dest_port>\n", argv[0]);
        return 1;
    }
    int sockfd;
    struct sockaddr_in serverAddr;
    char buffer[1024];
    
    if((sockfd = m_socket(AF_INET, SOCK_MTP, 0))<0){
        perror("Error in socket creation\n");
        return 1;
    }
    
    char src_ip[16]="127.0.0.1";
    char dest_ip[16]="127.0.0.1";
    uint16_t src_port=atoi(argv[1]);
    uint16_t dest_port=atoi(argv[2]);

    if(m_bind(src_ip, src_port, dest_ip, dest_port)<0){
        perror("Error in binding\n");
        return 1;
    }

    int fd=open("input.txt", O_RDONLY);
    if(fd<0){
        perror("Error in opening file\n");
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(dest_port);
    inet_aton(dest_ip, &serverAddr.sin_addr);
    printf("Reading file\n");
    int readlen;
    while((readlen=read(fd, buffer, 1024))>0){
        printf("Sending %d bytes\n",readlen);
        for (int i = 0; i < readlen; i++) {
            printf("%c", buffer[i]);
        }
        printf("\n\n\n\n");
        int sendlen;
        if((sendlen=m_sendto(sockfd, buffer, readlen, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr)))<0){
            perror("Error in sending\n");
            return 1;
        }
        printf("Sent %d bytes\n", sendlen);
    }
    sleep(10);   // To ensure all messages are sent

    close(fd);

    if(m_close(sockfd)<0){
        perror("Error in closing\n");
        return 1;
    }

    return 0;
}