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

#define PACKET_LOSS_RATE 50

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
 * @brief Prints the request in the form of : 