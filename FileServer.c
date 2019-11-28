#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdbool.h>


#define PORT htons(1234)
#define SA struct sockaddr
#define MAXFNLEN 512
#define MAXBUFFLEN 100000
#define FDIR "./Files"

void GetFilesInDirectory(char* dir)
{
	struct dirent *de;  // Pointer for directory entry
	FILE* file; 

    	DIR *dr = opendir(dir);

    	if (dr == NULL)  // opendir returns NULL if couldn't open directory
    	{
        	printf("Could not open current directory" );        
    	}
	

	if((file = fopen("FilesInDirectory.txt", "w")) == NULL)
	{
		printf("Child: Failed creating a new file\n");	
	}

	//Skip . and ..
	de = readdir(dr);
	de = readdir(dr);	

    	while ((de = readdir(dr)) != NULL)
	{
		fputs(de->d_name, file);
		fputs("\n", file);
            	//printf("%s\n", de->d_name);
	}

	fclose(file);
    	closedir(dr);
}

int SendFile(int clientSocket, char* fileName)
{
	char filePath[MAXFNLEN + 512];
	FILE* file;
	struct stat fileInfo;
	long fileLength, bytesSent, bytesSentTogether, bytesRead;
	unsigned char buff[MAXBUFFLEN];

	int n;
	n = sprintf(filePath, "%s/%s", FDIR, fileName);	
	printf("Child: Client requests file %s\n", fileName);
	
	//Get info about requested file
	if(stat(filePath, &fileInfo))
	{	
		printf("Child: Failed getting info about file %s\n", fileName);
		return -1;
	}

	//Check file size
	fileLength = (long) fileInfo.st_size;
	if(fileLength == 0)
	{
		printf("Child: File %s is empty\n", fileName);
		return -1;
	}
	printf("Child: File %s is %lu bytes long\n", fileName, fileLength);	

	//Send file size
	//fileLength = htonl(fileLength);
	if(send(clientSocket, &fileLength, sizeof(long), 0) != sizeof(long))
	{
		printf("Child: Failed sending file's %s size\n", fileName);
		return -1;
	}

	//Open requested file
	//fileLength = ntohl(fileLength);
	bytesSentTogether = 0;
	file = fopen(filePath, "rb");
	if(file == NULL)
	{
		printf("Child: Failed opening file %s\n", fileName);
		return -1;
	}
	printf("Child: Success opening file %s\n", fileName);
	
	
	//Client is ready to recieve
	bool flag = true;
	if(recv(clientSocket, &flag, sizeof(bool), 0) != sizeof(bool))
	{
		printf("Child: Client is not ready to recieve the file\n");
		return -1;
	}
	

	//Send requested file
	int i = 1;
	while(bytesSentTogether < fileLength)
	{
		bytesRead = fread(buff, 1, MAXBUFFLEN, file);
		bytesSent = send(clientSocket, buff, bytesRead, 0);
		printf("Child: Packet %d: %ld\n",i,bytesSent);
		if(bytesRead != bytesSent) break;
		bytesSentTogether += bytesSent;	
		i++;	
	}

	if(bytesSentTogether != fileLength)
	{
		printf("Child: Failed sending file %s\n", fileName);
		return -1;
	}
	else
	{
		printf("Child: Success sending file %s\n", fileName);
		return 0;
	}
	
}

char* RecvFileName(int clientSocket)
{
	static char fileName[MAXFNLEN];
	memset(fileName, 0, MAXFNLEN); 
	printf("Child: Getting file name\n");
	if(recv(clientSocket, &fileName, MAXFNLEN, 0) < 0)
	{
		printf("Child: Failed getting file name\n");
		return "\0";	
	}
	printf("Child: Success getting file name - %s\n", fileName);
	return fileName;
}

int GetFile(int clientSocket)
{
	char fileName[MAXFNLEN];
	char filePath[MAXFNLEN + 512];
	char buff[MAXBUFFLEN];
	long fileLength, bytesRecieved, bytesRecievedTogether;
	bool flag = true;	
	FILE* file;

	//Get file name
	memset(fileName, 0, MAXFNLEN); 
	printf("Child: Getting file name\n");
	if(recv(clientSocket, &fileName, MAXFNLEN, 0) < 0)
	{
		printf("Child: Failed getting file name\n");
		return -1;	
	}
	printf("Child: Success getting file name - %s\n", fileName);
	
	//Notify ready to recieve file length	
	send(clientSocket, &flag, sizeof(bool), 0);

	//Get file length
	if(recv(clientSocket, &fileLength, sizeof(long), 0) != sizeof(long))
	{
		printf("Child: Failed getting file length \n");
		return -1;
	}
	
	fileLength = fileLength;
	printf("Child: Success getting file length %ld\n", fileLength);

	//Create a new file
	int n;
	n = sprintf(filePath, "%s/%s", FDIR, fileName);
	if((file = fopen(filePath, "wb")) == NULL)
	{
		printf("Child: Failed creating a new file\n");
		return -1;
	}
	printf("Child: File %s created\n", fileName);
	
	//Notify ready to recieve file
	send(clientSocket, &flag, sizeof(bool), 0);

	//Recieve data and write it to the created file
	bytesRecievedTogether = 0;
	int i = 1;
	struct timeval timeout;	
	fd_set read_fd_set;
	
	while(bytesRecievedTogether < fileLength)
	{
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
		FD_ZERO(&read_fd_set);
		FD_SET((unsigned int)clientSocket, &read_fd_set);
	
		//Set memory in buffer
		memset(buff, 0, MAXBUFFLEN);

		select(clientSocket + 1, &read_fd_set, NULL, NULL, &timeout);
		
		if(!(FD_ISSET(clientSocket, &read_fd_set)))
		{
			printf("Child: Client timed out sending the file\n");
			break;
		}
				
		//Recieve bytes from client
		bytesRecieved = recv(clientSocket, buff, MAXBUFFLEN, 0);
		printf("Child: Packet %d: %ld\n",i,bytesRecieved);

		if(bytesRecieved <= 0) //If client stopped sending packets
		{
			printf("Child: Client stopped sending the file\n");
			break;
		}

		//If bytes count reached - break
		if(bytesRecievedTogether == fileLength)
		{
			printf("Child: Bytes received after %ld loops : %d\n",
			       bytesRecievedTogether, i);
			break;
		}		
		else //If server needs more bytes
		{
			bytesRecievedTogether += bytesRecieved;
			fwrite(buff, sizeof(char), bytesRecieved, file);	
		}

		

		i++;
			
	}	
	FD_CLR(clientSocket, &read_fd_set);
	send(clientSocket, &flag, sizeof(bool), 0);
	fclose(file);
	if(bytesRecievedTogether != fileLength)
	{
		printf("Child: Failed receiving the file\n");
		if (remove(filePath) == 0) 
			printf("Child: File deleted successfully\n"); 
        	else
			printf("Child: Unable to delete the file\n");

		return -1;
	}
	else
	{
		printf("Child: Success receiving the file\n");
		return 0;	
	}
}


void Test(){printf("Hooray!\n");}

int main(void)
{    	
	GetFilesInDirectory(FDIR);

	//Set variables	
	int listenSocket, clientSocket;
	struct sockaddr_in addr;
	socklen_t addrLen = sizeof(struct sockaddr_in);
	
	//Create socket for listening
	listenSocket = socket(PF_INET, SOCK_STREAM, 0);
	if(listenSocket == -1)
	{
		printf("Parent: Failed creating listening socket\n");
		return 1;
	}
	else
	{
		printf("Parent: Success creating listening socket\n");
	}

	//Assign IP and PORT
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = PORT;

	//Bind socket
	if(bind(listenSocket, (SA*) &addr, addrLen) < 0 )
	{
		printf("Parent: Failed binding listening socket\n");
		return 1;
	}
	else
	{
		printf("Parent: Success binding listening socket\n");
	}

	//Listen for connection
	if((listen(listenSocket,10) != 0))
	{
		printf("Parent: Failed listening for connection\n");
		return 1;
	}
	else
	{
		printf("Parent: Server listening...\n");
	}	

	//Connect to multiple clients
	while(1)
	{	
		addrLen = sizeof(struct sockaddr_in);
		
		//Make a connection with a client
		clientSocket = accept(listenSocket, (SA*) &addr, &addrLen);
		if(clientSocket < 0)
		{
			printf("Parent: Failed connecting to a client");
			continue;
		}
		else
		{
			printf("Parent: Connected to %s:%u\n",
				inet_ntoa(addr.sin_addr),
				ntohs(addr.sin_port));
			
			//Create a child process
			printf("Parent: Creating a child process\n");
			if(fork() == 0)
			{
				/*----Child process----*/
				char a;
				char* fileName;
				printf("Child: Starting service\n");

				//Connection loop
				while(1) 
				{
					printf("Child: Waiting for client's request...\n");
					a = '\0';
					recv(clientSocket, &a, sizeof(char), 0);
					if(a == '\0')
					{
						printf("Child: Client timed out\n");
						break;
					}
								
					switch(a) {
						case 'a': //Recieve file
							if((GetFile(clientSocket)) == -1)
							{
								printf("Child: Operation failed\n");
								GetFilesInDirectory(FDIR);
							}
							else
							{
								printf("Child: Operation successful\n");
								GetFilesInDirectory(FDIR);
							}
							break;
						case 'b': //Send file
							fileName = RecvFileName(clientSocket);
							if(fileName[0] == '\0')
							{
								printf("Child: File name not recieved\n");
								break;
							}
							
							if((SendFile(clientSocket, fileName)) == -1)
							{
								printf("Child: Operation failed\n");
							}
							else
							{
								printf("Child: Operation successful\n");
							}
							break;
						case 'c': //Send files in directory
							GetFilesInDirectory(FDIR);
							if((SendFile(clientSocket, "../FilesInDirectory.txt")) == -1)
							{
								printf("Child: Operation failed\n");
							}
							else
							{
								printf("Child: Operation successful\n");
							}
							break;
						case 'd': //Disconnect
							printf("Child: Closing socket\n");
							close(clientSocket);
							printf("Child: Closing process\n");
							exit(0);
							break;
						default: //Wrong request
							printf("Child: Wrong request from client\n");
							break;
						}
				}
			}
			else
			{
				/*----Parent process----*/
				printf("Parent: Server going back to listening...\n");
				continue;
			}
		}

	}

    	return 0;
}
