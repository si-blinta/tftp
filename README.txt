# TFTP ETAPE 1 ET 2 

### Simple TFTP Server program
### Simple TFTP Client program
### A Server Directory : server_directory containing :
                                                      -ai.png
                                                      -test.txt
Every file is requested/sent from/to server_directory ! 
## NOTE 1 
We respected the tftp RFC, it means that the transfer does not have any security.
If you want us to implement something more secure, you are welcome.

## Compilation

Makefile for server and Makefile for client.

tftp/tftp_client/make

tftp/tftp_server/make


## Launching

you will need to be in tftp_server folder :

./server [port] [packet loss percentage]

you will need to be in tftp_client folder :

./client [server ip] [server port] [packet loss percentage]

for example to connect to the real TFTP server installed on local machine :
./client 127.0.0.1 69 0

## NOTE 2
in this implementation, even if you choose packet loss percentage to be 0 : 

[packet loss] sending data#1

this line is always printed when a packet is sent , but it may be lost.


sent DATA <block=1, 516 bytes>

this line means that the data is actually sent.(no packet loss)

## TFTP Client
#### HELP , help command.

<tftp>?
Commands available:
  put     	Send file to the server
  get     	Receive file from the server
  status  	Show current status
  trace   	Toggle packet tracing
  quit    	Exit TFTP client
  ?       	Print this help information
<tftp>


#### RRQ , type get then enter then filename. this will get the file from server_directory to current directory.

<tftp>get
<tftp>(file) filename

#### WRQ , type put then enter then filename. this will put the file from current directory to server_directory.

<tftp>put
<tftp>(file) filename

#### STATUS , this command shows the current configuration of the TFTP client.

<tftp>status
Connected to 127.0.0.1
Mode : octet
Tracing : Off
per Packet timeout : 1 seconds
Timeout : 10 seconds
<tftp>

#### TRACE , this command allows to trace every packet sent or received.

<tftp>status
Packet tracing on
<tftp>

#### QUIT , this command allows you to quit properly the TFTP client.

<tftp>quit
Exiting TFTP client.
