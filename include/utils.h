#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h> 
#define PORT 8080
#define MAX_BLOCK_SIZE 516
#if defined(_WIN32) || defined(_WIN64)
    // Windows (32-bit and 64-bit)
    #define PLATFORM_NAME "windows"
#elif defined(__unix__) || defined(__unix)
    // UNIX
    #define PLATFORM_NAME "unix"
#endif
//Remarques : htons ? 

/**
 * important informations about packets :
 * 
 * RRQ / WWRQ packets : has a NON fixed size :
 *          
 *          2 bytes     string    1 byte     string   1 byte
 *           ------------------------------------------------
 *          | Opcode |  Filename  |   0  |    Mode    |   0  |
 *           ------------------------------------------------
 * 
 * Data packets : has maximum of 516 bytes : 
 *              - if the Data size is < 512     ->  signals the ebd of the transfer 
 *              - block numbers on data packets begins with 1 and increases by 1     
 *                  
 *                  2 bytes     2 bytes      n bytes
 *                  ----------------------------------
 *                | Opcode |   Block #  |   Data     |
 *                ----------------------------------
 *  
 * 
 * Ack packets : 
 *               - block number -> the block that is acknowledged
 *               - WRQ is acknowledged with block number = 0
 * 
 *                          2 bytes     2 bytes
 *                        ---------------------
 *                       | Opcode |   Block #  |
 *                        ---------------------
 * 
 * 
 * Error packets : 
 *              - can acknowledge all other packets.
 *              - error message is in netascii
 *              
 *                  2 bytes     2 bytes      string     1 byte
 *               ----------------------------------------
 *               | opcode    |  ErrorCode |   ErrMsg   |   0  |
 *               ---------------------------------------- 
 * 
 * 
 * Error Codes

   Value     Meaning

   0         Not defined, see error message (if any).
   1         File not found.
   2         Access violation.
   3         Disk full or allocation exceeded.
   4         Illegal TFTP operation.
   5         Unknown transfer ID.
   6         File already exists.
   7         No such user.
*/





/**
 * @enum Operation codes for TFTP operations.
 */
enum opcode{
    RRQ = 1,
    WRQ,
    DATA,
    ACK,
    ERROR
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------------

/**
 * @enum Codes of errors in the ERROR packet.
 */
enum error {
    NOT_DEFINED,
    FILE_NOT_FOUND,
    ACCESS_VIOLATION,
    MEMORY,
    ILLEGAL_OPERATION,
    UKNOWN_ID,
    FILE_ALREADY_EXISTS,
    NO_SUCH_USER
};




//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Sends a request packet over UDP.
 * 
 * Constructs and sends a TFTP request packet to the server specified by addr.
 * 
 * @param type The type of request ( RRQ or WRQ )
 * @param filename The name of the file to read.
 * @param mode The transfer mode ("netascii", "octet", or "mail").
 * @param sockfd The socket file descriptor used to send the packet.
 * @param addr The address of the server to which the request is sent.
 * @return Returns 1 on success, -1 on failure with an error message printed to stderr.
 */
int request(uint16_t opcode,const char* filename, const char* mode, int sockfd, struct sockaddr* addr);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Extracts the opcode from a TFTP packet.
 * 
 * @param packet The TFTP packet from which to extract the opcode.
 * @return The opcode as an integer.
 */
uint16_t get_opcode(char* packet);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Extracts the file name from a TFTP RRQ or WRQ packet.
 * 
 * @param packet The TFTP packet from which to extract the file name.
 * @return A pointer to a dynamically allocated string containing the file name. Caller must free this string.
 */
char* get_file_name(char* packet);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Extracts the transfer mode from a TFTP RRQ or WRQ packet.
 * 
 * @param packet The TFTP packet from which to extract the mode.
 * @return A pointer to a dynamically allocated string containing the mode. Caller must free this string.
 */
char* get_mode(char* packet);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Prints the request in the form of : opcode
 *                                            filename
 *                                            mode
 * 
 * @param packet The TFTP packet from which to extract the informations.
 * @return Nothing
 */
void print_request_packet(char* packet);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Extract error code from a TFTP ERROR packet.
 * 
 * @param packet The TFTP packet from which to extract the error code.
 * @return The error code as integer.
 */
uint16_t get_error_code(char* packet);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Extracts the error message from a TFTP ERROR packet.
 * 
 * @param packet The TFTP packet from which to extract the error code.
 * @return A pointer to a dynamically allocated string containing the error message. Caller must free this string.
 */
char* get_error_message(char* packet);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Builds an error packet
 * 
 * @param error_code the error code.
 * @param error_msg the error message.
 * @return A pointer to a dynamically allocated string containing the error packet. Caller must free this string.
 */
void print_error_packet(char* packet);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Prints the error packet in the form of : opcode
 *                                                 error code
 *                                                 error message
 * 
 * @param packet The TFTP packet from which to extract the informations.
 * @return Nothing
 */
char* build_error_packet(uint16_t error_code, char* error_msg,size_t* packet_size);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Converts text from Unix/Linux line endings (LF) to Netascii line endings (CR LF).
 *
 * @param buffer A pointer to the input text buffer to be converted. This buffer is expected
 *              to contain text using Unix/Linux line endings (LF).
 *
 * @return The length of the converted text. This length may be
 *         greater than the input length if line endings were converted.
 */
size_t convert_to_netascii(char* buffer);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Converts text from Netascii format to the native end-of-line format.
 * 
 * @param data The Netascii text data to be converted, modified in place.
 * @param length Pointer to an integer representing the length of the input data. The function updates this value to reflect the length of the converted text.
 * @return Nothing.
 */

//--------------------------------------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief TODO
 * 
 * @param data TODO
 * @param length TODO
 * @return TODO.
 */
uint16_t get_block_number(char* packet);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief TODO
 * 
 * @param data TODO
 * @param length TODO
 * @return TODO.
 */
char* get_data(char* packet);
//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief TODO
 * 
 * @param data TODO
 * @param length TODO
 * @return TODO.
 */
void print_data_packet(char* packet);
//TODO
int handle_request(char* packet, struct sockaddr_in* client_addr,int sockfd);

//TODO : gérer les modes netascii , octet et email
int handle_rrq(char* packet,struct sockaddr_in* client_addr,int sockfd);

char* build_ack_packet(uint16_t block_number, size_t* packet_size);
char* build_data_packet(uint16_t block_number, const char* data, size_t data_length, size_t* packet_size);
int send_ack_packet(int sockfd, const struct sockaddr* dest_addr, socklen_t addrlen, uint16_t block_number);
int send_error_packet(int error_code,char* error_msg, const struct sockaddr_in* client_addr, int sockfd);
void print_error_message(char* buf);


















#endif