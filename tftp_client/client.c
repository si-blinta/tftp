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
        trace_sent(packet,packet_size,-1);
    }
    free(packet); 
    return 0;
}

static void exit_tftp_client(int sockfd){
    close(sockfd);
    exit(0);
}
static void print_status(config status){
    printf("Connected to %s\n",status.server_ip);
    printf("Mode: %s\n",status.transfer_mode);
    if(status.trace){
        printf("Tracing: On\n");
    }
    else{
        printf("Tracing: Off\n");
    }
    printf("Per Packet timeout: %d seconds\n",status.per_packet_time_out);
    printf("Timeout: %d seconds\n",status.timemout);
}
static int process_command(char* command,config* status,struct sockaddr_in server_addr,int sockfd) {
    /**
     * Very important ! 
     * here we pass a pointer to the server_adress to handle_put and handle_get commands , note that this function
     * can't modify the value of server_address but instead sends a copy to put and get so they can modify it 
     * without a problem.
     * every time we execute a new command , the port is the same ! 
     * but in every put/get command the port is changing because of the tftp server using differents ports to handle requests.
     * 
    */
    
    if (strcmp(command, "put") == 0) {
        handle_put_command(*status,&server_addr,sockfd);
    } else if (strcmp(command, "get") == 0) {
        handle_get_command(*status,&server_addr,sockfd);
    }
     else if (strcmp(command, "?") == 0) {
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


static void handle_put_command(config status,struct sockaddr_in* server_addr,int sockfd) {
    char filename[100];
    printf("(file)");
    scanf("%s", filename);
    send_file(filename,status,server_addr,sockfd);
}

static void handle_get_command(config status,struct sockaddr_in* server_addr,int sockfd) {
    char filename[100];
    printf("(file)");
    scanf("%s", filename);
    receive_file(filename,status,server_addr,sockfd);
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
static int connect_to_tftp_server(int per_packet_timout,const char* server_ip, int server_port,struct sockaddr_in* server_addr,int* sockfd) {

    if ((*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[socket]");
        return -1;
    }
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(server_port);
    server_addr->sin_addr.s_addr = inet_addr(server_ip); 
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
static int send_file(const char* filename,config status,struct sockaddr_in* server_addr, int sockfd) {
     /**
    *   Make recvfrom non blocking using a time out which the value is the amount of time to wait
    *   before resending the data packet, assuming that it got lost.
    */
    set_socket_timer(sockfd,status.per_packet_time_out,0);
    uint8_t timeout = 0;                //the time out in which we wait for the same ack before quiting the program.
    char ack_packet[MAX_BLOCK_SIZE];    // buffer to store ack packet
    memset(ack_packet, 0, sizeof(ack_packet));
    FILE* requested_file = NULL;  
    size_t total_bytes_sent = 0;    //keep track of total bytes sent
    //TODO : gerer le netascii
    if(!strcasecmp(status.transfer_mode,"octet")){
         requested_file= fopen(filename, "rb");
    }
    if (requested_file == NULL) {
        perror("Failed to open file");
        return -1 ; 
    }
    time_t start = time(NULL); 
    //Send a WRQ request
    if(request(WRQ, filename, status, sockfd, (struct sockaddr*)server_addr) == -1){
        return -1;
    }
    
    char data[MAX_BLOCK_SIZE-4];        // buffer to read data from a file
    memset(data, 0, sizeof(data));
    size_t bytes_read;                  // bytes_read using fread
    socklen_t len = sizeof(*server_addr);       // size of the server adress
    size_t bytes_received = 0;
    uint16_t block_number = 1;               // the current block 
 
    // Wait for the initial ACK for the WRQ
    bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, (struct sockaddr*)server_addr, &len);
    if (bytes_received == -1) {
        perror("[recvfrom]");
        fclose(requested_file);
        return -1 ;
    }
    if(status.trace){
        trace_received(ack_packet,bytes_received,-1);
    }

    if (get_opcode(ack_packet) == ERROR) {    
            print_error_message(ack_packet);
            fclose(requested_file);
            return 0 ;
        }  
    while ((bytes_read = fread(data, 1, MAX_BLOCK_SIZE-4, requested_file)) > 0) {
        printf("[packet loss] sending data#%d\n",block_number);
        if(!packet_loss(status.packet_loss_percentage)){      
            if(send_data_packet(status,block_number,data,server_addr,bytes_read,sockfd,-1) == -1){
                    fclose(requested_file);
                    return -1;
                }
            total_bytes_sent+= bytes_read;  //Increment bytes sent     
        }
        bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, (struct sockaddr*)server_addr, &len);
        while (bytes_received == -1) {
            //Error recvfrom
            if(errno != EWOULDBLOCK && errno != EAGAIN){
                perror("[recvfrom]");
                fclose(requested_file);
                return -1 ;
            }
            // Per packet time out reached : Didnt receive ack packet
            // Resend data
            printf("[packet loss] sending data#%d\n",block_number);
            if(!packet_loss(status.packet_loss_percentage)){
                if(send_data_packet(status,block_number,data,server_addr,bytes_read,sockfd,-1) == -1){
                    fclose(requested_file);
                    return -1;
                }
            }
            // Wait for ack again
            bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0,(struct sockaddr*)server_addr, &len);

            //Increment the time out for the actual ack packet.
            timeout+=status.per_packet_time_out;
            //Check if we exceeded the time out for the same packet.
            if(timeout >= status.timemout){
                printf("[put] : time out reached : %d seconds\n",timeout);
                fclose(requested_file);
                return -1;
            }
        }
        //Check for unexpected packets
        if(check_packet(ack_packet,ACK,status,server_addr,sockfd,-1) == -1){
            fclose(requested_file);
            return -1;
        }
        // An ACK packet is received :
        timeout = 0;                //Reset time out , because a new data block is about to be sent
        block_number++ ;           //Increment block number
        if(status.trace){
            trace_received(ack_packet,bytes_received,-1);
        }
    }
    time_t end = time(NULL);
    printf("Sent %ld bytes in %lf secondes\n",total_bytes_sent,difftime(end, start));
    fclose(requested_file);
    return 0;
}
static int receive_file(const char* filename, config status ,struct sockaddr_in* server_addr,int sockfd) {
    /**
    *   Make recvfrom non blocking using a time out which the value is the amount of time to wait
    *   when receiving the same packet before quiting the program.
    */
    set_socket_timer(sockfd,status.timemout,0);
    FILE* requested_file = NULL;
    // TODO : gerer le cas netascii
    if(!strcasecmp(status.transfer_mode,"octet")){
        requested_file= fopen(filename, "wb");
    }
    if (!requested_file) {
        perror("Failed to open file");
        fclose(requested_file);
        return -1;
    }
    size_t total_bytes_received = 0;
    size_t bytes_received = 0 ;
    char packet[MAX_BLOCK_SIZE];
    memset(packet, 0, sizeof(packet));
    socklen_t len = sizeof(*server_addr);   
    uint16_t last_block_number_received = 0; // starts with a number != 1
    time_t start = time(NULL);
    // Send RRQ 
    if(request(RRQ, filename, status, sockfd, (struct sockaddr*)server_addr) == -1){
        return -1;
    }
    while (1) {
        bytes_received = recvfrom(sockfd, packet, sizeof(packet), 0, (struct sockaddr*)server_addr, &len);        
        if(bytes_received == -1) {
            //Actual error when using recvfrom
            if(errno != EWOULDBLOCK && errno != EAGAIN){
                perror("[recvfrom][receive_file]");
                fclose(requested_file);
                return -1;
            }
            //Time out occured
            printf("[get]: FAILED : time out reached \n");
            fclose(requested_file);
            return -1;                
        }
        if(status.trace){
            trace_received(packet,bytes_received,-1);
        }
        if(check_packet(packet,DATA,status,server_addr,sockfd,-1) == -1){
            fclose(requested_file);
            return -1;
        }
        printf("[packet loss] sending ack %d\n",get_block_number(packet));
        if(!packet_loss(status.packet_loss_percentage)){
            if(send_ack_packet(status,(struct sockaddr*)server_addr,get_block_number(packet),sockfd,-1) == -1){
                return -1;
            }
        }
        if(last_block_number_received != get_block_number(packet)){
            fwrite(get_data(packet), 1, bytes_received - 4, requested_file);
            last_block_number_received = get_block_number(packet);  // update last block #
            total_bytes_received+= bytes_received-4; // increments bytes received if its new data packet
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
    srand(time(NULL));
    if(argc < 4){
        printf("USAGE ./client [server ip] [server port] [packet loss percentage (between 0 and 100)]\n");
        return 0;
    }
    config status = {.server_ip = (char*)argv[1],.transfer_mode = "octet",.trace = 0,.per_packet_time_out = 1,.timemout = 10,.packet_loss_percentage = (uint8_t)atoi(argv[3])};
    int server_port = atoi(argv[2]);
    int sockfd;
    struct sockaddr_in server_addr;
    if (connect_to_tftp_server(status.per_packet_time_out,status.server_ip,server_port,&server_addr,&sockfd) == -1) {
        printf("[connect_to_tftp_server] : erreur");
    }
    char command[100];
    while(1){
        printf("<tftp>");
        scanf("%s",command);
        if(process_command(command,&status,server_addr,sockfd) == -1)break;
    }  	
}
