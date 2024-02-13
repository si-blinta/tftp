#include "server.h"


static int init_tftp_server(int port,int* sockfd) {
    //Create socket with SOCK_DGRAM
    if ((*sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[socket]");
        return -1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    // Bind the socket with the adress
    if (bind(*sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[bind]");
        return -1;
    }
    return 0;
}
static int handle_client_requests(config status,int sockfd,struct sockaddr_in* client_addr){
    //NOT SECURE : imagine if i send a RRQ/WRQ with filename / mode size very big :) 
    char packet[MAX_BLOCK_SIZE];
    socklen_t len = sizeof(*client_addr);
    if(recvfrom(sockfd,(char* )packet,MAX_BLOCK_SIZE,0,(struct sockaddr *) client_addr,&len) < 0){
        if(errno != EAGAIN && errno != EWOULDBLOCK){
            perror("[recvfrom][handle_client_requests]");
            return -1;
        }
    return 0;    
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
    return 0;    
}
static int process_rrq(config status,char* filename,char* mode, const struct sockaddr_in* client_addr, int sockfd){
   /**
    *   Make recvfrom non blocking using a time out which the value is the amount of time to wait
    *   before resending the data packet, assuming that it got lost.
    */
    struct timeval per_packet_timeout;      
    per_packet_timeout.tv_sec = status.per_packet_time_out ;
    per_packet_timeout.tv_usec = 0;
    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, &per_packet_timeout,sizeof per_packet_timeout) < 0){
        perror("[setsockopt][process_rrq]\n");
    } 
    int timeout = 0;        //the time out in which we wait for the same ack before quiting the program.
    char ack_packet[MAX_BLOCK_SIZE];   // ack packet
    FILE* requested_file = NULL;
    char path[100] = SERVER_DIRECTORY;
    strcat(path,filename);
    //TODO : gerer le cas de netascii
    if(!strcasecmp(mode,"octet")){
        requested_file = fopen(path, "rb");
    }
    if (requested_file == NULL) {
        send_error_packet(status,FILE_NOT_FOUND,"File does not exist",client_addr,sockfd);
        return -1;
    }
    char data[MAX_BLOCK_SIZE-4];             // The buffer to store file bytes with fread.
    size_t bytes_read = 0;      //bytes read from fread
    size_t bytes_received = 0;  //bytes received from recvfrom
    int block_number = 1;       //current data block#

    while ((bytes_read = fread(data, 1, MAX_BLOCK_SIZE-4, requested_file)) > 0) {
        printf("[packet loss] sending data#%d\n",block_number);
        if(!packet_loss(PACKET_LOSS_PERCENTAGE)){
            send_data_packet(status,block_number,data,client_addr,bytes_read,sockfd);
        }
        bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, NULL, 0);
        while (bytes_received == -1) { 
            //Error recvfrom
            if(errno != EAGAIN && errno != EWOULDBLOCK){
                perror("[recvfrom]");
                fclose(requested_file); 
                return -1;
            }
            // Per packet time out reached : Didnt receive ack packet
            // Resend data
            printf("[packet loss] sending data#%d\n",block_number);
            if(!packet_loss(PACKET_LOSS_PERCENTAGE)){
                send_data_packet(status,block_number,data,client_addr,bytes_read,sockfd);
            }
            // Wait for ack again
            bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, NULL, 0);

            //Increment the time out for the actual ack packet.
            timeout+=status.per_packet_time_out;
            //Check if we exceeded the time out for the same packet.
            if(timeout >= status.timemout){
                printf("RRQ FAILED : time out reached : %d seconds\n",timeout);
                fclose(requested_file);
                return -1;
            }
        }
        // An ACK packet is received :
        timeout = 0;                //Reset time out , because a new data block is about to be sent
        block_number++ ;           //Increment block number
        if(status.trace){
            trace_received(ack_packet,bytes_received);
        }
        // If the packet is not an ack
        if(get_opcode(ack_packet) != ACK){
            // if it is an error packet we print error and we quit
            if (get_opcode(ack_packet) == ERROR) {    
                print_error_message(ack_packet);
                break;
            }
            // if its other type
            else {
                if(send_error_packet(status,NOT_DEFINED,"Expected Ack packet",&client_addr,sockfd)){
                    fclose(requested_file);
                    return -1;
                }
                printf("RRQ FAILED : Unexpected packet\n");
                fclose(requested_file);
                return -1;
            }
        }
    }
    fclose(requested_file); 
    printf("RRQ SUCCESS\n");
    return 0;
    
}
static int process_wrq(config status,char* filename, char* mode, const struct sockaddr_in* client_addr, int sockfd) {
    /**
    *   Make recvfrom non blocking using a time out which the value is the amount of time to wait
    *   when receiving the same packet before quiting the program.
    */
    struct timeval timeout;
    timeout.tv_sec = status.timemout;
    timeout.tv_usec = 0;
    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof timeout) < 0){
        perror("[setsockopt]\n");
    }
    char path[100] = SERVER_DIRECTORY;
    strcat(path,filename); 
    FILE* received_file = NULL;
   
    //TODO : gerer le cas de netascii
    if(!strcasecmp(mode,"octet")){
        received_file = fopen(path, "wb"); 
    }
    //If opening file fails , we send a not defined error, maybe later we will implement permissions ! 
    if (received_file == NULL) {
        send_error_packet(status,NOT_DEFINED, "Unexpected error while opening file", client_addr, sockfd);
        return -1;
    }
    int last_block_number_received = 0; // to keep track of the last block# in case of packet loss.
    char data_packet[MAX_BLOCK_SIZE];              // where to store the data packet.
    size_t bytes_received;              // size of bytes received from the client. 
    
    // Send the ACK0 to the client
    if(send_ack_packet(status,(struct sockaddr*)client_addr,0,sockfd) == -1){
        return -1;
    }
    //WRQ loop
    while (1) { 
        //Receive the data packet , no need to store client address
        bytes_received = recvfrom(sockfd, data_packet, sizeof(data_packet), 0, NULL, 0);
        if(bytes_received == -1){
            //Actual error when using recvfrom
            if(errno != EAGAIN && errno != EWOULDBLOCK){
                perror("[recvfrom]");
                fclose(received_file); 
                return -1;
            }
            //Time out occured
            printf("WRQ FAILED : time out reached \n");
            fclose(received_file);
            return -1;            
        }
        //Tracing
        if(status.trace){
            trace_received(data_packet,bytes_received);
        }
        printf("[packet loss] sending ack#%d\n",get_block_number(data_packet));
        //if there is no packet loss we send the ack.
        if(!packet_loss(PACKET_LOSS_PERCENTAGE)){
            if(send_ack_packet(status,(struct sockaddr*)client_addr,get_block_number(data_packet),sockfd) == -1){
                return -1;
            }
        }
        //write the data if the data packet received is different from the last one
        if(last_block_number_received != get_block_number(data_packet)){
            fwrite(get_data(data_packet), 1, bytes_received - 4, received_file);
            last_block_number_received = get_block_number(data_packet);
        }
        //break if its the last data packet
        if(bytes_received < MAX_BLOCK_SIZE){
            break;
        }
    }
    fclose(received_file);
    printf("WRQ SUCCESS\n");
    return 0;
}

int main(int argc, char**argv)
{   
    srand(time(NULL));
    if(argc < 2){
        printf("USAGE ./server [port]\n");
        return 0;
    }
    int server_port = atoi(argv[1]);
    config status = {.server = NULL,.transfer_mode = NULL,.trace = 1,.per_packet_time_out = 1,.timemout = 10};
    struct sockaddr_in client_addr;
    int sockfd;
    int error = 0;
    if(init_tftp_server(server_port,&sockfd) == -1){
        printf("[init_udp_socket] : erreur\n");
    }
    while (!error)
    {
        error = handle_client_requests(status,sockfd,&client_addr);
    }
    
}

    