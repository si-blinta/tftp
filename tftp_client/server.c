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
void client_handler_init(client_handler* client_h){
    client_h->file_fd = NULL;
    client_h->block_number = 0;
    client_h->filename     = NULL;
    client_h->number_bytes_operated = 0;
    client_h->operation = NONE;
    client_h->socket    = -1;
}
int client_handler_available(client_handler client_h[MAX_CLIENT]){
   for(int i = 0 ; i < MAX_CLIENT; i++){
        if(client_h[i].operation == NONE)return i;
   }
   return -1; 
}



static int handle_client_requests(config status,int main_socket_fd){    //TODO error checking
    fd_set master,clone;
    client_handler client_h[MAX_CLIENT];
    for(int i = 0 ; i < MAX_CLIENT; i++)client_handler_init(&client_h[i]);
    FD_ZERO(&master);
    FD_ZERO(&clone);
    FD_SET(main_socket_fd,&master);
    char buffer[MAX_BLOCK_SIZE];
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int current = 0;
    while(1){
        clone = master;
        select(FD_SETSIZE,&clone,NULL,NULL,NULL);
        if(FD_ISSET(main_socket_fd,&clone)){        // if its a RRQ / WRQ since its sent to the port 8080 in our case
            recvfrom(main_socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len);
            char* filename = get_file_name(buffer);
            char path[100] = SERVER_DIRECTORY;
            strcat(path,filename);
            FILE* requested_file = fopen(path,"rb");
            if (requested_file == NULL) {
                send_error_packet(status,FILE_NOT_FOUND,"File does not exist",&client_addr,main_socket_fd);
                free(filename);
                return -1;
            }
            current = client_handler_available(client_h);
            if( current != -1){
                //If we have available ressources
                client_h[current].socket = socket(AF_INET,SOCK_DGRAM,0);
                FD_SET(client_h[current].socket,&master);
                //answer his request
                int opcode = get_opcode(buffer);
                switch (opcode)
                {
                case RRQ:
                    client_h[current].filename = filename;
                    client_h[current].file_fd = requested_file;
                    client_h[current].operation = READ;
                    client_h[current].block_number = 1;
                    size_t bytes_read = fread(buffer,1,MAX_BLOCK_SIZE-4, client_h[current].file_fd);
                    client_h[current].number_bytes_operated += bytes_read; 
                    send_data_packet(status,client_h[current].block_number,buffer,&client_addr,bytes_read,client_h[current].socket);
                    break;
                case WRQ:
                    client_h[current].filename = get_file_name(buffer);
                    client_h[current].operation = WRITE;
                    break;
                default:
                    
                    break;
                }
            }
            else {
                printf("Sent error packet\n");
                send_error_packet(status,NOT_DEFINED,"No ressource available retry later",&client_addr,main_socket_fd);
            }
        }
        for(int i = 0; i < MAX_CLIENT; i++){
            if(client_h[i].socket != -1){
                if(FD_ISSET(client_h[i].socket,&clone)){
                recvfrom(client_h[i].socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len);
                switch (client_h[i].operation)
                {
                case READ:
                    fseek(client_h[i].file_fd,client_h[i].number_bytes_operated,SEEK_CUR);
                    size_t bytes_read = fread(buffer,1,MAX_BLOCK_SIZE-4, client_h[current].file_fd);
                    client_h[current].number_bytes_operated += bytes_read; 
                    send_data_packet(status,client_h[current].block_number++,buffer,&client_addr,bytes_read,client_h[current].socket);
                    break;
                
                default:
                    break;
                }

                }
            }
        }
    }
   
}
static int process_rrq(config status,char* filename,char* mode, const struct sockaddr_in* client_addr, int sockfd){
   /**
    *   Make recvfrom non blocking using a time out which the value is the amount of time to wait
    *   before resending the data packet, assuming that it got lost.
    */
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
        send_error_packet(status,FILE_NOT_FOUND,"File does not exist",client_addr,sockfd);
        return -1;
    }
    char data[MAX_BLOCK_SIZE-4];             // The buffer to store file bytes with fread.
    memset(data, 0, sizeof(data));
    size_t bytes_read = 0;      //bytes read from fread
    size_t bytes_received = 0;  //bytes received from recvfrom
    uint16_t block_number = 1;       //current data block#

    while ((bytes_read = fread(data, 1, MAX_BLOCK_SIZE-4, requested_file)) > 0) {
        if(send_data_packet(status,block_number,data,client_addr,bytes_read,sockfd)){
            fclose(requested_file);
            return -1;
        }
        while ((bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, NULL, 0)) == -1) { 
            //Increment the time out for the actual ack packet.
            timeout+=status.per_packet_time_out;
            //Error recvfrom
            if(errno != EAGAIN && errno != EWOULDBLOCK){
                perror("[recvfrom]");
                fclose(requested_file); 
                return -1;
            }
            if(send_data_packet(status,block_number,data,client_addr,bytes_read,sockfd)){
                fclose(requested_file);
                return -1;
            }
            
            //Check if we exceeded the time out for the same packet.
            if(timeout >= status.timemout){
                printf("RRQ FAILED : time out reached : %d seconds\n",timeout);
                fclose(requested_file);
                return -1;
            }
        }
        if(check_packet(ack_packet,ACK,status,client_addr,sockfd) == -1){
            fclose(requested_file);
            return -1;
        }
        // An ACK packet is received :
        timeout = 0;                //Reset time out , because a new data block is about to be sent
        block_number++ ;           //Increment block number
        if(status.trace){
            trace_received(ack_packet,bytes_received);
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
        send_error_packet(status,NOT_DEFINED, "Unexpected error while opening file", client_addr, sockfd);
        return -1;
    }
    uint16_t last_block_number_received = 0; // to keep track of the last block# in case of packet loss.
    char data_packet[MAX_BLOCK_SIZE];              // where to store the data packet.
    memset(data_packet,0,sizeof(data_packet));
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
        if(check_packet(data_packet,DATA,status,client_addr,sockfd) == -1){
            fclose(received_file);
            return -1;
        }
        if(send_ack_packet(status,(struct sockaddr*)client_addr,get_block_number(data_packet),sockfd) == -1){
            return -1;
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
    config status = {.server_ip = NULL,.transfer_mode = NULL,.trace = 1,.per_packet_time_out = 1,.timemout = 10};
    int sockfd;
    if(init_tftp_server(server_port,&sockfd) == -1){
        printf("[init_udp_socket] : erreur\n");
    }
    handle_client_requests(status,sockfd);
}

    