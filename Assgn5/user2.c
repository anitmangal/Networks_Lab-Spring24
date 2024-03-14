#include<stdio.h>
#include"msocket.h"
#include<unistd.h>
#include<fcntl.h>

int main(){
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
    uint16_t src_port=3000;
    uint16_t dest_port=4000;

    if(m_bind(src_ip, src_port, dest_ip, dest_port)<0){
        perror("Error in binding\n");
        return 1;
    }

    int fd=open("new.txt", O_WRONLY | O_CREAT, 0666);
    if(fd<0){
        perror("Error in opening/creating file\n");
        return 1;
    }

    while(m_recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr*)&serverAddr, &addr_size)>0){
        if(write(fd, buffer, 1024)<0){
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