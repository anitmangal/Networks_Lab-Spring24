#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <asm-generic/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <time.h>
#include <signal.h>

#define BUFFSIZE 1518
#define DROPRATE 0.2
#define interface_name "lo"
int ipID = 0;

// Linked list to store query IDs to handle duplicate queries
typedef struct node{
    int id;
    struct node *next;
}node;

void insertNode(node *head, int id){
    node *current = head;
    while(current->next != NULL){
        current = current->next;
    }
    current->next = (node *)malloc(sizeof(node));
    current->next->id = id;
    current->next->next = NULL;
}

node *findNode(node *head, int id){
    node *current = head;
    while(current != NULL){
        if(current->id == id) return current;
        current = current->next;
    }
    return NULL;
}

// Drop message with probability p
int dropmessage(float p) {
    float r = (float)rand()/(float)RAND_MAX;
    if (r < p) return 1;
    return 0;
}

// Singal handler to free memory and close socket
node * headptr;
int sockfd;
void sigHandler(int sig) {
    if (sig != SIGINT) return;
    node *current = headptr;
    node *temp;
    while(current != NULL){
        temp = current;
        current = current->next;
        free(temp);
    }
    close(sockfd);
    exit(0);
}

int main() {
    signal(SIGINT, sigHandler);
    node *head = (node *)malloc(sizeof(node));
    headptr = head;
    head->id = 0;
    head->next = NULL;

    srand(time(NULL));

    // Raw socket creation in promiscuous mode
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    // Bind raw socket to local IP Address
    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = if_nametoindex(interface_name);
    if (bind(sockfd, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        perror("bind");
        exit(1);
    }

    // Receive packets
    char buffer[BUFFSIZE];
    while(1) {
        /*

            RECEIVE PACKET

        */

        int len = recvfrom(sockfd, buffer, BUFFSIZE, 0, NULL, NULL);
        if (len < 0) {
            perror("recvfrom");
            exit(1);
        }
        if (len == 0) {
            break;
        }

        // Drop packet with probability DROPRATE
        if (dropmessage(DROPRATE)) {
            continue;
        }

        // Parse Ethernet header
        struct ethhdr *eth = (struct ethhdr *)buffer;

        // Check if the packet is an IP packet
        if (ntohs(eth->h_proto) != ETH_P_IP) {
            continue;
        }

        // Parse IP header
        struct iphdr *ipheader = (struct iphdr *)(buffer + sizeof(struct ethhdr));
        // Check if the packet uses protocol 254
        if (ipheader->protocol != 254) {
            continue;
        }

        // Parse simDNS query
        char simDNSresponse[44];    // 4 bytes for header, 5 bytes for each query string response

        char *simDNSquery = buffer + sizeof(struct ethhdr) + sizeof(struct iphdr);  // Start of simDNS query in packet

        // Same ID as query
        simDNSresponse[0] = simDNSquery[0];
        simDNSresponse[1] = simDNSquery[1];
        simDNSresponse[2] = 0x01;    // Response type

        int qID = (simDNSquery[0]<<8) | simDNSquery[1];
        if(findNode(head, qID) != NULL) continue;


        // Parse query
        char type = simDNSquery[2];
        if ((int)type == 1) continue;   // Only support Query type

        // Parse number of query strings
        uint8_t numQ = simDNSquery[3];
        simDNSresponse[3] = numQ;

        // Parse query strings
        int responsePointer = 4;    // Cursor to response string
        int qStrPointer = 4;    // Cursor to query string
        for (int i = 0; i < numQ; i++) {
            // Get length of domain
            int domainSize = (simDNSquery[qStrPointer]<<24) | (simDNSquery[qStrPointer+1]<<16) | (simDNSquery[qStrPointer+2]<<8) | simDNSquery[qStrPointer+3];
            qStrPointer += 4;
            // Get domain
            char domain[domainSize+1];
            for (int j = 0; j < domainSize; j++) {
                domain[j] = simDNSquery[qStrPointer+j];
            }
            domain[domainSize] = '\0';
            qStrPointer += domainSize;
            struct hostent *host = gethostbyname(domain);       // Get IP address
            if (host == NULL || host->h_addr_list == NULL) simDNSresponse[responsePointer] = 0x00;         // No such domain 
            else {
                // Add first IP address to response
                simDNSresponse[responsePointer] = 0x01;
                simDNSresponse[responsePointer+1] = host->h_addr_list[0][0];
                simDNSresponse[responsePointer+2] = host->h_addr_list[0][1];
                simDNSresponse[responsePointer+3] = host->h_addr_list[0][2];
                simDNSresponse[responsePointer+4] = host->h_addr_list[0][3];
            }
            responsePointer += 5;
        }

        /*

            SEND RESPONSE PACKET

        */

        // Fill in destination address
        struct sockaddr_ll destaddr;
        destaddr.sll_family = AF_PACKET;
        destaddr.sll_protocol = htons(ETH_P_ALL);
        destaddr.sll_ifindex = if_nametoindex(interface_name);
        destaddr.sll_halen = 6;
        memcpy(destaddr.sll_addr, eth->h_source, 6);            // Send response to source address

        // Create packet to send as response
        int packetLength = sizeof(struct ethhdr)+sizeof(struct iphdr)+sizeof(char)*responsePointer;
        char packet[packetLength];
        // Pointers to Ethernet and IP headers
        struct ethhdr *ethResponse = (struct ethhdr *)packet;
        struct iphdr *ipResponse = (struct iphdr *)(packet + sizeof(struct ethhdr));

        // Copy simDNS response created above to packet
        memcpy(packet + sizeof(struct ethhdr) + sizeof(struct iphdr), simDNSresponse, responsePointer);

        // Fill in IP header
        ipResponse->ihl = 5;
        ipResponse->version = 4;
        ipResponse->tos = 0;
        ipResponse->tot_len = htons(sizeof(struct iphdr) + 4*sizeof(char) + 5*sizeof(char)*numQ);
        ipResponse->id = htons(ipID++);
        ipResponse->frag_off = 0;
        ipResponse->ttl = 64;
        ipResponse->protocol = 254;
        ipResponse->saddr = ipheader->daddr;
        ipResponse->daddr = ipheader->saddr;

        // Fill in Ethernet header
        memcpy(ethResponse->h_dest, eth->h_source, 6);
        memcpy(ethResponse->h_source, eth->h_dest, 6);
        ethResponse->h_proto = htons(ETH_P_IP);

        // Send response
        int sentBytes = sendto(sockfd, packet, packetLength, 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
        if (sentBytes < 0) {
            perror("sendto");
            exit(1);
        }
        insertNode(head, qID);
    }
}