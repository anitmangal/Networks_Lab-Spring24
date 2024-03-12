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

// Function to check if a free entry is available in SM (replace with your logic)
static int is_free_entry_available() {
    for(int i=0;i<25;i++){
        if(SM[i].is_free==1){
            return 1;
        }
    }
    return 0;
}

// Function to initialize SM with corresponding entries (replace with your logic)
static void initialize_SM_entry(struct MSocket *msock) {
    int i=0;
    for(i=0;i<25;i++){
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

    if (!is_free_entry_available()) {
        msocket_errno = ENOBUFS;
        return -1;
    }

    int udp_socket = socket(domain, SOCK_DGRAM, protocol);
    if (udp_socket == -1) {
        msocket_errno = errno;
        return -1;
    }

    // Allocate memory for MSocket structure
    struct MSocket *msock = (struct MSocket *)malloc(sizeof(struct MSocket));
    if (msock == NULL) {
        msocket_errno = ENOMEM;
        close(udp_socket);
        return -1;
    }

    // Initialize MSocket structure
    msock->udp_socket = udp_socket;

    // Initialize SM with corresponding entries (replace with your logic)
    initialize_SM_entry(msock);

    // Return the MTP socket descriptor
    return (int)msock;
}

int m_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    // Implement as needed
    return bind(((struct MSocket *)sockfd)->udp_socket, addr, addrlen);
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
