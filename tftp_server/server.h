#ifndef SERVER_H
#define SERVER_H
#include "utils.h"
#include <sys/select.h>
#define SERVER_DIRECTORY "../server_directory/"
#define MAX_CLIENT 2

typedef struct {
    FILE* file_fd;
    int socket;
    struct sockaddr_in* client_addr;
    uint16_t block_number;
    char* filename;
    size_t number_bytes_operated;   // Only used for debug now since we don't close the file
    int operation;
    char last_block[MAX_BLOCK_SIZE];
    size_t last_block_size;
    time_t last_time_stamp;
    double timeout ;
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
* @brief Handles RRQ, it can be called in two cases :
 *                      -First call, needs filename to open the file and send the first data packet, also handles error of opening a file.
 *                      -Follow up call, sends the right data packet, checks for terminaison.( we don't open the file )
 * 
 * @param status Config of the server .
 * @param filename The filename requested.
 * @param main_socket_fd The server socket , we need it for first call to send an error if we can't open a file.
 * @param client_h   The adress to client handler structure.
 * @param client_handler_id The index of the client handler in , client_h array.
 */
static int handle_client_requests(config status,int main_socket_fd);

//-------------------------------------------------------------------------------------
/**
 * @brief Handles WRQ, it can be called in two cases :
 *                      -First call, needs filename to open the file and send the first ack packet, also handles error of opening a file.
 *                      -Follow up call, sends the ack packet, writes data , checks for terminaison.( we don't open the file )
 *                          ,updates client handler ( updates time_stamp , last block ....)
 * 
 * @param status Config of the server .
 * @param filename The filename requested.
 * @param main_socket_fd The server socket , we need it for first call to send an error if we can't open a file.
 * @param client_h   The adress to client handler structure.
 * @param bytes_received The bytes received from the recvfrom call.
 * @param buffer         The buffer used in recvfrom. ( buffer storing data packet )
 * @param client_handler_id The index of the client handler in , client_h array.
 * @return 0 on success , -1 on error.
 */
int handle_wrq(config status, char* filename,int main_socket_fd,client_handler* client_h, size_t bytes_received, char buffer[MAX_BLOCK_SIZE],int client_handler_id);

//-------------------------------------------------------------------------------------
/**
 * @brief Handles RRQ, it can be called in two cases :
 *                      -First call, needs filename to open the file and send the first data packet, also handles error of opening a file.
 *                      -Follow up call, sends the right data packet, checks for terminaison.( we don't open the file )
 *                          ,updates client handler ( updates time_stamp , last block ....)
 * 
 * @param status Config of the server .
 * @param filename The name of the file requested.
 * @param main_socket_fd The server socket , we need it for first call to send an error if we can't open a file.
 * @param client_h   The adress to client handler structure.
 * @param client_handler_id The index of the client handler in , client_h array.
 * @return 0 on success , -1 on error.
 */
int handle_rrq(config status, char* filename,int main_socket_fd,client_handler* client_h, int client_handler_id);

//-------------------------------------------------------------------------------------
/**
 * @brief Initialize client_handler structure.
 * @param client_h The pointer to the structure;
 */
void client_handler_init(client_handler* client_h);
//-------------------------------------------------------------------------------------
/**
 * @brief Checks for available ressources in client_handler array.
 * @param client_h Client handler array to check in .
 * @return index available, -1 if no ressource is available.
*/
int client_handler_available(client_handler client_h[MAX_CLIENT]);
//----------------------------------------------------------------------------------------
/**
 * @brief Checks if a file is available according to WRITERS / READERS problem.
 * @param client_h Client handler array to check in.
 * @param filename The name of the file that we want to manipulate.
 * @param operation The type of operation we want to do on the file.
 * @return -1 if we can access it , other if we cant.
*/
//------------------------------------------------------------------------------------------
int client_handler_file_available(client_handler client_h[MAX_CLIENT],char* filename, int operation);
/**
 * @brief Prints client handlers status.
 * @param client_h Client handler array.
*/
void client_handler_print(client_handler client_h[MAX_CLIENT]) ;
//--------------------------------------------------------------------------------
/**
 * @brief checks if there is at least one active client handler
 * @param client_h An array of client handlers
 * @return 0 if there is no active handlers , 1 if at least one is present
*/
int client_handler_one_is_active(client_handler client_h[MAX_CLIENT]);
#endif // SERVER_H
