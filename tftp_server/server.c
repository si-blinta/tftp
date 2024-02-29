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
                    send_data_packet(status,client_h[current].block_number++,buffer,&client_addr,bytes_read,client_h[current].socket);
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
                            size_t bytes_read = fread(buffer,1,MAX_BLOCK_SIZE-4, client_h[i].file_fd);
                            client_h[i].number_bytes_operated += bytes_read; 
                            send_data_packet(status,client_h[i].block_number++,buffer,&client_addr,bytes_read,client_h[i].socket);
                            if(bytes_read < 512){
                                fclose(client_h[i].file_fd);
                                free(client_h[i].filename);
                                client_handler_init(&client_h[i]);
                            }
                            usleep(100000);
                            break;
                        
                        default:
                            break;
                    }

                }
            }
        }
    }
   
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

    