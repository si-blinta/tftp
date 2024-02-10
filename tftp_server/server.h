#ifndef SERVER_H
#define SERVER_H

#include "utils.h"

//-------------------------------------------------------------------------------------
/**
 * @brief Initializes a UDP server socket.
 * 
 * This function creates a UDP socket and binds it to a specified port to listen for incoming client requests.
 * 
 * @param sockfd Pointer to an integer where the socket file descriptor will be stored.
 * @param port Port number on which the server will listen for incoming connections.
 * @return Returns 0 on success, -1 on failure with an error message printed to stderr.
 */
static int init_tftp_server(int port,int* sockfd,struct sockaddr_in* addr);

//-------------------------------------------------------------------------------------
/**
 * @brief Handles incoming client requests.
 * 
 * This function listens for and processes incoming client requests, including read requests (RRQ) and write requests (WRQ).
 * 
 * @param sockfd The socket file descriptor used to listen for incoming requests.
 */
static void handle_client_requests(config status,int sockfd,struct sockaddr_in* addr,struct sockaddr_in* client_addr);

//-------------------------------------------------------------------------------------
/**
 * @brief Processes a read request from a client.
 * 
 * This function handles a client's read request (RRQ) by sending the requested file to the client.
 * 
 * @param filename The name of the file requested by the client.
 * @param client_addr Pointer to struct sockaddr_in containing the client's address information.
 * @param sockfd The socket file descriptor for communicating with the client.
 * @return Returns 0 on success, -1 on failure.
 */
static int process_rrq(config status,char* filename,char* mode, const struct sockaddr_in* client_addr, int sockfd);
//-------------------------------------------------------------------------------------
/**
 * @brief Processes a write request from a client.
 * 
 * This function handles a client's write request (WRQ) by receiving a file from the client and saving it to the server.
 * 
 * @param filename The name of the file to be received from the client.
 * @param client_addr Pointer to struct sockaddr_in containing the client's address information.
 * @param sockfd The socket file descriptor for communicating with the client.
 * @return Returns 0 on success, -1 on failure.
 */
static int process_wrq(config status,char* filename,char* mode, const struct sockaddr_in* client_addr, int sockfd);

//-------------------------------------------------------------------------------------
/**
 * @brief Sends an error packet to a client.
 * 
 * This function sends an error packet to the client, indicating a problem with the client's request or the server's ability to fulfill it.
 * 
 * @param error_code The TFTP error code to be sent to the client.
 * @param error_msg The error message associated with the error code.
 * @param client_addr Pointer to struct sockaddr_in containing the client's address information.
 * @param sockfd The socket file descriptor for communicating with the client.
 */

#endif // SERVER_H
