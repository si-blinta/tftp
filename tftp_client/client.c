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
    uint16_t net_opcode = htons(opcode);
    memcpy(packet + offset, &net_opcode, sizeof(net_opcode));
    offset += sizeof(net_opcode);

    // Copy the filename and mode
    memcpy(packet + offset, filename, strlen(filename));
    offset += strlen(filename);
    //zero byte
    packet[offset++] = end_of_file;

    memcpy(packet + offset, status.transfer_mode, strlen(status.transfer_mode));
    offset += strlen(status.transfer_mode);
    //zero byte
    packet[offset++] = end_of_file; //Increment offset so we can get the total size of the packet

    // Send the packet
    if (sendto(sockfd, packet, offset, 0, addr, sizeof(*addr)) == -1) {
        fprintf(stderr,"[request] : Failed request\n");
        perror("[sendto]");
        free(packet); 
        return -1;
    }
    if(status.trace){
        trace_sent(packet,packet_size);
    }
    free(packet); 
    return 0;
}

static void exit_tftp_client(int sockfd){
    close(sockfd);
    exit(0);
}

static void print_status(config status){
    printf("Connected to %s\n",status.server);
    printf("Mode: %s\n",status.transfer_mode);
    printf("Verbose: %d\n",status.verbose);
    printf("Tracing: %d\n",status.trace);
    printf("Rexmt-interval: %d seconds\n",status.rexmt);
    printf("Max-timeout: %d seconds\n",status.timemout);
}
static void process_command(config* status,const char* command,struct sockaddr_in addr,int sockfd) {
    if (strcmp(command, "put") == 0) {
        handle_put_command(*status,addr,sockfd);
    } else if (strcmp(command, "get") == 0) {
        handle_get_command(*status,addr,sockfd);
    } else if (strcmp(command, "?") == 0) {
        print_help();
    } else if (strcmp(command, "quit") == 0) {
        printf("Exiting TFTP client.\n");
        exit_tftp_client(sockfd);
    } else if (strcmp(command, "status") == 0) {
        print_status(*status);
    }else if (strcmp(command, "trace") == 0) {
        set_trace(status);
    }
     
     else {
        printf("Error: Unrecognized command. Type '?' for help.\n");
    }
}


static void handle_put_command(config status,struct sockaddr_in addr,int sockfd) {
    char filename[100];
    printf("Enter filename to send: ");
    scanf("%s", filename);
    send_file(filename,status,addr,sockfd);
}

static void handle_get_command(config status,struct sockaddr_in addr,int sockfd) {
    char filename[100];
    printf("Enter filename to receive: ");
    scanf("%s", filename);
    receive_file(filename,status,addr,sockfd);
}

static void print_help() {
    printf(
        "Commands available:\n"
        "  put     \tSend file to the server\n"
        "  get     \tReceive file from the server\n"
        "  status  \tShow current status\n"
        "  trace   \tToggle packet tracing\n"
        "  quit    \tExit TFTP client\n"
        "  ?       \tPrint this help information\n"
    );
}
static int connect_to_tftp_server(const char* server_ip, int server_port, int client_port,struct sockaddr_in* addr,int* sockfd) {
    struct sockaddr_in clientAddr;

    if ((*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[socket]");
        return -1;
    }
    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(client_port);
    clientAddr.sin_addr.s_addr = inet_addr(server_ip);

    // Bind the socket with the adress  : not necessary ! Useful for debugging when using WireShark
    if (bind(*sockfd, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0) {
        perror("[bind]");
        close(*sockfd);  
        return -1;
    }
    addr->sin_family = AF_INET;
    addr->sin_port = htons(server_port);
    addr->sin_addr.s_addr = inet_addr(server_ip); 
    return 0;
}
static void set_file_transfer_mode(const char* mode,char* transfer_mode){
        strcpy(transfer_mode,mode);
}
static void set_trace(config* status){
        if(status->trace){
            status->trace = 0;
            printf("Packet tracing off\n");
        }else{
            status->trace = 1;
            printf("Packet tracing on\n");
        }

}
static int send_file(const char* filename,config status,struct sockaddr_in addr, int sockfd) {
    FILE* requested_file = fopen(filename, "rb");
    if (requested_file == NULL) {
        perror("Failed to open file");
        return -1 ; 
    }

    if(request(WRQ, filename, status, sockfd, (struct sockaddr*)&addr) == -1){
        return -1;
    }
    char ack_packet[MAX_BLOCK_SIZE];
    char data_packet[MAX_BLOCK_SIZE];
    size_t packet_size, bytes_read;
    socklen_t len = sizeof(addr);
    int block_number = 0;

    // Wait for the initial ACK for the WRQ
    size_t bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, (struct sockaddr*)&addr, &len);
    if (bytes_received == -1) {
        perror("[recvfrom]");
        fclose(requested_file);
        return -1 ;
    }
    if(status.trace){
        trace_received(ack_packet,bytes_received);
    }

    if (get_opcode(ack_packet) == ERROR) {    
            print_error_message(ack_packet);
            fclose(requested_file);
            return 0 ;
        }
    while ((bytes_read = fread(data_packet, 1, 512, requested_file)) > 0) {
        block_number++;
        if(send_data_packet(status,block_number,data_packet,&addr,bytes_read,sockfd) == -1){
            return -1;
        }
        bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, (struct sockaddr*)&addr, &len);
        if (bytes_received == -1) {
            perror("[recvfrom]");
            fclose(requested_file);
            return -1 ;
        }
        if(status.trace){
            trace_received(ack_packet,bytes_received);
        }

        if (get_opcode(ack_packet) == ERROR) {    
            print_error_message(ack_packet);
            fclose(requested_file);
            return 0; 
        }
    }

    fclose(requested_file);
    return 0;
}
static int receive_file(const char* filename, config status ,struct sockaddr_in addr,int sockfd) {
    size_t total_bytes_received = 0;
    time_t start = time(NULL);
    if(request(RRQ, filename, status, sockfd, (struct sockaddr*)&addr) == -1){
        return -1;
    }
    char packet[MAX_BLOCK_SIZE];
    size_t packet_size;
    FILE* requested_file = fopen(filename, "wb");
    if (!requested_file) {
        perror("Failed to open file");
        fclose(requested_file);
        return -1;
    }
    socklen_t len = sizeof(addr);
    while (1) {
        size_t bytes_received = recvfrom(sockfd, packet, sizeof(packet), 0, (struct sockaddr*)&addr, &len);
        if (bytes_received == -1) {
            perror("[recvfrom]");
            fclose(requested_file);
            return -1;
        }
        total_bytes_received+= bytes_received;
        if(status.trace){
            trace_received(packet,bytes_received);
        }
        if(get_opcode(packet) == ERROR){
            printf("[get] : error code = %d : error message : %s\n",get_error_code(packet),get_error_message(packet));
            if(remove(filename)){
                perror("[remove]");
            }
            return -1;
        }
        fwrite(get_data(packet), 1, bytes_received - 4, requested_file); 
        if(send_ack_packet(status,(struct sockaddr*)&addr,get_block_number(packet),sockfd) == -1){
            return -1;
        }
        // Check if this is the last data block
        if (bytes_received < MAX_BLOCK_SIZE) {
            break;
        }
    }
    fclose(requested_file);
    time_t end = time(NULL);
    printf("Received %ld bytes in %lf secondes\n",total_bytes_received,difftime(end, start));
    return 0;
}



















int main(int argc, char const* argv[]) {
    config status = {.server = IP,.transfer_mode = "netascii",.verbose = 0,.trace = 0,.rexmt = 0,.timemout = 0};
    int sockfd;
    struct sockaddr_in addr;
    if (connect_to_tftp_server(IP,SERVER_PORT,CLIENT_PORT,&addr,&sockfd) == -1) {
        printf("[connect_to_tftp_server] : erreur");
    }
    char command[100];
    while(1){
        printf("<tftp> ");
        scanf("%s",command);
        process_command(&status,command,addr,sockfd);

    }
   	

}
