#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>


#define PORT htons(1234)
#define SA struct sockaddr
#define MAXFNLEN 512
#define MAXBUFFLEN 1024

void PrintFilesInDirectory(char* dir)
{
	struct dirent *de;  // Pointer for directory entry

    	DIR *dr = opendir(dir);

    	if (dr == NULL)  // opendir returns NULL if couldn't open directory
    	{
        	printf("Could not open current directory" );        
    	}

    	while ((de = readdir(dr)) != NULL)
            	printf("%s\n", de->d_name);

    	closedir(dr);
}

int GetFile(int clientSocket)
{
	char fileName[MAXFNLEN];
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
	
	fileLength = ntohl(fileLength);
	printf("Child: Success getting file length %ld\n", fileLength);

	//Create a new file
	if((file = fopen(fileName, "wb")) == NULL)
	{
		printf("Child: Failed creating a new file\n");
		return -1;
	}
	printf("Child: File %s created\n", fileName);
	
	//Notify ready to recieve file
	send(clientSocket, &flag, sizeof(bool), 0);

	//Recieve data and write it to the created file
	bytesRecievedTogether = 0;
	while(bytesRecievedTogether < fileLength)
	{
		memset(buff, 0, MAXBUFFLEN);
		bytesRecieved = recv(clientSocket, buff, MAXBUFFLEN, 0);
		if(bytesRecieved < 0) break;
		bytesRecievedTogether += bytesRecieved;
		fputs(buff, file);
	}	
	
	
	fclose(file);
	if(bytesRecievedTogether != bytesRecieved)
	{
		printf("Child: Failed recieving the file\n");
		return -1;
	}
	else
	{
		printf("Child: Success recieving the file\n");
		return 0;	
	}
}


void Test(){printf("Hooray!\n");}

int main(void)
{    	
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
				printf("Child: Starting service\n");
				//Test();
				if((GetFile(clientSocket)) == -1)
					printf("Child: Operation failed\n");
				else
					printf("Child: Opeartion successful\n");
				printf("Child: Closing socket\n");
				close(clientSocket);
				printf("Child: Closing process\n");
				exit(0);
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

