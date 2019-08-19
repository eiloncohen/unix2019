#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>


#define PORT 0x0da2
#define IP_ADDR 0x7f000001
#define QUEUE_LEN 20

int main(int argc, char* argv[])
{
	printf("hello"); 
	int listenS = socket(AF_INET, SOCK_STREAM, 0);
	int socket_opt_val = 1 , addrlen , new_socket , client_socket[30] ,  
    max_clients = 30 , activity, i , valread , sd , max_sd; 
	fd_set readfds;  
		
	setsockopt(listenS, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR, &socket_opt_val, sizeof(int));
	if (listenS < 0)
	{
		perror("socket");
		return 1;
	}
	
	struct sockaddr_in s = {0};
	s.sin_family = AF_INET;
	s.sin_port = htons(PORT);
	s.sin_addr.s_addr = htonl(IP_ADDR);
	if (bind(listenS, (struct sockaddr*)&s, sizeof(s)) < 0)
	{
		perror("bind");
		return 1;
	}
	
	 if (listen(listenS, QUEUE_LEN) < 0)
	{
		perror("listen");
		return 1;
	}
	 
	 //accept the incoming connection  
    addrlen = sizeof(s);  	
	
    printf("Waiting for connections ...");  

    while(1){
	
	struct sockaddr_in clientIn ={0};
	int clientInSize = sizeof clientIn;

	   //clear the socket set  
	
        FD_ZERO(&readfds);   
	   //add master socket to set  
	    
        FD_SET(listenS, &readfds);   
		max_sd = listenS;   
       	
		        
        //add child sockets to set  
        for ( i = 0 ; i < max_clients ; i++)   
        {   
            //socket descriptor  
            sd = client_socket[i];   
                 
            //if valid socket descriptor then add to read list  
            if(sd > 0)   
                FD_SET( sd , &readfds);   
                 
            //highest file descriptor number, need it for the select function  
            if(sd > max_sd)   
                max_sd = sd;   
        }   
	    activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);   
        if ((activity < 0) && (errno!=EINTR))   
        {   
            printf("select error");   
        }   
	int newfd = accept(listenS, (struct sockaddr*)&clientIn, (socklen_t*)&clientInSize);
	if (newfd < 0)
	{
		perror("accept");
		return 1;
	}
	int op=0,fileFd,fileSize;
	if (recv(newfd, &op, sizeof(op), 0) < 1)
	{
		perror("Could not receive op ");
		exit(1);
	}
		printf("Connection accepted from %s:%d\n", inet_ntoa(clientIn.sin_addr), ntohs(clientIn.sin_port));
           
	int filenameLength = 0;
	char* filename =0;
	char path[512]={0};
	struct stat st;
	int b,tot;
    char buff[1200];
    int num;
  	FILE *fp;
	 int ch = 0;
	char sdbuf[256]; 


	switch (op)
	{
		case 1: //send single file to the client
		if (recv(newfd, &filenameLength, sizeof(filenameLength), 0) < 1)
		{
			perror("Could not receive filename length");
			exit(1);
		}
		filename = (char*) malloc( filenameLength + 1);
		memset(filename, 0, filenameLength);
		if (recv(newfd, filename, filenameLength, 0) < 1)
		{
			perror("Could not receive filename");
			exit(1);
		}
		if(argc!=1){
			strcat(path,argv[1]);
		}
		strcat(path,filename);
		if (stat(path, &st) < 0)
		{
			int err = -1;
			send(newfd, &err, sizeof(err), 0);
			//close(newfd);
			//close(listenS);
			//exit(-1);	
		}else
		{
			printf("%s\n", filename);
			if (send(newfd, &st.st_size, sizeof(st.st_size), 0) < 0)
			{
				perror("Could not send filesize to client");
				exit(1);
			}
			 fileFd = open(filename, O_RDONLY);
			if (fileFd)
			{
				void* file = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fileFd, 0);
				if (file)
				{
					int count = send(newfd, file, st.st_size, 0);
					if (count != st.st_size)
						printf("Error writing to socket");
					munmap(file, st.st_size);
				}
			}
		}
		break;
	
	case 2:// save file in the server

       if (recv(newfd, &filenameLength, sizeof(filenameLength), 0) < 1)
		{
			perror("Could not receive filename length");
			exit(1);
		}
		 filename = (char*) malloc(filenameLength + 1);
		memset(filename, 0, filenameLength);
		if (recv(newfd, filename, filenameLength, 0) < 1)
		{
			perror("Could not receive filename");
			exit(1);
		}
		if(argc!=1){
			strcat(path,argv[1]);
		}
		strcat(path,filename);
		recv(newfd,&fileSize,sizeof(fileSize),0);
		//if file size less then zero
		if(fileSize<0){
			close(newfd);
			continue;
		}
		fileFd = open(path,O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		ftruncate(fileFd, fileSize);
		void* addr = mmap(0, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fileFd, 0);
		char *data = malloc(sizeof(fileSize));
		int count = recv(newfd, addr, fileSize, MSG_WAITALL);
		if (count < 0)
		{
			printf("error receive: %s\n",strerror(errno));
		}
		munmap(addr, fileSize);
		close(fileFd);
		break;
    

		case 3://download multiple files
		
		break;
	}
	close(newfd);
    }
	close(listenS);
	return 0;
}