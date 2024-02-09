#include "server.h"
#define DEBUG 1

static int init_tftp_server(int port,int* sockfd,struct sockaddr_in* addr) {
    if ((*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[socket]");
        return -1;
    }
    (*addr).sin_family = AF_INET;
    (*addr).sin_port = htons(port);
    (*addr).sin_addr.s_addr = INADDR_ANY;

    if (bind(*sockfd, (struct sockaddr*)addr, sizeof(*addr)) < 0) {
        perror("[bind]");
        return -1;
    }
    return 0;
}
static void handle_client_requests(int sockfd,struct sockaddr_in* addr,struct sockaddr_in* client_addr){
    char packet[MAX_BLOCK_SIZE];
    socklen_t len = sizeof(addr);
    if(recvfrom(sockfd,(char* )packet,516,0,(struct sockaddr *) client_addr,&len) < 0){
        perror("[recvfrom]");
    }
    
    uint16_t opcode = get_opcode(packet);
    char* filename ;
    char* mode;
    switch (opcode)
    {
    case RRQ:
        filename = get_file_name(packet);
        mode = get_mode(packet);
#ifdef DEBUG
        print_request_packet(packet);
#endif //DEBUG        
        process_rrq(filename,mode,client_addr,sockfd);
        free(filename);
        free(mode);
        break;
    case WRQ :
        filename = get_file_name(packet);
        mode = get_mode(packet);
#ifdef DEBUG
        print_request_packet(packet);
#endif //DEBUG   
        process_wrq(filename,mode,client_addr,sockfd);
        free(filename);
        free(mode);
    default:
        break;
    }    
}
static int process_rrq(char* filename,char* mode, const struct sockaddr_in* client_addr, int sockfd){
    char ack_packet[516]; 
    size_t packet_size;
    FILE* requested_file = fopen(filename, "rb");

    if (requested_file == NULL) {
        send_error_packet(FILE_NOT_FOUND,"File does'nt exist",client_addr,sockfd);
        return -1;
    }

    char data[512];
    size_t bytes_read = 0;
    int block_number = 1;
    socklen_t len = sizeof(*client_addr);
#ifdef DEBUG
printf("______________________________________________________________________\n");
#endif //DEBUG   
    while ((bytes_read = fread(data, 1, 512, requested_file)) > 0) {
        char* data_packet = build_data_packet(block_number++, data, bytes_read, &packet_size);
        if (sendto(sockfd, data_packet, packet_size, 0, (struct sockaddr*)client_addr, len) == -1) {
            perror("[sendto]");
            free(data_packet);
            fclose(requested_file); 
            return -1;
        }
        free(data_packet);
        // Receive the ACK packet for the current block_number
        if (recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, (struct sockaddr*)client_addr, &len) == -1) {
            perror("[recvfrom]");
            fclose(requested_file); 
            return -1;
            
        }
#ifdef DEBUG     
        if (get_opcode(ack_packet) == ACK && get_block_number(ack_packet) == block_number-1) {
   
            printf("ACK %d\n", block_number-1);
#endif //DEBUG       
        } 
        if (get_opcode(ack_packet) == ERROR) {    
            print_error_message(ack_packet);
            break; // Exit the loop on error
        }
    }

    fclose(requested_file); 
#ifdef DEBUG
        printf("______________________________________________________________________\n");
        printf("[get] : %s is sent\n",filename);
        printf("______________________________________________________________________\n");
#endif //DEBUG  

    return 1;
}
static int process_wrq(char* filename, char* mode, const struct sockaddr_in* client_addr, int sockfd) {
    FILE* file_exist = fopen(filename,"r");
    if( file_exist != NULL){
        //the file exists
        fclose(file_exist);
        send_error_packet(FILE_ALREADY_EXISTS,"File already exists",client_addr,sockfd);
        return 0;
    } 
    FILE* received_file = fopen(filename, "wb"); 
    if (received_file == NULL) {
        send_error_packet(NOT_DEFINED, "Unexpected error while opening file", client_addr, sockfd);
        return -1;
    }

    // Send initial ACK for WRQ to start receiving data
    socklen_t len = sizeof(*client_addr);
    if(send_ack_packet((struct sockaddr*)client_addr,0,sockfd) == -1){
        return -1;
    }
    char data_packet[516];
    size_t bytes_received; 

    while (1) { 
        bytes_received = recvfrom(sockfd, data_packet, sizeof(data_packet), 0, (struct sockaddr*)client_addr, &len);
        uint16_t received_block_number = get_block_number(data_packet);
        // Write received data to file
        fwrite(data_packet + 4, 1, bytes_received - 4, received_file);
        if(send_ack_packet((struct sockaddr*)client_addr,received_block_number,sockfd) == -1){
            return -1;
        }
        if(bytes_received < 516){
            break;
        }
    }

    fclose(received_file);
#ifdef DEBUG
        printf("______________________________________________________________________\n");
        printf("[put] : received %s \n",filename);
        printf("______________________________________________________________________\n");
#endif //DEBUG
    return 0;
}

int main(int argc, char**argv)
{   
    struct sockaddr_in addr,client_addr;
    int sockfd;
    if(init_tftp_server(SERVER_PORT,&sockfd,&addr) == -1){
        printf("[init_udp_socket] : erreur\n");
    }
    while (1)
    {
     handle_client_requests(sockfd,&addr,&client_addr);
    }
    
}

