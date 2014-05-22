FTP client and server
==========

#1.	Protocol Specification
The following section specifies the protocol for communication between the myftp client and the myftpd server. 
##1.1.	Transport Layer Protocol and Port Number
The transport layer protocol used is TCP (transmission control protocol). 
The default port for the myftp protocol is 40903.
## 1.2.	PUT Command
Message Sequence and Format

##### 1. The client sends one message to the server to request a file  upload. The message format is given below:
* one byte opcode, which is ASCII character P, followed by
* two-byte integer in two's compliment and in network byte order, which represents the length of the file name of the file sent to the server by
* the sequence of ASCII characters representing the file name

##### 2. Once the server receives the message with the appropriate opcode, it responds with:
* one byte opcode which is ASCII character P followed by
* one-byte acknowledgement code, which is one of the following ASCII characters:
 * 0 - The server is ready to accept the named file
 * 1 - The server cannot accept the file because a file with the same name already exists in the target directory.
 * 2 - The server cannot accept the file because it cannot create the named file.
 * 3 - Server cannot accept due to miscellaneous error.

##### 3. Once the client receives the P message from the server and if the acknowledgement code is 0, it responds with the following message:
* one byte opcode which is the ASCII character Q followed by
* four byte integer in two's compliment and in network byte order which represents the length of the file followed by
* a sequence of N bytes which is the content of the file. Files are transferred in 5120 byte blocks. If the file size is above 5120 bytes, multiple read/ writes must be constructed.
##### 4.	Once the server receives the Q message from the client, the server responds with:
* a one byte opcode which is ASCII character Q followed by
* a one byte status code, which is one of the following ASCII characters:
 * 0 - File successfully written to disk
 * 1 - Server unable to write file to disk

#### PUT Content
##### 1 Client Request
> Opcode	Length	file name

##### 2 Server Response
> Opcode	Status

##### 3 Client Request
> Opcode	Length	file contents

##### 4 Server Response
> Opcode	status

#### PUT Example
#####  1 Client Request
> P	7	foo.txt

##### 2 Server Response
> P	0

##### 3 Client Request
> Q	1335	foo.txt contents

##### 4 Server Response
> Q	0

 
## 1.3.	GET Command

Message Sequence and Format
##### 1.	The client sends a message to the server to request a file download. The message format is given below:
*	one byte opcode, which is ASCII character G, followed by
*	two-byte integer in two's compliment and in network byte order, which represents the length of the file name of the file downloaded from the server by
*	the sequence of ASCII characters representing the file name
##### 2.	Once the server receives the message with the appropriate opcode, it responds with:
*	one byte opcode which is ASCII character G followed by
*	one-byte acknowledgement code, which is one of the following ASCII characters:
 *	0 - The server is ready to send the named file to the client
 *	1 - The file name specified does not exist
 *	2 - File permissions error
 *	3 - The server cannot send file due to miscellaneous error.
##### 3.	Once the client receives the G message from the server and if the acknowledgement code is 0, it issues the following message:
*	one byte opcode which is the ASCII character H followed by
*	four byte integer in two's compliment and in network byte order which represents the length of the file followed by
*	a sequence of N bytes which is the content of the file
##### 4.	Once the server receives the H message from the client, the server responds with:
*	a one byte opcode which is ASCII character H followed by
*	a one byte status code, which is one of the following ASCII characters:
 *	0 - File transfer initiated successfully
 *	1 - File name specified does not exist
 *	2 - File permission error
 *	3 - The server encountered a miscellaneous error
*	four byte integer in two's compliment and in network byte order which represents the length of the file followed by
*	a sequence of N bytes which is the content of the file. Files are transferred in 5120 byte blocks. If the file size is above 5120 bytes, multiple read/ writes must be constructed.

#### GET Content
##### 1 Client Request
> opcode	length	file name

##### 2 Server Response
> opcode	status

##### 3 Client Request
> opcode	length	file name

##### 4 Server Response
> opcode	status	length	file content

#### GET Example
##### 1 Client Request
> G	7	bar.txt

##### 2 Server Response
> G	0

##### 3 Client Request
> H	7	bar.txt

##### 4 Server Response
> H	0	1400	bar.txt file contents

 

## 1.4.	PWD Command
Message Sequence and Format
#####  1. The client sends a message to the server to request the current directory of the server. The message format is given below:
* one byte opcode, which is ASCII character W
#####  2. Once the server receives the message with the opcode W, it responds with:
* one byte opcode, which is ASCII character W, followed by
* two-byte integer in two's compliment and in network byte order, which represents the length of the path returned in the result
* a one byte status code containing one of the following ASCII characters:
 * 0 - Command completed successfully
 * 1 - Miscellaneous error
* a sequence of ASCII characters representing the current path

#### PWD Content
##### 1 Client Request
> opcode

##### 2 Server Response
> opcode	length	status	path

#### PWD Example
##### 1 Client Request
> W

##### 2 Server Response
> W	16	0	/home/user/temp

 
## 1.5.	DIR Command
Message Sequence and Format
##### 1.	The client sends a message to the server to request the contents of the current directory of the server. The message format is given below:
* one byte opcode, which is ASCII character D
##### 2.	Once the server receives the message with the opcode D, it responds with:
* one byte opcode, which is ASCII character D followed by
* two-byte integer in two's compliment and in network byte order, which represents the length of the list of files
* a one byte status code containing one of the following ASCII characters:
 * 0 - Navigated directory successfully
 * 1 - Miscellaneous error
* a sequence of ASCII characters representing the current directory contents

#### DIR Content
##### 1 Client Request
> opcode

##### 2 Server Response
> opcode	length	status	list of files

#### DIR Example
##### 1 Client Request
> D

##### 2 Server Response
> D	23	0	foo.txt bar.txt baz.txt

## 1.6.	CD Command
Message Sequence and Format
##### 1.	The client sends a message to the server to request a change of the host directory. The message format is given below:
* one byte opcode, which is ASCII character C
* a two-byte integer in two's compliment and in network byte order, which represents the length of the directory to navigate to
* a sequence of ASCII characters representing the host directory to navigate to
##### 2.	Once the server receives the message with the opcode C, it responds with:
* one byte opcode, which is ASCII character C followed by
* a one byte status code containing one of the following ASCII characters:
 * 0 - Navigated to given directory successfully
 * 1 - Miscellaneous error

#### CD Content
##### 1 Client Request
> opcode	length	directory

##### 2 Server Response
> opcode	result

#### CD Example
##### 1 Client Request
> C	16	/home/user/temp

##### 2 Server Response
> C	0
