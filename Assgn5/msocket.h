#ifndef MSOCKET_H
#define MSOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h> 

// Custom socket type for MTP
#define SOCK_MTP 3

// defining T
#define T 5

// Global error variable
extern int msocket_errno;

// SM structure
struct SM_entry {
    int is_free;               // (i) whether the MTP socket with ID i is free or allotted
    pid_t process_id;          // (ii) process ID for the process that created the MTP socket
    int udp_socket_id;         // (iii) mapping from the MTP socket i to the corresponding UDP socket ID
    char ip_address[16];       // (iv) IP address of the other end of the MTP socket (assuming IPv4)
    uint16_t port;             // (iv) Port address of the other end of the MTP socket
    char send_buffer[10240];    // (v) send buffer
    char recv_buffer[5120];    // (vi) receive buffer
    // (vii) and (viii) should be replaced with actual structures
};

typedef struct SOCK_INFO{
    int sock_id;
    char ip_address[16];
    uint16_t port;
    int err_no;
} SOCK_INFO;

// Function prototypes
int m_socket(int domain, int type, int protocol);
int m_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t m_sendto(int sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen);
int m_close(int sockfd);

#endif  // MSOCKET_H
