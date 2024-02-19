#include "utils.h"
int packet_loss(uint8_t loss_percentage) {
    int rand_val = rand() % 100;
    return rand_val < loss_percentage;
}
uint16_t get_opcode(char* packet){
    uint16_t opcode;
    memcpy(&opcode, packet, sizeof(opcode));
    return ntohs(opcode); // Convert from network byte order to host byte order
}
char* get_file_name(char* packet){
    char* buffer=strdup(packet+2);
    return buffer;
}
char* get_mode(char* packet){
    char* filename = get_file_name(packet);
    char* mode = strdup(packet+strlen(filename)+3);
    free(filename);
    return mode;
}
char* get_data(char* packet){
    return packet+4;
}
uint16_t get_error_code(char* packet){
    uint16_t error_code;
    memcpy(&error_code, packet + 2, sizeof(error_code)); 
    return ntohs(error_code); 
}
char* get_error_message(char* packet){
    char* error_msg = strdup(packet+4);
    return error_msg;

}
uint16_t get_block_number(char* packet){
    uint16_t block_number;
    memcpy(&block_number, packet + 2, sizeof(block_number));
    return ntohs(block_number);
}
int send_ack_packet(config status,const struct sockaddr* client_addr, uint16_t block_number, int sockfd) {
    size_t packet_size;
    char* ack_packet = build_ack_packet(block_number, &packet_size);

    if (ack_packet == NULL) {
        fprintf(stderr,"[send_ack_packet] : failed to build ack packet\n");
        return -1; 
    }

    if(sendto(sockfd, ack_packet, packet_size, 0, client_addr, sizeof(*client_addr))<0){
        if(errno != EWOULDBLOCK && errno != EAGAIN){
            perror("[sendto]");
            free(ack_packet); 
            return -1;
        }
        //Time out reached
        printf("[send_ack_packet] : time out\n");
        free(ack_packet);
        return -1;
    }
    if(status.trace){
        trace_sent(ack_packet,packet_size);
    }
    free(ack_packet);
    return 0;
}
char* build_error_packet(uint16_t error_code, char* error_msg,size_t* packet_size) {
    //The zero byte
    u_int8_t end_of_error_msg = 0;
    //dynamically allocate the string
    char* error_packet = malloc(strlen(error_msg) + 5 + 1);
    if (error_packet == NULL) {
        perror("malloc");
        return NULL;
    }
    int offset = 0;
    // Conversion en network byte order
    uint16_t net_opcode = htons(ERROR); 
    uint16_t net_error_code = htons(error_code); 
    
    // copy of opcode
    memcpy(error_packet + offset, &net_opcode, sizeof(net_opcode));
    offset += sizeof(net_opcode);

    // copy of error code
    memcpy(error_packet + offset, &net_error_code, sizeof(net_error_code));
    offset += sizeof(net_error_code);

    // copy of error message 
    memcpy(error_packet + offset, error_msg, strlen(error_msg));
    offset += strlen(error_msg); 

    // adding the zero byte
    error_packet[offset++] = end_of_error_msg; //incrementing offset to get the real size
    *packet_size = offset; //assign the size
    return error_packet;
}
char* build_data_packet(uint16_t block_number, const char* data, size_t data_length, size_t* packet_size) {
    *packet_size = 4 + data_length; // Opcode (2 bytes) + Block number (2 bytes) + Data length
    char* packet = malloc(*packet_size);
    if (packet == NULL) {
        perror("malloc");
        return NULL;
    }

    uint16_t net_opcode = htons(DATA);
    uint16_t net_block_number = htons(block_number);

    int offset = 0;
    memcpy(packet + offset, &net_opcode, sizeof(net_opcode));
    offset += sizeof(net_opcode);

    memcpy(packet + offset, &net_block_number, sizeof(net_block_number));
    offset += sizeof(net_block_number);

    memcpy(packet + offset, data, data_length); // No need to adjust offset after this, as we're done

    return packet;
}
char* build_ack_packet(uint16_t block_number, size_t* packet_size) {
    *packet_size = 4; // Opcode (2 bytes) + Block number (2 bytes)
    char* packet = malloc(*packet_size);
    if (packet == NULL) {
        perror("malloc");
        return NULL;
    }

    uint16_t net_opcode = htons(ACK);
    uint16_t net_block_number = htons(block_number);

    int offset = 0;
    memcpy(packet + offset, &net_opcode, sizeof(net_opcode));
    offset += sizeof(net_opcode);

    memcpy(packet + offset, &net_block_number, sizeof(net_block_number));
    // No need to adjust offset after this, as we're done

    return packet;
}
int send_error_packet(config status,uint8_t error_code,char* error_msg, const struct sockaddr_in* client_addr, int sockfd){
    size_t packet_size;
    char* error_packet = build_error_packet(error_code, error_msg, &packet_size);
    if(error_packet == NULL){
        fprintf(stderr,"[send_error_packet] : failed to build error packet\n");
        return -1;
    }
    if (sendto(sockfd, error_packet, packet_size, 0, (struct sockaddr*)client_addr, sizeof(*client_addr)) == -1) {
        if(errno != EWOULDBLOCK && errno != EAGAIN){
            perror("[sendto]");
            free(error_packet); 
            return -1;
        }
    }
    if(status.trace){
        trace_sent(error_packet,packet_size);
    }
    free(error_packet); 
    return 0;
}
int send_data_packet(config status,uint16_t block_number,char* data, const struct sockaddr_in* client_addr,uint16_t data_length, int sockfd){
    size_t packet_size;
    char* data_packet = build_data_packet(block_number,data,data_length,&packet_size);
    if(data_packet == NULL){
        fprintf(stderr,"[send_data_packet] : failed to build data packet\n");
        return -1;
    }
    if (sendto(sockfd, data_packet, packet_size, 0, (struct sockaddr*)client_addr, sizeof(*client_addr)) == -1) {
        if(errno != EWOULDBLOCK && errno != EAGAIN){
            perror("[sendto]");
            free(data_packet); 
            return -1;
        }
    }
    if(status.trace){
        trace_sent(data_packet,packet_size);
    }
    free(data_packet); 
    return 0;
}
void print_error_message(char* error_packet){
    printf("[ERROR] Code = %d : Message = %s\n", get_error_code(error_packet), get_error_message(error_packet));
}
void trace_sent(char* packet,size_t packet_size){
    char* filename = NULL;
    char* transfer_mode = NULL;
    switch(get_opcode(packet)){
        case RRQ:{
            filename = get_file_name(packet);
            transfer_mode= get_mode(packet);
            printf("sent RRQ <file=%s, mode=%s>\n",filename,transfer_mode);
            free(filename);
            free(transfer_mode);
            break;
        }
        case WRQ:{
            filename = get_file_name(packet);
            transfer_mode = get_mode(packet);
            printf("sent WRQ <file=%s, mode=%s>\n",filename,transfer_mode);
            free(filename);
            free(transfer_mode);
            break;
        }
        case ACK:{
            printf("sent ACK <block=%d>\n",get_block_number(packet));
            break;
        }
        case DATA:{
            printf("sent DATA <block=%d, %ld bytes>\n",get_block_number(packet),packet_size);
            break;
        }
        case ERROR:{
            char* error_msg = get_error_message(packet);
            printf("sent ERROR <code=%d, msg=%s>\n",get_error_code(packet),error_msg);
            free(error_msg);
            break;
        }
        default:
            break;
    }
}

void trace_received(char* packet,size_t packet_size){
    char* filename = NULL;
    char* transfer_mode = NULL;
    char* error_msg = NULL;
    switch(get_opcode(packet)){
        case RRQ:
            filename = get_file_name(packet);
            transfer_mode= get_mode(packet);
            printf("received RRQ <file=%s, mode=%s>\n",filename,transfer_mode);
            free(filename);
            free(transfer_mode);
            break;
        case WRQ:
            filename = get_file_name(packet);
            transfer_mode = get_mode(packet);
            printf("received WRQ <file=%s, mode=%s>\n",filename,transfer_mode);
            free(filename);
            free(transfer_mode);
            break;
        case ACK:
            printf("received ACK <block=%d>\n",get_block_number(packet));
            break;
        case DATA:
            printf("received DATA <block=%d, %ld bytes>\n",get_block_number(packet),packet_size);
            break;
        case ERROR:
            error_msg = get_error_message(packet);
            printf("received ERROR <code=%d, msg=%s>\n",get_error_code(packet),get_error_message(packet));
            free(error_msg);
            break;
        default:
            break;
    }
}
int set_socket_timer(uint8_t sockfd,uint8_t time_sec, uint8_t time_usec){
    struct timeval timeout;
    timeout.tv_sec = time_sec;
    timeout.tv_usec = time_usec;
    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof timeout) < 0){
        perror("[setsockopt]\n");
        return -1;
    }
    return 0;
}
int check_packet(char* packet, int type,config status,const struct sockaddr_in* addr,int sockfd){
        if(get_opcode(packet) != type){
            // if it is an error packet we print error and we quit
            if (get_opcode(packet) == ERROR) {    
                print_error_message(packet);
                return -1 ;
            }
            // if its other type
            else {
                send_error_packet(status,NOT_DEFINED,"Unexpected packet",addr,sockfd);
                printf("[put] : Unexpected packet\n");
                return -1;
            }
        }
        return 0;
}