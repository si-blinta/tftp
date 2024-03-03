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
void client_handler_init(client_handler* client_h){ //for initialization and destroy (i use malloc and free outside to remember)
    client_h->client_addr = NULL;
    client_h->file_fd = NULL;
    client_h->block_number = -1;
    client_h->filename     = NULL;
    client_h->number_bytes_operated = 0;
    client_h->operation = NONE;
    client_h->socket    = -1;
    client_h->last_time_stamp = -1;
    client_h->timeout         = 0;
    memset(client_h->last_block,0,MAX_BLOCK_SIZE);
}

int client_handler_available(client_handler client_h[MAX_CLIENT]){
   for(int i = 0 ; i < MAX_CLIENT; i++){
        if(client_h[i].operation == NONE)return i;
   }
   return -1; 
}
int client_handler_file_available(client_handler client_h[MAX_CLIENT],char* filename, int operation){
    for( int i = 0; i < MAX_CLIENT; i++){
        if(client_h[i].filename == NULL)continue;
        if(strcmp(filename,client_h[i].filename) == 0){
            if((operation == WRITE) || (operation == READ && client_h[i].operation == WRITE))
                return i;
            else 
                return -1;          
        }
    }
    return -1;
}
void client_handler_print(client_handler client_h[MAX_CLIENT]) {
    for (int i = 0; i < MAX_CLIENT; i++) {
        const char* status_string;
        switch (client_h[i].operation) {
            case READ:
                status_string = "READ";
                break;
            case WRITE:
                status_string = "WRITE";
                break;
            case NONE:
                status_string = "NONE";
                break;
            default:
                break;
        }
        printf("#%d|%s = %s | ", i,client_h[i].filename,status_string);
    }
    printf("\n");
}

int handle_rrq(config status, char* filename,int main_socket_fd,client_handler* client_h, int client_handler_id) {
    char buffer[MAX_BLOCK_SIZE];
    size_t bytes_read;
    //check if its first call

    if(client_h->operation == NONE){
        char path[100] = SERVER_DIRECTORY;
        strcat(path,filename);
        //The file needs to exits
        FILE* requested_file = fopen(path,"rb");
        if (requested_file == NULL) {
            send_error_packet(status,FILE_NOT_FOUND,"File does not exist",client_h->client_addr,main_socket_fd,client_handler_id);
            return -1;
        }
        client_h->filename = filename;
        client_h->file_fd = requested_file;
        client_h->operation = READ;
        client_h->block_number = 1;
        bytes_read = fread(buffer,1,MAX_BLOCK_SIZE-4, client_h->file_fd);
        client_h->number_bytes_operated += bytes_read; 
        if(send_data_packet(status,client_h->block_number,buffer,client_h->client_addr,bytes_read,client_h->socket,client_handler_id) == -1 )return -1;
        memcpy(client_h->last_block,buffer,MAX_BLOCK_SIZE); // For retransmissions
        client_h->last_block_size = bytes_read;
        return 0;
        // We don't close the file , IO operations are very slow
    }
    else {
        client_h->timeout = 0;    // reset timer on success (received an ack for last packed sent)
        bytes_read = fread(buffer,1,MAX_BLOCK_SIZE-4, client_h->file_fd);  // no need to use fseek because we dont close file
        client_h->number_bytes_operated += bytes_read;                            // maybe useful for further improvements
        if(bytes_read <= 0){   // If we cant read
            printf("RRQ SUCCES : <%s, %ld bytes>\n",client_h->filename,client_h->number_bytes_operated);
            fclose(client_h->file_fd);
            free(client_h->filename);
            free(client_h->client_addr);
            client_handler_init(client_h);
            return 0;
        }
        if(!packet_loss(status.packet_loss_percentage)){
            if(send_data_packet(status,client_h->block_number+1,buffer,client_h->client_addr,bytes_read,client_h->socket,client_handler_id) == -1 )return -1; 
        }
    
        memcpy(client_h->last_block,buffer,MAX_BLOCK_SIZE); // For retransmissions
        client_h->last_block_size = bytes_read;
        return 0;
    }
    
}

int handle_wrq(config status, char* filename,int main_socket_fd,client_handler* client_h, size_t bytes_received, char buffer[MAX_BLOCK_SIZE],int client_handler_id) {
    if(client_h->operation == NONE){    
        char path[100] = SERVER_DIRECTORY;
        strcat(path,filename);
        FILE* requested_file = fopen(path,"wb");  // create the file
        client_h->filename = filename;
        client_h->file_fd = requested_file;
        client_h->operation = WRITE;
        client_h->block_number = 0; // Respecting TFTP protocol , ack 0 
        if(send_ack_packet(status,(struct sockaddr*) client_h->client_addr,client_h->block_number,client_h->socket,client_handler_id) == -1)return -1;
        return 0;
    }
    else {
        client_h->timeout = 0;  // reset timer because we received a packet
        //Check if its a different block from last one
        if(client_h->block_number != get_block_number(buffer)){
            client_h->block_number = get_block_number(buffer);
            size_t bytes_written = fwrite(get_data(buffer),1,bytes_received-4, client_h->file_fd);  // no need to use fseek because we dont close file
            memcpy(client_h->last_block,buffer,MAX_BLOCK_SIZE); // For retransmissions
            client_h->number_bytes_operated += bytes_written;   // maybe useful for further improvements
        }
        if(!packet_loss(status.packet_loss_percentage)){
            if(send_ack_packet(status,(struct sockaddr*) client_h->client_addr,client_h->block_number,client_h->socket,client_handler_id) == -1 )return -1; // send ack
        }
        if(bytes_received < MAX_BLOCK_SIZE){   // If last block free ressources , Initialize client_handler to make it available for other clients
            printf("WRQ SUCCES : <%s, %ld bytes>\n",client_h->filename,client_h->number_bytes_operated);
            fclose(client_h->file_fd);
            free(client_h->filename);
            free(client_h->client_addr);
            client_handler_init(client_h);
            return 0 ;
        }
        return 0;
    }
    
}


static int handle_client_requests(config status,int main_socket_fd){
    fd_set master,clone;
    struct timeval timer;
    client_handler client_h[MAX_CLIENT];
    for(int i = 0 ; i < MAX_CLIENT; i++)client_handler_init(&client_h[i]);
    FD_ZERO(&master);
    FD_ZERO(&clone);
    FD_SET(main_socket_fd,&master);
    
    char buffer[MAX_BLOCK_SIZE];
    char* filename = NULL;
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int current = 0;        // current client_handler
    size_t bytes_received = 0;
    
    while(1){
        client_handler_print(client_h);
        clone = master;                             // work on a copy because select is destructive
        timer.tv_sec = status.per_packet_time_out;  // reseting the timer because select is destructive
        timer.tv_usec = 0;
        if(select(FD_SETSIZE,&clone,NULL,NULL,&timer) == -1){
            perror("[handle_client_requests][select]");
            exit(EXIT_FAILURE);
        }
        if(FD_ISSET(main_socket_fd,&clone)){        // if its a RRQ / WRQ since its sent to the port 8080 in our case
            bytes_received = recvfrom(main_socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len);
            if(bytes_received == -1){ // handle recvfrom errors
                perror("[handle_client_requests][recvfrom]");
                exit(EXIT_FAILURE);
            }
            current = client_handler_available(client_h);   // check if we have available client_handlers, we made the choice to limit the ressources (pool)
            if( current != -1){
                //we have available ressources (client handler)
                filename = get_file_name(buffer); // TO FREE
                int opcode = get_opcode(buffer);                         // check request type
                switch (opcode)
                {
                case RRQ:
                    //Check if we can access the file (writers / readers problem)
                    if(client_handler_file_available(client_h,filename,READ) == -1){
                        client_h[current].socket = socket(AF_INET,SOCK_DGRAM,0); // create new socket         
                        client_h[current].client_addr = malloc(sizeof(client_addr));// add client_addr TO FREE
                        memcpy(client_h[current].client_addr,&client_addr,len);
                        FD_SET(client_h[current].socket,&master);                // add it to the set
                        handle_rrq(status,filename,main_socket_fd,&client_h[current],current); 
                        time(&client_h[current].last_time_stamp);   // update last time stamp (i did it here for readibility , i could have done it in the function)
                    }
                    else { // if we can not access the file 
                           // with our implementation we can't wait , ( if we had more time we could have figured it out ), so just send an error instead
                        send_error_packet(status,ACCESS_VIOLATION,"Can not access the file, a user is modifying it",&client_addr,main_socket_fd,-1);
                    }
                    break;
                case WRQ:
                    if(client_handler_file_available(client_h,filename,WRITE) == -1){
                        client_h[current].socket = socket(AF_INET,SOCK_DGRAM,0); // create new socket        
                        client_h[current].client_addr = malloc(sizeof(client_addr));    // add client_addr  TO FREE
                        memcpy(client_h[current].client_addr,&client_addr,len);
                        FD_SET(client_h[current].socket,&master);                // add it to the set
                        handle_wrq(status,filename,main_socket_fd,&client_h[current],bytes_received,buffer,current);
                        time(&client_h[current].last_time_stamp);   // update last time stamp (i did it here for readibility , i could have done it in the function)
                    }
                    else {
                        send_error_packet(status,ACCESS_VIOLATION,"Can not access the file, a user is modifying it",&client_addr,main_socket_fd,-1);
                    }   
                    break;
                default:
                    break;
                }
            }
            else {
                //If no ressources available, send error
                send_error_packet(status,NOT_DEFINED,"No ressource available retry later",&client_addr,main_socket_fd,-1);
            }
        }
        for(int i = 0; i < MAX_CLIENT; i++){    // For every client handler socket
            if(client_h[i].socket != -1){       // If its an active client handler    
                if(FD_ISSET(client_h[i].socket,&clone)){    // Check if there is someting to read ( received ack or data )
                    memset(buffer,0,MAX_BLOCK_SIZE);        // Clean the buffer
                    bytes_received = recvfrom(client_h[i].socket, buffer, sizeof(buffer), 0, (struct sockaddr*)client_h[i].client_addr, &len);
                    if(bytes_received == -1){   // handle recvfrom error
                        perror("[handle_client_requests][recvfrom]");
                        exit(EXIT_FAILURE);
                    }         
                    switch (client_h[i].operation)  // depending on operation of the client handler
                    {
                        case READ:
                            client_h[i].block_number = get_block_number(buffer);
                            handle_rrq(status,filename,main_socket_fd,&client_h[i],i);    
                            time(&client_h[i].last_time_stamp); // update last time_stamp
                            break;
                        case WRITE:
                            handle_wrq(status,filename,main_socket_fd,&client_h[i],bytes_received,buffer,i);
                            time(&client_h[i].last_time_stamp); // update last time_stamp
                            break;
                        default:
                            break;
                    }

                }
                else if(client_h[i].operation == READ){ // For every client handler that is not active and doing a read operation
                    double time_passed = difftime(time(NULL),client_h[i].last_time_stamp);  
                    client_h[i].timeout+=time_passed;               // Update total time passed 
                    if(client_h[i].timeout >= status.timemout){     // total time out reached for a single block
                        //free all ressources and quit 
                        printf("RRQ FAILED : <%s, %ld bytes>\n",client_h[i].filename,client_h[i].number_bytes_operated);
                        fclose(client_h[i].file_fd);
                        free(client_h[i].filename);
                        free(client_h[i].client_addr);
                        client_handler_init(&client_h[i]);  
                    }
                    else if(time_passed >= status.per_packet_time_out){  // check if a second has passed between last time_stamp and now , if so resend the last packet
                        send_data_packet(status,client_h[i].block_number+1,client_h[i].last_block,client_h[i].client_addr,client_h[i].last_block_size,client_h[i].socket,i);
                        trace_sent(client_h[i].last_block,client_h[i].last_block_size,i);
                        time(&client_h[i].last_time_stamp); // update the last time stamp
                    }
                }
                else if(client_h[i].operation == WRITE){
                    double time_passed = difftime(time(NULL),client_h[i].last_time_stamp);  
                    client_h[i].timeout+=time_passed;               // Update total time passed
                    time(&client_h[i].last_time_stamp); // update last time_stamp
                    if(client_h[i].timeout >= status.timemout){     // total time out reached for a single block
                        //free all ressources and quit 
                        printf("WRQ FAILED : <%s, %ld bytes>\n",client_h[i].filename,client_h[i].number_bytes_operated);
                        fclose(client_h[i].file_fd);
                        free(client_h[i].filename);
                        free(client_h[i].client_addr);
                        client_handler_init(&client_h[i]);
                    }
                    // We don't resend the ack
                }
            }
        }
    }
    return 0;  
}


int main(int argc, char**argv)
{   
    if(argc < 3){
        printf("USAGE ./server [port] [packet loss percentage (between 0 and 100)]\n");
        return 0;
    }
    int server_port = atoi(argv[1]);
    config status = {.server_ip = NULL,.transfer_mode = NULL,.trace = 1,.per_packet_time_out = 1,.timemout = 10, .packet_loss_percentage = (uint8_t)atoi(argv[2])};
    int sockfd;
    if(init_tftp_server(server_port,&sockfd) == -1){
        printf("[init_udp_socket] : erreur\n");
    }
    handle_client_requests(status,sockfd);
}

    