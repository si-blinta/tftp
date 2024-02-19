#include "client.h"
#include<unistd.h>

int request(uint16_t opcode, const char* filename, config status, int sockfd, struct sockaddr* addr) {
    //the zero byte
    u_int8_t end_of_file = 0;
    int offset = 0;
    size_t packet_size = strlen(filename) + strlen(status.transfer_mode) + 4 + 2;
    char* packet = malloc(packet_size);
    if (packet == NULL) {
        fprintf(stderr,"[request] : Failed request\n");
        perror("malloc");
        return -1;
    }

    // Convert to network byte
    uint16_t