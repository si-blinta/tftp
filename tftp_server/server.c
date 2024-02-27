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
void* handle_client_requests(void* args){
    thread_context* th_cntxt = (thread_context*)args;
    char* packet = th_cntxt->packet;
    int sockfd = th_cntxt->sockfd;
    struct sockaddr_in client_addr = th_cntxt->client_addr;
    config status = th_cntxt->status;
    int thread_id = th_cntxt->id;
    uint16_t opcode = get_opcode(packet);
    char* filename ;
    char* mode;
    switch (opcode)
    {
    case RRQ:
        filename = get_file_name(packet);
        mode = get_mode(packet);
        process_rrq(status,filename,mode,&client_addr,sockfd,thread_id,&th_cntxt->working);
        free(filename);
        free(mode);
        break;
    case WRQ :
        filename = get_file_name(packet);
        mode = get_mode(packet);
        process_wrq(status,filename,mode,&client_addr,sockfd,thread_id,&th_cntxt->working);
        free(filename);
        free(mode);
    default:
        break;
    }
    return NULL;   
}
static int process_rrq(config status,char* filename,char* mode, const struct sockaddr_in* client_addr, int sockfd, int thread_id, int* thread_working){
   /**
    *   Make recvfrom non blocking using a time out which the value is the amount of time to wait
    *   before resending the data packet, assuming that it got lost.
    */
    *thread_working = READING;
    set_socket_timer(sockfd,status.per_packet_time_out,0);
    u_int8_t timeout = 0;                    //the time out in which we wait for the same ack before quiting the program.
    char ack_packet[MAX_BLOCK_SIZE];   // ack packet
    memset(ack_packet, 0, sizeof(ack_packet));
    FILE* requested_file = NULL;
    char path[100] = SERVER_DIRECTORY;
    strcat(path,filename);
    //TODO : gerer le cas de netascii
    if(!strcasecmp(mode,"octet")){
        requested_file = fopen(path, "rb");
    }
    if (requested_file == NULL) {
        send_error_packet(status,FILE_NOT_FOUND,"File does not exist",client_addr,sockfd,thread_id);
        *thread_working = 0;
        return -1;
    }
    char data[MAX_BLOCK_SIZE-4];             // The buffer to store file bytes with fread.
    memset(data, 0, sizeof(data));
    size_t bytes_read = 0;      //bytes read from fread
    size_t bytes_received = 0;  //bytes received from recvfrom
    uint16_t block_number = 1;       //current data block#

    while ((bytes_read = fread(data, 1, MAX_BLOCK_SIZE-4, requested_file)) > 0) {
        printf("[packet loss] sending data#%d\n",block_number);
        if(!packet_loss(status.packet_loss_percentage)){
            if(send_data_packet(status,block_number,data,client_addr,bytes_read,sockfd,thread_id)){
                fclose(requested_file);
                *thread_working = NOTHING;
                return -1;
            }
        }
        bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, NULL, 0);
        while (bytes_received == -1) { 
            //Error recvfrom
            if(errno != EAGAIN && errno != EWOULDBLOCK){
                perror("[recvfrom]");
                fclose(requested_file); 
                *thread_working = NOTHING;
                return -1;
            }
            // Per packet time out reached : Didnt receive ack packet
            // Resend data
            printf("[packet loss] sending data#%d\n",block_number);
            if(!packet_loss(status.packet_loss_percentage)){
                if(send_data_packet(status,block_number,data,client_addr,bytes_read,sockfd,thread_id)){
                    fclose(requested_file);
                    *thread_working = NOTHING;
                    return -1;
                }
            }
            // Wait for ack again
            bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, NULL, 0);

            //Increment the time out for the actual ack packet.
            timeout+=status.per_packet_time_out;
            //Check if we exceeded the time out for the same packet.
            if(timeout >= status.timemout){
                printf("RRQ FAILED : time out reached : %d seconds\n",timeout);
                fclose(requested_file);
                *thread_working = NOTHING;
                return -1;
            }
        }
        if(check_packet(ack_packet,ACK,status,client_addr,sockfd,thread_id) == -1){
            fclose(requested_file);
            *thread_working = NOTHING;
            return -1;
        }
        // An ACK packet is received :
        timeout = 0;                //Reset time out , because a new data block is about to be sent
        block_number++ ;           //Increment block number
        if(status.trace){
            trace_received(ack_packet,bytes_received,thread_id);
        }
    }
    fclose(requested_file); 
    printf("RRQ SUCCESS\n");
    *thread_working = NOTHING;
    return 0;
    
}
static int process_wrq(config status,char* filename, char* mode, const struct sockaddr_in* client_addr, int sockfd, int thread_id,int* thread_working) {
    /**
    *   Make recvfrom non blocking using a time out which the value is the amount of time to wait
    *   when receiving the same packet before quiting the program.
    */
    *thread_working = READING;
    set_socket_timer(sockfd,status.timemout,0);
    char path[100] = SERVER_DIRECTORY;
    strcat(path,filename); 
    FILE* received_file = NULL;
   
    //TODO : gerer le cas de netascii
    if(!strcasecmp(mode,"octet")){
        received_file = fopen(path, "wb"); 
    }
    //If opening file fails , we send a not defined error, maybe later we will implement permissions ! 
    if (received_file == NULL) {
        send_error_packet(status,NOT_DEFINED, "Unexpected error while opening file", client_addr, sockfd,thread_id);
        *thread_working = NOTHING;
        return -1;
    }
    uint16_t last_block_number_received = 0; // to keep track of the last block# in case of packet loss.
    char data_packet[MAX_BLOCK_SIZE];              // where to store the data packet.
    memset(data_packet,0,sizeof(data_packet));
    size_t bytes_received;              // size of bytes received from the client. 
    
    // Send the ACK0 to the client
    if(send_ack_packet(status,(struct sockaddr*)client_addr,0,sockfd,thread_id) == -1){
        *thread_working = NOTHING;
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
                *thread_working = NOTHING;
                return -1;
            }
            //Time out occured
            printf("WRQ FAILED : time out reached \n");
            fclose(received_file);
            *thread_working = NOTHING;
            return -1;            
        }
        //Tracing
        if(status.trace){
            trace_received(data_packet,bytes_received,thread_id);
        }
        if(check_packet(data_packet,DATA,status,client_addr,sockfd,thread_id) == -1){
            fclose(received_file);
            *thread_working = NOTHING;
            return -1;
        }
        printf("[packet loss] sending ack#%d\n",get_block_number(data_packet));
        //if there is no packet loss we send the ack.
        if(!packet_loss(status.packet_loss_percentage)){
            if(send_ack_packet(status,(struct sockaddr*)client_addr,get_block_number(data_packet),sockfd,thread_id) == -1){
                *thread_working = NOTHING;
                return -1;
            }
        }
        //write the data if the data packet received is different from the last one
        if(last_block_number_received != get_block_number(data_packet)){
            fwrite(get_data(data_packet), 1, bytes_received - 4, received_file);
            last_block_number_received = get_block_number(data_packet);// update last block #
        }
        //break if its the last data packet
        if(bytes_received < MAX_BLOCK_SIZE){
            break;
        }
    }
    *thread_working = NOTHING;
    fclose(received_file);
    printf("WRQ SUCCESS\n");
    return 0;
}

int main(int argc, char**argv)
{   
    srand(time(NULL));
    if(argc < 3){
        printf("USAGE ./server [port] [packet loss percentage (between 0 and 100)]\n");
        return 0;
    }
    int server_port = atoi(argv[1]);
    config status = {.server_ip = NULL,.transfer_mode = NULL,.trace = 1,.per_packet_time_out = 1,.timemout = 10,.packet_loss_percentage = (uint8_t)atoi(argv[2])};
    int sockfd;
    if(init_tftp_server(server_port,&sockfd) == -1){
        printf("[init_udp_socket] : erreur\n");
    }
    //Make the recfrom blocking to limit cpu usage when waiting
    set_socket_timer(sockfd,0,0);
    char packet[MAX_BLOCK_SIZE];
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    thread_context pool[POOL_SIZE];
    //Initializing the thread pool
    for(int i = 0; i< POOL_SIZE; i++){
        pool[i].id = i;
        pool[i].working = 0;
        }
    int current; // current available thread 
    while(1){
    if(recvfrom(sockfd,(char* )packet,MAX_BLOCK_SIZE,0,(struct sockaddr *) &client_addr,&len) < 0){
        if(errno != EAGAIN && errno != EWOULDBLOCK){
            perror("[recvfrom][handle_client_requests]");
            return -1;
        }
        return 0;    
    }
    //checks if a thread is available , if not we send an error to the client
    for(current = 0; current < POOL_SIZE ; current++){
        if(pool[current].working == NOTHING){
            pool[current].client_addr = client_addr;
            pool[current].packet = packet;
            pool[current].sockfd = socket(AF_INET,SOCK_DGRAM,0);
            pool[current].status = status;
            pthread_create(&pool[current].thread,NULL,handle_client_requests,&pool[current]);
            break;
        }
    }
    if(pool[current].working != NOTHING){
        send_error_packet(status,NOT_DEFINED,"No available threads , retry later",&client_addr,sockfd,-1);
    }
    }
    
}

    