#ifndef CLIENT_H
#define CLIENT_H
#include "utils.h"
//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Sends a request packet over UDP.
 * 
 * Constructs and sends a TFTP request packet to the server specified by addr.
 * 
 * @param type The type of request ( RRQ or WRQ )
 * @param filename The name of the file to read.
 * @param status The configuration of the client.
 * @param sockfd The socket file descriptor used to send the packet.
 * @param addr The address of the server to which the request is sent.
 * @return Returns 1 on success, -1 on failure with an error message printed to stderr.
 */
int request(uint16_t opcode,const char* filename, config status, int sockfd, struct sockaddr* addr);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Initializes a UDP client socket.
 * 
 * This function creates a UDP socket for the client. The socket is not bound to a specific address or port.
 * @param per_packet_timeout time out value .
 * @param addr Pointer to struct sockaddr_in where client address and port are specified. Not used in this function but included for symmetry with server initialization.
 * @param sockfd Pointer to an integer where the socket file descriptor will be stored.
 * @param server_ip IP address of the server to connect to.
 * @param server_port Port number of the server to connect to.
 * @return Returns 0 on success, -1 on failure with an error message printed to stderr.
 */
//
static int connect_to_tftp_server(int per_packet_timeout,const char* server_ip, int client_port,struct sockaddr_in* server_addr,int* sockfd);

//----------------------------------------------------------------------------------------------
/**
 * @brief Sets the file transfer mode.
 * 
 * This function allows the user to set the file transfer mode, such as binary or ASCII.
 * 
 * @param mode The desired file transfer mode (e.g., "binary" or "ascii").
 */
//static void set_file_transfer_mode(const char* mode,char* transfer_mode);

//----------------------------------------------------------------------------------------------
/**
 * @brief Sends a file to the TFTP server.
 * 
 * This function sends a file to the connected TFTP server.
 * 
 * @param filename The name of the file to send.
 */
static int send_file(const char* filename,config status,struct sockaddr_in* server_addr, int sockfd);

//----------------------------------------------------------------------------------------------
/**
 * @brief Receives a file from the TFTP server.
 * 
 * This function receives a file from the connected TFTP server.
 * 
 * @param filename The name of the file to receive.
 */
static int receive_file(const char* filename, config status,struct sockaddr_in* server_addr,int sockfd) ;

//----------------------------------------------------------------------------------------------
/**
 * @brief Exits the TFTP client.
 * 
 * This function exits the TFTP client program.
 */
static void exit_tftp_client();

//----------------------------------------------------------------------------------------------
/**
 * @brief Toggles verbose mode.
 * 
 * This function toggles verbose mode, which controls whether detailed information is displayed during file transfers.
 */
void set_verbose();

//----------------------------------------------------------------------------------------------
/**
 * @brief Toggles packet tracing.
 * 
 * This function toggles packet tracing, which controls whether packet-level information is displayed during file transfers.
 */
static void set_trace(config* status);

//----------------------------------------------------------------------------------------------
/**
 * @brief Shows the current status.
 * 
 * This function displays the current status of the TFTP client, including the server it's connected to and the file transfer mode.
 */
static void print_status();

//----------------------------------------------------------------------------------------------
/**
 * @brief Sets the per-packet retransmission timeout.
 * 
 * This function allows the user to set the per-packet retransmission timeout for reliability.
 * 
 * @param timeout The desired retransmission timeout in milliseconds.
 */
void set_per_packet_retransmission_timeout(int timeout);

//----------------------------------------------------------------------------------------------
/**
 * @brief Sets the total retransmission timeout.
 * 
 * This function allows the user to set the total retransmission timeout for reliability.
 * 
 * @param timeout The desired total retransmission timeout in milliseconds.
 */
void set_total_retransmission_timeout(int timeout);

//----------------------------------------------------------------------------------------------
/**
 * @brief Processing user command by calling handle_put_command / handle_get_command ...
 * 
 * @param status The current configuration.
 * @param command The command of the user.
 * @param addr The server socket address.
 * @param sockfd The file descriptor of the socket.
 */
static void process_command(char* command,config* status,struct sockaddr_in server_addr,int sockfd);

//----------------------------------------------------------------------------------------------
/**
 * @brief Handles put command.
 * @param status The current configuration.
 * @param addr The server socket adress.
 * @param sockfd The file discriptor of the socket.
 */
static void handle_put_command(config status,struct sockaddr_in* server_addr,int sockfd) ;

//----------------------------------------------------------------------------------------------
/**
 * @brief Handles get command.
 * @param status The current configuration.
 * @param addr The server socket adress.
 * @param sockfd The file discriptor of the socket.
 */
static void handle_get_command(config status ,struct sockaddr_in* server_addr,int sockfd);

//----------------------------------------------------------------------------------------------
/**
 * @brief Prints help.
 */
static void print_help();

//----------------------------------------------------------------------------------------------
/**
 * @brief Sets the total retransmission timeout.
 * 
 * @param status The current configuration.
 */
static void print_status(config status);

//----------------------------------------------------------------------------------------------
/**
 * @brief toggle Bigfile option
 * @param status , The config structure
 * 
*/
static void toggle_bigfile_option(config* status);



#endif
