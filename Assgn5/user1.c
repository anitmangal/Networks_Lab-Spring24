#include<stdio.h>
#include"msocket.h"
#include<unistd.h>
#include <fcntl.h>

int main(){
    int sockfd;
    struct sockaddr_in serverAddr;
    char buffer[1024];
    socklen_t addr_size;
    
    if((sockfd = m_socket(AF_INET, SOCK_MTP, 0))<0){
        printf("Error in socket creation\n");
        return 1;
    }
    
    char src_ip[16]="127.0.0.1";
    char dest_ip[16]="127.0.0.1";
    uint16_t src_port=12345;
    uint16_t dest_port=12346;

    if(m_bind(src_ip, src_port, dest_ip, dest_port)<0){
        printf("Error in binding\n");
        return 1;
    }

    int fd=open("input.txt", O_RDONLY);
    if(fd<0){
        printf("Error in opening file\n");
        return 1;
    }

    while(read(fd, buffer, 1024)>0){
        if(m_sendto(sockfd, buffer, 1024, 0, (struct sockaddr*)&serverAddr, addr_size)<0){
            printf("Error in sending\n");
            return 1;
        }
    }

    close(fd);

    if(m_close(sockfd)<0){
        printf("Error in closing\n");
        return 1;
    }

    return 0;
}