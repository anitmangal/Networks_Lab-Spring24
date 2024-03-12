#include "msocket.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Global error variable
int msocket_errno = 0;

// Shared memory structure
struct SM_entry SM[N];
SOCK_INFO * sock_info;

// semaphore and shared memory variables
int sem1, sem2;
int sem_sock_info, sem_SM;
int shmid_sock_info, shmid_SM;

// Function to check if a free entry is available in SM
int is_free_entry_available() {
    for(int i=0;i<25;i++){
        if(SM[i].is_free==1){
            return i;
        }
    }
    return -1;
}

int m_socket(int domain, int type, int protocol) {
    if (type != SOCK_MTP) {
        msocket_errno = EINVAL;
        return -1;
    }
    
    int i;

    if ((i=is_free_entry_available())==-1) {
        msocket_errno = ENOBUFS;
        msocket_errno = sock_info->err_no;

        // resetting sock_info
        sock_info->sock_id=0;
        sock_info->err_no=0;
        sock_info->ip_address[0]='\0';
        sock_info->port=0;
        return -1;
    }

    V(sem1);

    P(sem2);

    if(sock_info->sock_id==-1){
        msocket_errno = sock_info->err_no;
        return -1;
    }

    SM[i].is_free=0;
    SM[i].process_id=getpid();
    SM[i].udp_socket_id=sock_info->sock_id;
    
    // resetting sock_info
    sock_info->sock_id=0;
    sock_info->err_no=0;
    sock_info->ip_address[0]='\0';
    sock_info->port=0;

    return i;
}

int m_bind(char src_ip[], uint16_t src_port, char dest_ip[], uint16_t dest_port) {
    int sockfd=-1;
    for(int i=0;i<25;i++){
        if(SM[i].is_free==0 && SM[i].process_id==getpid()){
            sockfd=i;
            break;
        }
    }

    // should never execute... just for safety
    if(sockfd==-1){
        msocket_errno = ENOBUFS;
        sock_info->sock_id=0;
        sock_info->err_no=0;
        sock_info->ip_address[0]='\0';
        sock_info->port=0;
        return -1;
    }

    sock_info->sock_id=SM[sockfd].udp_socket_id;
    strcpy(sock_info->ip_address, src_ip);
    sock_info->port=src_port;

    V(sem1);

    P(sem2);

    if(sock_info->sock_id==-1){
        msocket_errno = sock_info->err_no;
        sock_info->sock_id=0;
        sock_info->err_no=0;
        sock_info->ip_address[0]='\0';
        sock_info->port=0;
        return -1;
    }

    strcpy(SM[sockfd].ip_address, dest_ip);
    SM[sockfd].port=dest_port;

    sock_info->sock_id=0;
    sock_info->err_no=0;
    sock_info->ip_address[0]='\0';
    sock_info->port=0;

    return 0;
}

ssize_t m_sendto(int m_sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen) {
    char dest_ip[16];
    uint16_t dest_port;
    int udp_sockfd=SM[m_sockfd].udp_socket_id;
    inet_ntop(AF_INET, &(((struct sockaddr_in *)dest_addr)->sin_addr), dest_ip, 16);
    dest_port=ntohs(((struct sockaddr_in *)dest_addr)->sin_port);

    if(strcmp(SM[m_sockfd].ip_address, dest_ip)!=0 || SM[m_sockfd].port!=dest_port){
        msocket_errno = ENOTBOUND;      //????
        return -1;
    }

    if(SM[m_sockfd].swnd.size==0){      //???? assuming swnd.size=0 means send buffer is full
        errno=ENOBUFS;
        return -1;
    }

    return sendto(udp_sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen) {
    // Implement as needed
    return recvfrom(((struct MSocket *)sockfd)->udp_socket, buf, len, flags, src_addr, addrlen);
}

int m_close(int sockfd) {
    
}

int dropMessage() {
    float r = (float)rand() / (float)RAND_MAX;
    if (r < p) return 1;
    return 0;
}