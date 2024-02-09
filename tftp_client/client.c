#include "client.h"
#define DEBUG 1
#include<unistd.h>
static char transfer_mode[9] = "netascii";
static int sockfd;
static struct sockaddr_in addr;
static void exit_tftp_client(){
    close(sockfd);
    exit(0);
}

static void process_command(const char* command) {
    if (strcmp(command, "put") == 0) {
        handle_put_command();
    } else if (strcmp(command, "get") == 0) {
        handle_get_command();
    } else if (strcmp(command, "?") == 0) {
        print_help();
    } else if (strcmp(command, "quit") == 0) {
        printf("Exiting TFTP client.\n");
        exit_tftp_client(sockfd);
    } else {
        printf("Error: Unrecognized command. Type '?' for help.\n");
    }
}


static void handle_put_command() {
    char filename[100];
    printf("Enter filename to send: ");
    scanf("%s", filename);
    send_file(filename);
}

static void handle_get_command() {
    char filename[100];
    printf("Enter filename to receive: ");
    scanf("%s", filename);
    receive_file(filename);
}

static void print_help() {
    printf(
        "Commands available:\n"
        "  put     \tSend file to the server\n"
        "  get     \tReceive file from the server\n"
        "  quit    \tExit TFTP client\n"
        "  ?       \tPrint this help information\n"
    );
}
static int connect_to_tftp_server(const char* server_ip, int server_port, int client_port) {
    struct sockaddr_in clientAddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[socket]");
        return -1;
    }
    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(client_port);
    clientAddr.sin_addr.s_addr = inet_addr(server_ip);

    // Bind the socket with the adress  : not necessary ! Useful for debugging when using WireShark
    if (bind(sockfd, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0) {
        perror("[bind]");
        close(sockfd);  
        return -1;
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    addr.sin_addr.s_addr = inet_addr(server_ip); 

    return 0;
}
static void set_file_transfer_mode(const char* mode){
        strcpy(transfer_mode,mode);
}
static int send_file(const char* filename) {
    FILE* requested_file = fopen(filename, "rb");
    if (requested_file == NULL) {
        perror("Failed to open file");
        return -1 ; 
    }

    if(request(WRQ, filename, transfer_mode, sockfd, (struct sockaddr*)&addr) == -1){
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

    if (get_opcode(ack_packet) == ERROR) {    
            print_error_message(ack_packet);
            fclose(requested_file);
            return 0 ;
        }
#ifdef DEBUG
printf("______________________________________________________________________\n");
#endif //DEBUG   
    while ((bytes_read = fread(data_packet, 1, 512, requested_file)) > 0) {
        block_number++;
        if(send_data_packet(block_number,data_packet,&addr,bytes_read,sockfd) == -1){
            return -1;
        };
        bytes_received = recvfrom(sockfd, ack_packet, sizeof(ack_packet), 0, (struct sockaddr*)&addr, &len);
        if (bytes_received == -1) {
            perror("[recvfrom]");
            fclose(requested_file);
            return -1 ;
        }
#ifdef DEBUG     
        if (get_opcode(ack_packet) == ACK && get_block_number(ack_packet) == block_number) {
            printf("ACK %d\n", block_number);
#endif //DEBUG       
        } 
        if (get_opcode(ack_packet) == ERROR) {    
            print_error_message(ack_packet);
            fclose(requested_file);
            return 0; 
        }
    }

#ifdef DEBUG
        printf("______________________________________________________________________\n");
        printf("[put] : %s is sent \n",filename);
        printf("______________________________________________________________________\n");
#endif //DEBUG
    fclose(requested_file);
}
static int receive_file(const char* filename) {
    if(request(RRQ, filename, transfer_mode, sockfd, (struct sockaddr*)&addr) == -1){
        return -1;
    }
    char packet[MAX_BLOCK_SIZE];
    size_t packet_size;
    FILE* requested_file = fopen(filename, "wb");
    if (!requested_file) {
        perror("Failed to open file");
        fclose(requested_file);
        exit(1);
    }
    socklen_t len = sizeof(addr);
    int block_number = 1;

    while (1) {
        size_t bytes_received = recvfrom(sockfd, packet, sizeof(packet), 0, (struct sockaddr*)&addr, &len);
        if (bytes_received == -1) {
            perror("[recvfrom]");
            break;
        }
        if(get_opcode(packet) == ERROR){
            printf("[get] : error code = %d : error message : %s\n",get_error_code(packet),get_error_message(packet));
            if(remove(filename)){
                perror("[remove]");
            }
            break;
        }
        
        fwrite(get_data(packet), 1, bytes_received - 4, requested_file); 
        if(send_ack_packet((struct sockaddr*)&addr,block_number,sockfd) == -1){
            return -1;
        }
       
        block_number++;
        // Check if this is the last data block
        if (bytes_received < MAX_BLOCK_SIZE) {
#ifdef DEBUG
        printf("______________________________________________________________________\n");
        printf("[get] : received %s \n",filename);
        printf("______________________________________________________________________\n");
#endif //DEBUG
            break;
        }
    }
    fclose(requested_file);
    
}



















int main(int argc, char const* argv[]) {
    if (connect_to_tftp_server(IP,SERVER_PORT,CLIENT_PORT) == -1) {
        printf("[connect_to_tftp_server] : erreur");
    }
    char command[100];
    while(1){
        printf("<tftp> ");
        scanf("%s",command);
        process_command(command);

    }
   	

}
