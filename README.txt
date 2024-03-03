# TFTP MULTITHREADED VERSION

### Mono threaded TFTP Server program using Select
### Simple TFTP Client program
### A Server Directory : server_directory containing :
                                                      -ai.png
Every file is requested/sent from/to server_directory ! 

## Compilation

./build.sh
it will compile and also creates directories for clients to simulate a multiusers scenario.

## Launching

you will need to be in tftp_server folder :

./server [port] [packet loss percentage]  ( we suggest to put packet loss at 10 )

you will need to be in client-x folder : ( x number of the client )

./client-x [server ip] [server port] [packet loss percentage] ( we suggest to put packet loss at 10 )

## HOW

Our monothreaded server is using a pool of client handlers, if a user wants to write / read
and all the handlers are busy , he will receive a NOT DEFINED ERROR saying that no thread is available.

We respected the Writers / Readers problem : 
We send an ACCESS_VIOLATION error if we encounter writers / readers problem.
(could have implemented some time out solution , but no time to do it )

## Debugging

In the server program : We print each packet received / sent by which client_handler and also which file is open , for which operation.
In the client program : We still use the tracing : dont forget to activate it by typing "trace".
In build.sh           : We create POOL_SIZE +1 directories (clients) so you can test if number_clients > POOL_SIZE.
Feel free to modify POOL_SIZE in server.h

## Note

In client side : 
    Even if the file doesn't exist in the server directory and we do a get request we create it. ( we dont delete the file on failure )
In server side :
    Even if it the server thread time outs , it prints RRQ/WRQ FAILED time out reached ... , it means that the thread didnt receive the last ack.
    Which can happen if the last ack is lost but the client received the whole file.
    To check if it actually failed , check the block size < 516.