#ifndef MSOCKET_H
#define MSOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h> 
#include <sys/time.h>

// Custom socket type for MTP
#define SOCK_MTP 3

// defining T
#define T 5
#define p 0.05
#define N 25    


struct window {
    int wndw[16];               // 0: Sent-Acknowledged, NotSent, 1: Sent-NotAcknowledged for send window. 0: NotExpected, 1: Expected for receive window
    int size;
    int start_seq;              // start sequence number of the window
};

// SM structure
struct SM_entry {
    int is_free;               // (i) whether the MTP socket with ID i is free or allotted
    pid_t process_id;          // (ii) process ID for the process that created the MTP socket
    int udp_socket_id;         // (iii) mapping from the MTP socket i to the corresponding UDP socket ID
    char ip_address[16];       // (iv) IP address of the other end of the MTP socket (assuming IPv4)
    uint16_t port;             // (iv) Port address of the other end of the MTP socket
    char send_buffer[10][1024];    // (v) send buffer
    char recv_buffer[5][1024];    // (vi) receive buffer
    int send_buffer_valid[10]; // If the message at index i in send_buffer is valid or not
    int recv_buffer_valid[5];  // If the message at index i in recv_buffer is valid or not
    struct window swnd;        // (vii) send window
    struct window rwnd;        // (viii) receive window
    int nospace;               // whether receive buffer has space or not
    struct timeval lastSendTime;      // last message send time
};



typedef struct SOCK_INFO{
    int sock_id;
    char ip_address[16];
    uint16_t port;
    int err_no;
} SOCK_INFO;

// Global variables
extern int msocket_errno;
extern struct SM_entry SM[N];
extern SOCK_INFO sock_info;

// Function prototypes
int m_socket(int domain, int type, int protocol);
int m_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t m_sendto(int sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen);
int m_close(int sockfd);
int dropMessage();

#endif  // MSOCKET_H
