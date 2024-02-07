#ifndef CLIENT_H
#define CLIENT_H
#include "utils.h"
//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Initializes a UDP client socket.
 * 
 * This function creates a UDP socket for the client. The socket is not bound to a specific address or port.
 * 
 * @param addr Pointer to struct sockaddr_in where client address and port are specified. Not used in this function but included for symmetry with server initialization.
 * @param sockfd Pointer to an integer where the socket file descriptor will be stored.
 * @param server_ip IP address of the server to connect to.
 * @param server_port Port number of the server to connect to.
 * @return Returns 0 on success, -1 on failure with an error message printed to stderr.
 */
static int connect_to_tftp_server(const char* server_ip, int server_port);

//----------------------------------------------------------------------------------------------
/**
 * @brief Sets the file transfer mode.
 * 
 * This function allows the user to set the file transfer mode, such as binary or ASCII.
 * 
 * @param mode The desired file transfer mode (e.g., "binary" or "ascii").
 */
static void set_file_transfer_mode(const char* mode);

//----------------------------------------------------------------------------------------------
/**
 * @brief Sends a file to the TFTP server.
 * 
 * This function sends a file to the connected TFTP server.
 * 
 * @param filename The name of the file to send.
 */
static void send_file(const char* filename);

//----------------------------------------------------------------------------------------------
/**
 * @brief Receives a file from the TFTP server.
 * 
 * This function receives a file from the connected TFTP server.
 * 
 * @param filename The name of the file to receive.
 */
void receive_file(const char* filename);

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
void toggle_verbose_mode();

//----------------------------------------------------------------------------------------------
/**
 * @brief Toggles packet tracing.
 * 
 * This function toggles packet tracing, which controls whether packet-level information is displayed during file transfers.
 */
void toggle_packet_tracing();

//----------------------------------------------------------------------------------------------
/**
 * @brief Shows the current status.
 * 
 * This function displays the current status of the TFTP client, including the server it's connected to and the file transfer mode.
 */
void show_current_status();

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
 * @brief Prints help information.
 * 
 * This function prints help information and usage instructions for the TFTP client.
 */
static void process_command(const char* command) ;
static void handle_put_command() ;
static void handle_get_command();
static void print_help();























#endif
