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
#include <errno.h>
#include <time.h>
#include <strings.h>
#define IP "127.0.0.1"
#define MAX_BLOCK_SIZE 516

#define PACKET_LOSS_PERCENTAGE 50

typedef struct {
    char* server ;          // server ip
    char* transfer_mode;    // transfer mode , we only implemented octet
    uint8_t trace;          // tracing packets : for debugging 
    uint8_t rexmt ;         // per-packet retransmission timeout
    uint8_t timemout;       // total retransmission timeout
}config;


// BUG : if the file size = 0;


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

/**
 * @brief Extracts the block number of a DATA/ACK packet.
 * 
 * @param packet Data packet or acknowledgement packet.
 * @return block number.
 */
uint16_t get_block_number(char* packet);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Extracts the block number of a DATA packet.
 * 
 * @param packet Data packet.
 * @return the Data.
 */
char* get_data(char* packet);
//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief print the data packet , for debugging.
 * 
 * @param packet Data packet
 * @return nothing.
 */
void print_data_packet(char* packet);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief builds an ACK packet
 * @param block_number block #
 * @param packet_size  a pointer to packet_size to affect it with the real size of the block.
 * @return A pointer to a dynamically allocated string containing the ack packet. Caller must free this string.
 */
char* build_ack_packet(uint16_t block_number, size_t* packet_size);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief builds a DATA packet
 * @param block_number block #.
 * @param data the data.
 * @param data_length the size of the data.
 * @param packet_size  a pointer to packet_size to affect it with the real size of the block.
 * @return A pointer to a dynamically allocated string containing the data packet. Caller must free this string.
 */
char* build_data_packet(uint16_t block_number, const char* data, size_t data_length, size_t* packet_size);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Sends an ACK packet over UDP.
 * 
 * Constructs and sends a TFTP ack packet to the server specified by addr.
 * @param status The configuration of the caller (serverside is for debug only).
 * @param client_addr The address of the server to which the acknowledgement is sent.
 * @param block_number The block # to acknowledge.
 * @param sockfd The socket file descriptor used to send the packet.
 * @return Returns 0 on success, -1 on failure with an error message printed to stderr.
 */
int send_ack_packet(config status,const struct sockaddr* client_addr, uint16_t block_number, int sockfd);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Sends an error packet over UDP.
 * Constructs and sends a TFTP error packet to the server specified by addr.
 * @param dest_addr The address of the server to which the error is sent.
 * @param block_number The block # to acknowledge.
 * @param sockfd The socket file descriptor used to send the packet.
 * @return Returns 0 on success, -1 on failure with an error message printed to stderr.
 */
int send_error_packet(config status,int error_code,char* error_msg, const struct sockaddr_in* client_addr, int sockfd);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Sends a data packet over UDP.
 * Constructs and sends a TFTP data packet to the server specified by addr.
 * @param block_number The block # for the data sent.
 * @param data The data sent.
 * @param client_addr The address of the server to which the data is sent.
 * @param data_length the size of the data.
 * @param sockfd The socket file descriptor used to send the packet.
 * @return Returns 0 on success, -1 on failure with an error message printed to stderr.
 */
int send_data_packet(config status,int block_number,char* data, const struct sockaddr_in* client_addr,int data_length, int sockfd);


//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Prints error message of an error packet.
 * @param error_packet The error packet.
 * @return Nothing
 */
void print_error_message(char* error_packet);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Prints informations sent.
 * @param packet The error packet.
 * @param packet_size The size of the packet.
 * @return Nothing
 */
void trace_sent(char* packet,size_t packet_size);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Prints informations received.
 * @param packet The error packet.
 * @param packet_size The size of the packet.
 * @return Nothing
 */
void trace_received(char* packet,size_t packet_size);

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
/**
 * @brief Simulates packet loss chance.
 * @param loss_percentage The percentage of packet loss occuring.
 * @return 1 if no packet loss and 0 if we have a packet loss
 */
int packet_loss(int loss_percentage);










#endif