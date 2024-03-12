#include "msocket.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Define a structure to represent the state of each MTP socket
struct MSocket {
    int udp_socket; // The underlying UDP socket
    // Additional fields as needed
};

// Global error variable
int msocket_errno = 0;

// Shared memory structure
struct SM_entry SM[N];
SOCK_INFO sock_info;

// semaphore things
int sem1, sem2;
struct sembuf pop, vop;
                   
#define P(s) semop(s, &pop, 1)  /* pop is the structure we pass for doing
				   the P(s) operation */
#define V(s) semop(s, &vop, 1)  /* vop is the structure we pass for doing
				   the V(s) operation */

// Function to check if a free entry is available in SM
static int is_free_entry_available() {
    for(int i=0;i<25;i++){
        if(SM[i].is_free==1){
            return i;
        }
    }
    return -1;
}

// Function to initialize SM with corresponding entries (replace with your logic)
static void initialize_SM_entry(struct MSocket *msock) {
    int i=0;
    for(i=0;i<N;i++){
        if(SM[i].is_free==1){
            SM[i].is_free=0;
            SM[i].process_id=getpid();
            SM[i].udp_socket_id=msock->udp_socket;
            
            break;
        }
    }
}

int m_socket(int domain, int type, int protocol) {
    if (type != SOCK_MTP) {
        msocket_errno = EINVAL;
        return -1;
    }
    
    int i;

    if ((i=is_free_entry_available())==-1) {
        msocket_errno = ENOBUFS;
        msocket_errno = sock_info.err_no;
        sock_info.sock_id=0;
        sock_info.err_no=0;
        sock_info.ip_address[0]='\0';
        sock_info.port=0;
        return -1;
    }

    V(sem1);

    P(sem2);

    if(sock_info.sock_id==-1){
        msocket_errno = sock_info.err_no;
        return -1;
    }

    SM[i].is_free=0;
    SM[i].process_id=getpid();
    SM[i].udp_socket_id=sock_info.sock_id;
    
    // resetting sock_info
    sock_info.sock_id=0;
    sock_info.err_no=0;
    sock_info.ip_address[0]='\0';
    sock_info.port=0;

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
        sock_info.sock_id=0;
        sock_info.err_no=0;
        sock_info.ip_address[0]='\0';
        sock_info.port=0;
        return -1;
    }

    sock_info.sock_id=SM[sockfd].udp_socket_id;
    strcpy(sock_info.ip_address, src_ip);
    sock_info.port=src_port;

    V(sem1);

    P(sem2);

    if(sock_info.sock_id==-1){
        msocket_errno = sock_info.err_no;
        sock_info.sock_id=0;
        sock_info.err_no=0;
        sock_info.ip_address[0]='\0';
        sock_info.port=0;
        return -1;
    }

    strcpy(SM[sockfd].ip_address, dest_ip);
    SM[sockfd].port=dest_port;

    sock_info.sock_id=0;
    sock_info.err_no=0;
    sock_info.ip_address[0]='\0';
    sock_info.port=0;

    return 0;
}

ssize_t m_sendto(int sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen) {
    // Implement as needed
    return sendto(((struct MSocket *)sockfd)->udp_socket, buf, len, flags, dest_addr, addrlen);
}

ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen) {
    // Implement as needed
    return recvfrom(((struct MSocket *)sockfd)->udp_socket, buf, len, flags, src_addr, addrlen);
}

int m_close(int sockfd) {
    int result = close(((struct MSocket *)sockfd)->udp_socket);
    free((struct MSocket *)sockfd);
    return result;
}

int dropMessage() {
    float r = (float)rand() / (float)RAND_MAX;
    if (r < p) return 1;
    return 0;
}