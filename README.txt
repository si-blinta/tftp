# TFTP MULTITHREADED VERSION

### Simple TFTP Server program
### Simple TFTP Client program
### A Server Directory : server_directory containing :
                                                      -ai.png
Every file is requested/sent from/to server_directory ! 

## Compilation

./build.sh
it will compile and also creates directories for clients to simulate a multiuser scenario.

## Launching

you will need to be in tftp_server folder :

./server [port] [packet loss percentage]  ( i suggest to put packet loss at 10 )

you will need to be in client-x folder : (x number of the client)

./client-x [server ip] [server port]

## HOW

Our multithreaded server is using a pool of threads, if a user wants to write / read
and all threads are busy , he will receive a NOT DEFINED ERROR saying that no thread is available.

We respected the Writers / Readers problem.

## Debugging

In the server program : We print each packet received / sent by which thread and also which file is open , for which operation.
In the client program : We still use the tracing : dont forget to activate it by typing "trace".
In build.sh           : We create POOL_SIZE +1 directories (clients) so you can test if number_clients > POOL_SIZE.
Feel free to modify POOL_SIZE in server.h