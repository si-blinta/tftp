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
    client_h->block_number = -1;
    client_h->filename     = NULL;
    client_h->number_bytes_operated = 0;
    client_h->operation = NONE;
    client_h->socket    = -1;
    memset(client_h->last_block,0,MAX_BLOCK_SIZE);
}
int client_handler_available(client_handler client_h[MAX_CLIENT]){
   for(int i = 0 ; i < MAX_CLIENT; i++){
        if(client_h[i].operation == NONE)return i;
   }
   return -1; 
}
int handle_rrq(config status, char* filename,int main_socket_fd,client_handler* client_h, struct sockaddr_in client_addr) {
    char buffer[MAX_BLOCK_SIZE];
    size_t bytes_read;
    //check if its first call

    if(client_h->operation == NONE){
        char path[100] = SERVER_DIRECTORY;
        strcat(path,filename);
        //The file needs to exits
        FILE* requested_file = fopen(path,"rb");
        if (requested_file == NULL) {
            send_error_packet(status,FILE_NOT_FOUND,"File does not exist",&client_addr,main_socket_fd);
            return -1;
        }
        client_h->filename = filename;
        client_h->file_fd = requested_file;
        client_h->operation = READ;
        client_h->block_number = 1;
        bytes_read = fread(buffer,1,MAX_BLOCK_SIZE-4, client_h->file_fd);
        client_h->number_bytes_operated += bytes_read; 
        if(send_data_packet(status,client_h->block_number++,buffer,&client_addr,bytes_read,client_h->socket) == -1 )return -1;
        memcpy(client_h->last_block,buffer,MAX_BLOCK_SIZE); // For retransmissions
        return 0;
        // We don't close the file , IO operations are very slow
    }
    else {
        bytes_read = fread(buffer,1,MAX_BLOCK_SIZE-4, client_h->file_fd);  // no need to use fseek because we dont close file
        client_h->number_bytes_operated += bytes_read;                            // maybe useful for further improvements
        if(bytes_read <= 0){   // If we cant read
            printf("RRQ SUCCES : <%s, %ld bytes>\n",client_h->filename,client_h->number_bytes_operated);
            fclose(client_h->file_fd);
            free(client_h->filename);
            client_handler_init(client_h);
            return 0;
        }
        if(send_data_packet(status,client_h->block_number++,buffer,&client_addr,bytes_read,client_h->socket) == -1 )return -1;  
        memcpy(client_h->last_block,buffer,MAX_BLOCK_SIZE); // For retransmissions
        usleep(10000);        // for debug purposes
        return 0;
    }
    
}

int handle_wrq(config status, char* filename,int main_socket_fd,client_handler* client_h, struct sockaddr_in client_addr, size_t bytes_received, char buffer[MAX_BLOCK_SIZE]) {
    if(client_h->operation == NONE){
        char path[100] = SERVER_DIRECTORY;
        strcat(path,filename);
        FILE* requested_file = fopen(path,"wb");  // create the file
        client_h->filename = filename;
        client_h->file_fd = requested_file;
        client_h->operation = WRITE;
        client_h->block_number = 0; // Respecting TFTP protocol , ack 0 
        if(send_ack_packet(status,(struct sockaddr*) &client_addr,client_h->block_number++,client_h->socket) == -1)return -1;
        return 0;
    }
    else {
        size_t bytes_written = fwrite(get_data(buffer),1,bytes_received-4, client_h->file_fd);  // no need to use fseek because we dont close file
        memcpy(client_h->last_block,buffer,MAX_BLOCK_SIZE); // For retransmissions
        client_h->number_bytes_operated += bytes_written;                            // maybe useful for further improvements
        if(send_ack_packet(status,(struct sockaddr*) &client_addr,client_h->block_number++,client_h->socket) == -1 )return -1;
        if(bytes_received < MAX_BLOCK_SIZE){   // If last block free ressources , Initialize client_handler to make it available 
            printf("WRQ SUCCES : <%s, %ld bytes>\n",client_h->filename,client_h->number_bytes_operated);
            fclose(client_h->file_fd);
            free(client_h->filename);
            client_handler_init(client_h);
            return 0 ;
        }
        usleep(10000);        // for debug purposes   
        return 0;
    }
    
}


static int handle_client_requests(config status,int main_socket_fd){    //TODO error checking
    fd_set master,clone;
    client_handler client_h[MAX_CLIENT];
    for(int i = 0 ; i < MAX_CLIENT; i++)client_handler_init(&client_h[i]);
    FD_ZERO(&master);
    FD_ZERO(&clone);
    FD_SET(main_socket_fd,&master);
    
    char buffer[MAX_BLOCK_SIZE];
    char* filename = NULL;
    char path[100] ;
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int current = 0;        // current client_handler
    size_t bytes_received = 0;
    
    while(1){
        clone = master;             // work on a copy because select is destructive
        select(FD_SETSIZE,&clone,NULL,NULL,NULL);
        if(FD_ISSET(main_socket_fd,&clone)){        // if its a RRQ / WRQ since its sent to the port 8080 in our case
            current = client_handler_available(client_h);
            bytes_received = recvfrom(main_socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len);
            if( current != -1){
                filename = get_file_name(buffer); // TO FREE
                strcpy(path,SERVER_DIRECTORY);
                strcat(path,filename);
                //If we have available ressources
                client_h[current].socket = socket(AF_INET,SOCK_DGRAM,0); // create new socket
                FD_SET(client_h[current].socket,&master);                // add it to the set

                int opcode = get_opcode(buffer);
                switch (opcode)
                {
                case RRQ:
                    handle_rrq(status,filename,main_socket_fd,&client_h[current],client_addr);
                    break;
                case WRQ:
                    handle_wrq(status,filename,main_socket_fd,&client_h[current],client_addr,bytes_received,buffer);
                    break;
                default:
                    break;
                }
            }
            else {
                //If no ressources available, send error
                send_error_packet(status,NOT_DEFINED,"No ressource available retry later",&client_addr,main_socket_fd);
            }
        }
        for(int i = 0; i < MAX_CLIENT; i++){    // For every client handler socket
            if(client_h[i].socket != -1){       // If its an active client handler
                if(FD_ISSET(client_h[i].socket,&clone)){    // Check if there is someting to read
                    memset(buffer,0,MAX_BLOCK_SIZE);
                    bytes_received = recvfrom(client_h[i].socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len);
                               
                    switch (client_h[i].operation)  // depending on operation of the client handler
                    {
                        case READ:
                            handle_rrq(status,filename,main_socket_fd,&client_h[i],client_addr);
                            break;
                        case WRITE:
                            handle_wrq(status,filename,main_socket_fd,&client_h[i],client_addr,bytes_received,buffer);
                            break;
                        default:
                            break;
                    }

                }
            }
        }
    }
    return 0;  
}


int main(int argc, char**argv)
{   
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

    