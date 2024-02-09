#include "server.h"


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
static void handle_client_requests(config status,int sockfd,struct sockaddr_in* addr,struct sockaddr_in* client_addr){
    char packet[MAX_BLOCK_SIZE];
    socklen_t len = sizeof(*client_addr);
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
        process_rrq(status,filename,mode,client_addr,sockfd);
        free(filename);
        free(mode);
        break;
    case WRQ :
        filename = get_file_name(packet);
        mode = get_mode(packet);
        process_wrq(status,filename,mode,client_addr,sockfd);
        free(filename);
        free(mode);
    default:
        break;
    }    
}
static int process_rrq(config status,char* filename,char* mode, const struct sockaddr_in* client_addr, int sockfd){
    char ack_packet[516]; 
    size_t packet_size;
    FILE* requested_file;
    //TODO : gerer le cas de netascii
    if(!strcasecmp(mode,"octet")){
        requested_file = fopen(filename, "rb");
    }
   
    if (requested_file == NULL) {
        send_error_packet(status,FILE_NOT_FOUND,"File does'nt exist",client_addr,sockfd);
        return -1;
    }

    char data[512];
    size_t bytes_read = 0;
    int block_number = 1;
    socklen_t len = sizeof(*client_addr);
    while ((bytes_read = fread(data, 1, 512, requested_file)) > 0) {
        send_data_packet(status,block_number++,data,client_addr,bytes_read,sockfd);
        // Receive the ACK packet for the current block_number
        size_t bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, (struct sockaddr*)client_addr, &len);
        if ( bytes_received== -1) {
            perror("[recvfrom]");
            fclose(requested_file); 
            return -1;
            
        }
        if(status.trace){
            trace_received(ack_packet,bytes_received);
        }
        if (get_opcode(ack_packet) == ERROR) {    
            print_error_message(ack_packet);
            break;
        }
    }

    fclose(requested_file); 
    return 0;
}
static int process_wrq(config status,char* filename, char* mode, const struct sockaddr_in* client_addr, int sockfd) {
    FILE* received_file = fopen(filename,"r");
    if( received_file == NULL){
        //the file doesnt exist:
        send_error_packet(status,ACCESS_VIOLATION,"Acess violation",client_addr,sockfd);
        return 0;
    }
    fclose(received_file); 
    //TODO : gerer le cas de netascii
    if(!strcasecmp(mode,"octet")){
        received_file = fopen(filename, "wb"); 
    }
    if (received_file == NULL) {
        send_error_packet(status,NOT_DEFINED, "Unexpected error while opening file", client_addr, sockfd);
        return -1;
    }

    // Send the first ACK for WRQ to start receiving data
    socklen_t len = sizeof(*client_addr);
    if(send_ack_packet(status,(struct sockaddr*)client_addr,0,sockfd) == -1){
        return -1;
    }
    char data_packet[516];
    size_t bytes_received; 
    while (1) { 
        bytes_received = recvfrom(sockfd, data_packet, sizeof(data_packet), 0, (struct sockaddr*)client_addr, &len);
        if(bytes_received == -1){
            perror("[recvfrom]");
            fclose(received_file); 
            return -1;
        }
        if(status.trace){
            trace_received(data_packet,bytes_received);
        }
        fwrite(data_packet + 4, 1, bytes_received - 4, received_file);
        if(send_ack_packet(status,(struct sockaddr*)client_addr,get_block_number(data_packet),sockfd) == -1){
            return -1;
        }
        if(bytes_received < 516){
            break;
        }
    }
    fclose(received_file);
    return 0;
}

int main(int argc, char**argv)
{   
    config status = {.server = NULL,.transfer_mode = NULL,.verbose = 0,.trace = 1,.rexmt = 0,.timemout = 0};
    struct sockaddr_in addr,client_addr;
    int sockfd;
    if(init_tftp_server(SERVER_PORT,&sockfd,&addr) == -1){
        printf("[init_udp_socket] : erreur\n");
    }
    while (1)
    {
     handle_client_requests(status,sockfd,&addr,&client_addr);
    }
    
}

