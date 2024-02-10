#include "utils.h"
uint16_t get_opcode(char* packet){
    uint16_t opcode;
    memcpy(&opcode, packet, sizeof(opcode));
    return ntohs(opcode); // Convertit de network byte order à host byte order
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
void print_request_packet(char* packet){
    uint16_t opcode = get_opcode(packet);
    char* filename = get_file_name(packet);
    char* mode = get_mode(packet);
    printf("NEW REQUEST\n");
    printf("______________________________________________________________________\n");
    printf("opcode   : %u\n",opcode);
    printf("filename : %s\n",filename);
    printf("mode     : %s\n",mode);
    printf("______________________________________________________________________\n");
    free(filename);
}
void print_error_packet(char* packet){
    uint16_t opcode = get_opcode(packet);
    uint16_t error_code = get_error_code(packet);
    char* error_message = get_error_message(packet);
    printf("%u\n",opcode);
    printf("%u\n",error_code);
    printf("%s\n",error_message);
    free(error_message);
}
uint16_t get_error_code(char* packet){
    uint16_t error_code;
    memcpy(&error_code, packet + 2, sizeof(error_code)); 
    return ntohs(error_code); // Conversion nécessaire
}
char* get_error_message(char* packet){
    char* error_msg = strdup(packet+4);
    return error_msg;

}
uint16_t get_block_number(char* packet){
    uint16_t block_number;
    memcpy(&block_number, packet + 2, sizeof(block_number));
    return ntohs(block_number); // Conversion nécessaire
}
char* get_data(char* packet){
    char* data = strdup(packet+4);
    return data;

}
void print_data_packet(char* packet){
    uint16_t opcode = get_opcode(packet);
    uint16_t block_number = get_block_number(packet);
    char* data = get_data(packet);
    printf("%u\n",opcode);
    printf("%u\n",block_number);
    printf("%s\n",data);
    free(data);
}
size_t convert_to_netascii(char* buffer) {
    size_t i, j;
    size_t input_length = strlen(buffer); 
    for (i = 0, j = 0; i < input_length; ++i, ++j) {
        if (buffer[i] == '\n') {
            buffer[j++] = '\r'; 
        }
        buffer[j] = buffer[i];
    }
    buffer[j] = '\0'; 
    return j; 
}
int send_ack_packet(config status,const struct sockaddr* client_addr, uint16_t block_number, int sockfd) {
    size_t packet_size;
    char* ack_packet = build_ack_packet(block_number, &packet_size);

    if (ack_packet == NULL) {
        fprintf(stderr,"[send_ack_packet] : failed to build ack packet\n");
        return -1; 
    }

    size_t bytes_sent = sendto(sockfd, ack_packet, packet_size, 0, client_addr, sizeof(*client_addr));


    if (bytes_sent < 0) {
        perror("Failed to send ACK packet");
        return -1;
    }
    if(status.trace){
        trace_sent(ack_packet,packet_size);
    }
    free(ack_packet);
    return 0;
}
void convert_netascii_to_native(char *data, int *length) {
    if(strcmp(PLATFORM_NAME, "windows") == 0) {
        return; // No conversion needed for Windows, as it already uses CR LF.
    }
    int i, j;
    // Convert CR LF to LF for Linux/Unix
    for (i = 0, j = 0; i < *length; ++i, ++j) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            i++; // Skip CR in CR LF
        }
        data[j] = data[i];
    }
    *length = j; // Update the length after conversion
    data[j] = '\0'; // Ensure the result is null-terminated
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
int send_error_packet(config status,int error_code,char* error_msg, const struct sockaddr_in* client_addr, int sockfd){
    size_t packet_size;
    char* error_packet = build_error_packet(error_code, error_msg, &packet_size);
    if(error_packet == NULL){
        fprintf(stderr,"[send_error_packet] : failed to build error packet\n");
        return -1;
    }
    if (sendto(sockfd, error_packet, packet_size, 0, (struct sockaddr*)client_addr, sizeof(*client_addr)) == -1) {
        perror("[sendto]");
        free(error_packet); 
        return -1;
    }
    if(status.trace){
        trace_sent(error_packet,packet_size);
    }
    free(error_packet); 
    return 0;
}
int send_data_packet(config status,int block_number,char* data, const struct sockaddr_in* client_addr,int data_length, int sockfd){
    size_t packet_size;
    char* data_packet = build_data_packet(block_number,data,data_length,&packet_size);
    if(data_packet == NULL){
        fprintf(stderr,"[send_data_packet] : failed to build data packet\n");
        return -1;
    }
    if (sendto(sockfd, data_packet, packet_size, 0, (struct sockaddr*)client_addr, sizeof(*client_addr)) == -1) {
        perror("[sendto]");
        free(data_packet); 
        return -1;
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
            char* error_msg = get_error_message(packet);
            printf("received ERROR <code=%d, msg=%s>\n",get_error_code(packet),get_error_message(packet));
            free(error_msg);
            break;
        default:
            break;
    }
}
