#include "server.h"
#define DEBUG 1

static int init_tftp_server(int port,int* sockfd,struct sockaddr_in* addr) {
    if ((*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[socket]");
        return -1;
    }
   	/*struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	if (setsockopt(*sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
    		perror("Error");
    }*/
    (*addr).sin_family = AF_INET;
    (*addr).sin_port = htons(port); // Specify the port as an argument
    (*addr).sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces

    if (bind(*sockfd, (struct sockaddr*)addr, sizeof(*addr)) < 0) {
        perror("[bind]");
        return -1;
    }
    return 0;
}
static void handle_client_requests(int sockfd,struct sockaddr_in* addr,struct sockaddr_in* client_addr){
    char buf[MAX_BLOCK_SIZE];
    socklen_t len = sizeof(addr);
    if(recvfrom(sockfd,(char* )buf,516,0,(struct sockaddr *) client_addr,&len) < 0){
    	//timeout reached
    	//exit(1);
    }
    
    uint16_t opcode = get_opcode(buf);
    char* filename ;
    char* mode;
    switch (opcode)
    {
    case RRQ:
        filename = get_file_name(buf);
        mode = get_mode(buf);
#ifdef DEBUG
        print_request_packet(buf);
#endif //DEBUG        
        process_rrq(filename,mode,client_addr,sockfd);
        free(filename);
        free(mode);
        break;
    case WRQ :
        filename = get_file_name(buf);
        mode = get_mode(buf);
#ifdef DEBUG
        print_request_packet(buf);
#endif //DEBUG   
        process_wrq(filename,mode,client_addr,sockfd);
        free(filename);
        free(mode);
    default:
        break;
    }    
}
static int process_rrq(char* filename,char* mode, const struct sockaddr_in* client_addr, int sockfd){
    char buf[516]; 
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
        if (recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)client_addr, &len) == -1) {
            perror("[recvfrom]");
            fclose(requested_file); 
            return -1;
            
        }
#ifdef DEBUG     
        if (get_opcode(buf) == ACK && get_block_number(buf) == block_number-1) {
   
            printf("ACK %d\n", block_number-1);
#endif //DEBUG       
        } 
        if (get_opcode(buf) == ERROR) {    
            print_error_message(buf);
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
    if(send_ack_packet(sockfd,(struct sockaddr*)client_addr,len,0) == -1){
        return -1;
    }
    char buf[516];
    size_t bytes_received; 

    while (1) { 
        bytes_received = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)client_addr, &len);
        uint16_t received_block_number = get_block_number(buf);
        // Write received data to file
        fwrite(buf + 4, 1, bytes_received - 4, received_file);
        if(send_ack_packet(sockfd,(struct sockaddr*)client_addr,len,received_block_number) == -1){
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
    if(init_tftp_server(PORT,&sockfd,&addr) == -1){
        printf("[init_udp_socket] : erreur\n");
    }
    while (1)
    {
     handle_client_requests(sockfd,&addr,&client_addr);
    }
    
}

