#ifndef SERVER_H
#define SERVER_H
#include "utils.h"
#include <pthread.h>
#define SERVER_DIRECTORY "../server_directory/"
#define POOL_SIZE 2
typedef struct {
    int id;
    int working;
    pthread_t thread;
    config status;
    int sockfd;
    char* packet;
    struct sockaddr_in client_addr;
}thread_context;

typedef struct {
    char* filename;
    int  status;
    pthread_mutex_t mutex;
}file_control;

enum file_satus{
    FILE_READ,
    FILE_WRITE,
    FILE_IDLE
};
enum thread_work_type{
    THREAD_IDLE,
    THREAD_READING,
    THREAD_WRITING
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
 * @param args A pointer to a thread_context.
 */
void* handle_client_requests(void* args);

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
static int process_rrq(config status,char* filename,char* mode, const struct sockaddr_in* client_addr, int sockfd, int thread_id, int* thread_working);
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
static int process_wrq(config status,char* filename,char* mode, const struct sockaddr_in* client_addr, int sockfd, int thread_id,int* thread_working);

//-------------------------------------------------------------------------------------
/**
 * @brief Initializes file_control structure.Puts filename to NULL and status to INIT.
 * @param fc The adress of the structure.
 */
void file_control_init (file_control  fc[POOL_SIZE]);

//-------------------------------------------------------------------------------------
/**
 * @brief Update informations for file_control variable.
 * @param fc The adress of the structure.
 * @param status The type of operation : READ / WRITE / INIT
 * @param thread_id The Id of the thread responsable of the operation
 * @param filename  The filename
 */
void file_control_modify(file_control fc[POOL_SIZE], int status, int thread_id, char* filename);

//-------------------------------------------------------------------------------------
/**
 * @brief Determins if we can do an operation on a file.
 * @param fc The structure.
 * @param filename  The filename to check if its opened.
 * @param status The type of operation that we want to do : FILE_READ / FILE_WRITE / FILE_IDLE
 * @return the thread_id that opened the file  , -1 if the file is not opened.
 * 
 */
int file_control_available(file_control fc[POOL_SIZE],char* filename, int status );

//-------------------------------------------------------------------------------------
/**
 * @brief Handles the Readers Writers problem , it also handles the case when a mutex lock takes a lot of time , so it times out .
 * @param thread_id The thread id .
 * @param filename  The name of the file that we want to access.
 * @param status The type of operation that we want to do : FILE_READ / FILE_WRITE / FILE_IDLE 
 * @return the thread_id that opened the file  , -1 if the file is not opened and -2 if the lock of the mutex takes a long time.
 * 
 */
int file_control_synchronize(char* filename,int thread_id, int status);

//-------------------------------------------------------------------------------------
/**
 * @brief Unlocks mutexes and updates files_opened global variable.
 * @param thread_id The thread id .
 * @param id_opened  The return value of the function file_control_synchronize.
 * ( very important, to avoid unlocking a mutex that is not locked this can happen when a mutex_lock timed out  )
 * @param status The type of operation that we want to do : FILE_READ / FILE_WRITE / FILE_IDLE 
 * 
 */
void file_control_exit(int thread_id,int id_opened, int status );

//-------------------------------------------------------------------------------------
/**
 * @brief Prints files opened and their operations : for debug purposes
 * 
 */
void file_control_print();


#endif // SERVER_H
