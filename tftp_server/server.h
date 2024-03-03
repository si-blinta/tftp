#ifndef SERVER_H
#define SERVER_H
#include "utils.h"
#define SERVER_DIRECTORY "../server_directory/"
#define MAX_CLIENT 2

typedef struct {
    FILE* file_fd;
    int socket;
    struct sockaddr_in client_addr;
    uint16_t block_number;
    char* filename;
    size_t number_bytes_operated;   // Only used for debug now since we don't close the file
    int operation;
    char last_block[MAX_BLOCK_SIZE];
    size_t last_block_size;
    time_t last_time_stamp;
}client_handler;

enum operation{
    READ,
    WRITE,
    NONE
};

//-------------------------------------------------------------------------------------
/**
 * @brief Initializes a UDP server socket.
 * 
 * This function creates a UDP socket and binds it to a specified port to listen for incoming client requests.
 * @param sockfd Pointer to an integer where the socket file descriptor will be stored.
 * @param port Port number on which the server will listen for incoming connections.
 * @return Returns 0 on success, -1 on failure with an error message printed to stderr.
 */
static int init_tftp_server(int port,int* sockfd);

//-------------------------------------------------------------------------------------
/**
 * @brief Handles incoming client requests.
 * 
 * This function listens for and processes incoming client requests, including read requests (RRQ) and write requests (WRQ).
 * 
 * @param main_socket_fd The socket file descriptor used to listen for incoming requests.
 */
static int handle_client_requests(config status,int main_socket_fd);

//-------------------------------------------------------------------------------------
/**
 * @brief Initialize client_handler structure.
 * @param client_h The pointer to the structure;
 */
void client_handler_init(client_handler* client_h);


#endif // SERVER_H
