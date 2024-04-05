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
#include <time.h>

#define BUFFSIZE 1518
#define DROPRATE 0.1
int ipID = 0;

unsigned short checksum(unsigned short* buff, int _16bitword) {
    unsigned long sum;
    for(sum=0;_16bitword>0;_16bitword--)
    sum+=htons(*(buff)++);
    sum = ((sum >> 16) + (sum & 0xFFFF));
    sum += (sum>>16);
    return (unsigned short)(~sum);
}

int dropmessage(float p) {
    float r = (float)rand()/(float)RAND_MAX;
    if (r < p) return 1;
    return 0;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <MAC address>\n", argv[0]);
        exit(1);
    }

    // Raw socket creation
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    // Bind raw socket to local IP Address
    char * interface = "lo";
    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface)) < 0) {
        perror("SO_BINDTODEVICE");
        exit(1);
    }

    // get mac from argv[1]
    unsigned char mac[6];
    sscanf(argv[1], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

    // Receive packets
    char buffer[BUFFSIZE];
    while(1) {
        int len = recvfrom(sockfd, buffer, BUFFSIZE, 0, NULL, NULL);
        if (len < 0) {
            perror("recvfrom");
            exit(1);
        }
        if (len == 0) {
            break;
        }

        // Drop packet with probability DROPRATE
        if (dropmessage(DROPRATE)) continue;

        // Parse Ethernet header
        struct ethhdr *eth = (struct ethhdr *)buffer;
        // Check if the packet is from the MAC address
        if (memcmp(eth->h_source, mac, 6) != 0) {
            continue;
        }
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
        char simDNSresponse[44];    // 4 bytes for header, 5 bytes for each response

        char *simDNSquery = buffer + sizeof(struct ethhdr) + sizeof(struct iphdr);
        // Same ID as query
        simDNSresponse[0] = simDNSquery[0];
        simDNSresponse[1] = simDNSquery[1];
        simDNSresponse[2] = (uint16_t)1;    // Response

        // Parse query
        char type = simDNSquery[2];
        if ((int)type == 1) continue;   // Only support Query type

        // Parse number of queries
        uint8_t numQ = simDNSquery[3];
        simDNSresponse[3] = numQ;

        // Parse queries
        int responsePointer = 4;
        int qStrPointer = 4;
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
            if (host == NULL || host->h_addr_list == NULL) simDNSresponse[responsePointer] = 0;         // No such domain 
            else {
                // Add first IP address to response
                simDNSresponse[responsePointer] = (uint16_t)1;
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

        // Get MAC address from query
        struct sockaddr_ll destaddr;
        destaddr.sll_family = AF_PACKET;
        destaddr.sll_protocol = htons(ETH_P_ALL);
        destaddr.sll_ifindex = if_nametoindex("lo");
        destaddr.sll_halen = 6;
        destaddr.sll_addr[0] = eth->h_dest[0];
        destaddr.sll_addr[1] = eth->h_dest[1];
        destaddr.sll_addr[2] = eth->h_dest[2];
        destaddr.sll_addr[3] = eth->h_dest[3];
        destaddr.sll_addr[4] = eth->h_dest[4];
        destaddr.sll_addr[5] = eth->h_dest[5];

        // Create packet to send as response
        char packet[82];            // 18 bytes for Ethernet header, 20 bytes for IP header, 44 bytes for simDNS response
        // Pointers to Ethernet and IP headers
        struct ethhdr *ethResponse = (struct ethhdr *)packet;
        struct iphdr *ipResponse = (struct iphdr *)(packet + sizeof(struct ethhdr));

        // Copy simDNS response to packet
        memcpy(packet + sizeof(struct ethhdr) + sizeof(struct iphdr), simDNSresponse, responsePointer);

        // Fill in IP header
        ipResponse->ihl = 5;
        ipResponse->version = 4;
        ipResponse->tos = 0;
        ipResponse->tot_len = htons(64);    // Header: 20 bytes, Data: 44 bytes
        ipResponse->id = htons(ipID++);
        ipResponse->frag_off = 0;
        ipResponse->ttl = 64;
        ipResponse->protocol = 254;
        ipResponse->check = checksum((unsigned short *)ipResponse, sizeof(struct iphdr)/2);
        ipResponse->saddr = ipheader->daddr;
        ipResponse->daddr = ipheader->saddr;

        // Fill in Ethernet header
        memcpy(ethResponse->h_dest, eth->h_source, 6);
        memcpy(ethResponse->h_source, eth->h_dest, 6);
        ethResponse->h_proto = htons(ETH_P_IP);

        int sentBytes = sendto(sockfd, packet, 38+responsePointer, 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
        if (sentBytes < 0) {
            perror("sendto");
            exit(1);
        }
    }
}