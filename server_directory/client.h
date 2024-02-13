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
 * @brief Initializes a UDP 