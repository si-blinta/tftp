#include "server.h"

//Global shared variables 
file_control files_open[POOL_SIZE];
pthread_mutex_t files_control_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_file_control(file_control fc[POOL_SIZE]){
    for(int i = 0 ; i< POOL_SIZE; i++){
        fc[i].filename = NULL;
        fc[i].status   = INIT;
    }
}
void file_control_modify(file_control fc[POOL_SIZE], int status, int thread_id, char* filename){
    fc[thread_id].filename = filename;
    fc[thread_id].status = status;
}
int file_control_available(file_control fc[POOL_SIZE],char* filename, int status ){
    for( int i = 0; i < POOL_SIZE; i++){
        if(fc[i].filename == NULL)continue;
        if(strcmp(filename,fc[i].filename) == 0){
            if((status == WRITE) || (status == READ && fc[i].status == WRITE))
                return i;
            else 
                return -1;          
        }
    }
    return -1;
}

void file_control_exit(int thread_id, int id_opened,int status){
    pthread_mutex_lock(&files_control_mutex);
    file_control_modify(files_open,status,thread_id,NULL);
    pthread_mutex_unlock(&files_control_mutex);
    if(id_opened == -1){
        pthread_mutex_unlock(&files_open[thread_id].mutex);
    }
    else {
        pthread_mutex_unlock(&files_open[id_opened].mutex);
    }
}

int file_control_synchronize(char* filename,int thread_id,int status){
    pthread_mutex_lock(&files_control_mutex);
    int id_opened = file_control_available(files_open,filename,status);
    if(id_opened == -1){
        file_control_modify(files_open,status,thread_id,filename);
        pthread_mutex_unlock(&files_control_mutex);
        pthread_mutex_lock(&files_open[thread_id].mutex);
    }
    else {
        pthread_mutex_unlock(&files_control_mutex);
        pthread_mutex_lock(&files_open[id_opened].mutex);
        pthread_mutex_lock(&files_control_mutex);
        file_control_modify(files_open,status,thread_id,filename);
        pthread_mutex_unlock(&files_control_mutex);
    }
    return id_opened;
}
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
        *thread_working = NOTHING;
        return -1;
    }
    int id_opened = file_control_synchronize(filename,thread_id,READ);
    char data[MAX_BLOCK_SIZE-4];             // The buffer to store file bytes with fread.
    memset(data, 0, sizeof(data));
    size_t bytes_read = 0;      //bytes read from fread
    size_t bytes_received = 0;  //bytes received from recvfrom
    uint16_t block_number = 1;       //current data block#

    while ((bytes_read = fread(data, 1, MAX_BLOCK_SIZE-4, requested_file)) > 0) {
        printf("files opened %s : %d | %s : %d \n",files_open[0].filename,files_open[0].status,files_open[1].filename,files_open[1].status);
        printf("[packet loss] sending data#%d\n",block_number);
        if(!packet_loss(status.packet_loss_percentage)){
            if(send_data_packet(status,block_number,data,client_addr,bytes_read,sockfd,thread_id)){ 
                *thread_working = NOTHING;
                fclose(requested_file);
                file_control_exit(thread_id,id_opened,INIT);
                return -1; 
                }
        }
        bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, NULL, 0);
        while (bytes_received == -1) { 
            //Error recvfrom
            if(errno != EAGAIN && errno != EWOULDBLOCK){
                *thread_working = NOTHING;
                fclose(requested_file);
                file_control_exit(thread_id,id_opened,INIT);
                return -1; 
            }
            // Per packet time out reached : Didnt receive ack packet
            // Resend data
            printf("[packet loss] sending data#%d\n",block_number);
            if(!packet_loss(status.packet_loss_percentage)){        
                if(send_data_packet(status,block_number,data,client_addr,bytes_read,sockfd,thread_id)){
                    *thread_working = NOTHING;
                    fclose(requested_file);
                    file_control_exit(thread_id,id_opened,INIT);
                    return -1; 
                }
            }
            // Wait for ack again
            bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, NULL, 0);

            //Increment the time out for the actual ack packet.
            timeout+=status.per_packet_time_out;
            //Check if we exceeded the time out for the same packet.
            if(timeout >= status.timemout){     //ERROR
                printf("RRQ FAILED : time out reached : %d seconds\n",timeout);
                *thread_working = NOTHING;
                fclose(requested_file);
                file_control_exit(thread_id,id_opened,INIT);
                return -1; 
        }
        }
        if(check_packet(ack_packet,ACK,status,client_addr,sockfd,thread_id) == -1){     //ERROR
            *thread_working = NOTHING;
            fclose(requested_file);
            file_control_exit(thread_id,id_opened,INIT);
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
    file_control_exit(thread_id,id_opened,INIT);
    return 0;
    
}
static int process_wrq(config status,char* filename, char* mode, const struct sockaddr_in* client_addr, int sockfd, int thread_id,int* thread_working) {
    int id_opened = file_control_synchronize(filename,thread_id,WRITE);
    *thread_working = WRITING;
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
        file_control_exit(thread_id,id_opened,INIT);
        return -1;
    }
    uint16_t last_block_number_received = 0; // to keep track of the last block# in case of packet loss.
    char data_packet[MAX_BLOCK_SIZE];              // where to store the data packet.
    memset(data_packet,0,sizeof(data_packet));
    size_t bytes_received;              // size of bytes received from the client. 
    
    // Send the ACK0 to the client
    if(send_ack_packet(status,(struct sockaddr*)client_addr,0,sockfd,thread_id) == -1){
        *thread_working = NOTHING;
        fclose(received_file);
        file_control_exit(thread_id,id_opened,INIT);
        return -1; 
    }
    //WRQ loop
    while (1) { 
        printf("files opened %s : %d | %s : %d \n",files_open[0].filename,files_open[0].status,files_open[1].filename,files_open[1].status);
        //Receive the data packet , no need to store client address
        bytes_received = recvfrom(sockfd, data_packet, sizeof(data_packet), 0, NULL, 0);
        if(bytes_received == -1){
            //Actual error when using recvfrom
            if(errno != EAGAIN && errno != EWOULDBLOCK){   
                perror("[recvfrom]");
                *thread_working = NOTHING;
                fclose(received_file);
                file_control_exit(thread_id,id_opened,INIT);
                return -1; 
            }
            //Time out occured
            printf("WRQ FAILED : time out reached \n");
            *thread_working = NOTHING;
            fclose(received_file);
            file_control_exit(thread_id,id_opened,INIT);
            return -1;            
        }
        //Tracing
        if(status.trace){
            trace_received(data_packet,bytes_received,thread_id);
        }
        if(check_packet(data_packet,DATA,status,client_addr,sockfd,thread_id) == -1){   
            *thread_working = NOTHING;
            fclose(received_file);
            file_control_exit(thread_id,id_opened,INIT);
            return -1; 
        }
        printf("[packet loss] sending ack#%d\n",get_block_number(data_packet));
        //if there is no packet loss we send the ack.
        if(!packet_loss(status.packet_loss_percentage)){    
            if(send_ack_packet(status,(struct sockaddr*)client_addr,get_block_number(data_packet),sockfd,thread_id) == -1){
                *thread_working = NOTHING;
                fclose(received_file);
                file_control_exit(thread_id,id_opened,INIT);
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
    file_control_exit(thread_id,id_opened,INIT);
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
    init_file_control(files_open);
    
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
    else {
        printf("available thread %d\n",current);
    }
    }
    
}

    