#include<stdio.h>
#include"msocket.h"
#include<unistd.h>
#include<fcntl.h>

int main(int argc, char *argv[]){
    if(argc!=3){
        printf("Usage: %s <src_port> <dest_port>\n", argv[0]);
        return 1;
    }
    int sockfd;
    struct sockaddr_in serverAddr;
    char buffer[1024];
    socklen_t addr_size;
    
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

    char filename[100];
    sprintf(filename, "new_%d.txt", src_port);
    int fd=open(filename, O_WRONLY | O_CREAT, 0666);
    if(fd<0){
        perror("Error in opening/creating file\n");
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(dest_port);
    serverAddr.sin_addr.s_addr = inet_addr(dest_ip);
    printf("Reading file\n");
    int recvlen;
    // while ((recvlen = m_recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr*)&serverAddr, &addr_size))<=0){
    //     printf("Checking recv\n");
    //     if (recvlen <= 0) {
    //         sleep(1);
    //         continue;
    //     }
    // }
    // printf("Writing %d bytes\n", recvlen);
    // if(write(fd, buffer, recvlen)<0){
    //     perror("Error in writing to file\n");
    //     return 1;
    // }
    // while ((recvlen = m_recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr*)&serverAddr, &addr_size))<=0){
    //     printf("Checking recv\n");
    //     if (recvlen <= 0) {
    //         sleep(1);
    //         continue;
    //     }
    // }
    // printf("Writing\n");
    // if(write(fd, buffer, recvlen)<0){
    //     perror("Error in writing to file\n");
    //     return 1;
    // }

    while(1){
        while((recvlen = m_recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr*)&serverAddr, &addr_size))<=0){
            printf("Checking recv\n");
            if (recvlen <= 0) {
                sleep(1);
                continue;
            }
        }
        printf("Writing %d bytes\n", recvlen);
        if(write(fd, buffer, recvlen)<0){
            perror("Error in writing to file\n");
            return 1;
        }
    }

    close(fd);

    if(m_close(sockfd)<0){
        perror("Error in closing\n");
        return 1;
    }

    return 0;
}