/*
* File: peer.c
* Author: Sergio Gil Luque
* Version: 1.0
* Date: 09-21-15
*/

#include<stdlib.h>
#include<stdio.h>
#include<string.h>  
#include<sys/socket.h>
#include<arpa/inet.h> 
#include<time.h>
#include<fcntl.h> 
#include<unistd.h> 
#include<pthread.h>

#define SERVER_PORT 5000

/* Prototypes */
int sendFile (int sock, char* fileName);
int obtainFile (int port, char* fileName);
void showPromptMessage (int option);

/* Global variables */
char *peerID;
int testingMode = 0;
int seqRuns = 1;
int peerPort = 0;
int globalOpt = 1;


/*
 * This function will handle the connection for each request
 */

void *connection_handler(void *socket_desc)
{
	char request_received[2000];
	int sock = *(int*)socket_desc;
	char *message;
	char *serverReply;
	     
	read (sock , request_received , 255);
	printf ("+A request has been received: %s\n", request_received);

	message = strtok(request_received ," ");

	//The only call that the peer can recieve is an obtain() call. Otherwise error

	if (strcmp(message,"OBTAIN")==0) /* Check for errors and exceptions */
	{	
		printf("+Obtain file request\n");
		char *fileName = strtok(NULL, " "); //Getting fileName from the request

    		if (sendFile(sock, fileName) < 0)
		{
			printf("*File %s could not be obtained\n", fileName);
		}
		else
		{
			printf("+File %s obtained successfully\n", fileName);
		}
		//showPromptMessage(globalOpt);
	}

	else
	{
		printf ("*Error: Bad request recieved\n");
		serverReply = "ERROR BAD_REQUEST";
		write(sock, serverReply, sizeof(serverReply));
	}

    //Free the socket pointer
    free(socket_desc);
    showPromptMessage(globalOpt);
    return (void *) 0;
}


void *incoming_connections_handler (void* data)
{
    //printf ("+Initializing incoming connection handler...\n");
     
    int IN_socket_desc , IN_new_socket , c , *IN_new_sock;
    struct sockaddr_in IN_server , client;
     
    //Create socket
    IN_socket_desc = socket(AF_INET , SOCK_STREAM , 0);

    if (IN_socket_desc == -1)
    {
        perror("*Could not create socket");
	return (void *) 1;
    }
     
    //Prepare the sockaddr_in structure
    IN_server.sin_family = AF_INET;
    IN_server.sin_addr.s_addr = INADDR_ANY;
    IN_server.sin_port = htons( peerPort );
     
    //Bind
    if( bind(IN_socket_desc,(struct sockaddr *) &IN_server , sizeof(IN_server)) < 0)
    {
        perror("*Bind failed");
        return (void *) 1;
    }
     
    //Listen
    listen(IN_socket_desc , 1000); // The peer can handle 1000 simulteaneous connections
     
    //Accept and incoming connection
    //printf("+Waiting for incoming connections in port %i...\n", peerPort);
    //showPromptMessage (globalOpt);
    c = sizeof(struct sockaddr_in);
	
    while( (IN_new_socket = accept(IN_socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        printf("+New connection accepted\n");
         
        pthread_t sniffer_thread;
        IN_new_sock = malloc(1);
        *IN_new_sock = IN_new_socket;
         
        if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) IN_new_sock) < 0)
        {
            perror("*Could not create thread");
            return (void *) 1;
        }
         
        //Now join the thread , so that we dont terminate before the thread
        pthread_join( sniffer_thread , NULL);
        //puts("Handler assigned");
    }
     
    if (IN_new_socket<0)
    {
        perror("*Accept failed");
        return (void *) 1;
    }
     
    return (void *) 0;
}

int searchFile (char* fileName)
{
    int sock;
    struct sockaddr_in server;
    char serverReply[2000];
    char *serverCode;
    char request[80];
    char peerList[2000];
    char option[10];
    int fixedOption;
    int i;


    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        printf("*Could not create socket");
    }
    //puts("Socket created");
     
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( SERVER_PORT );
 
    //Connect to server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("*Connection to server failed");
        return -1;
    }
     
    	printf("Connected to server\n");

	strcpy(request, "SEARCH");
	strcat(request, " ");
	strcat(request, fileName);

    	printf("\nThe request that is going to be sent is %s\n", request);
    	write(sock, request, strlen(request));
	read (sock , serverReply , sizeof(serverReply));

	strncpy(peerList, serverReply, sizeof(serverReply));
	//printf("Recieved: %s\n", serverReply);
	serverCode = strtok(serverReply," "); // The first string is the server reply
	printf ("Server reply is: %s\n", serverCode);

	if (strcmp(serverCode, "OK") == 0)
	{
		char *aux = strtok(NULL, " ");
		
		printf("File found in the following peers: \n");

		int count = 1;
		while (aux != NULL && strlen(aux)>1)
		{
			printf("\t[%i] - %s\n", count, aux);
			aux = strtok(NULL, " ");
			count++;
		}

		printf("Select the peer to download the file from [1 - %i]: ", count - 1);

		/* TESTING MODE --> SELECTING FIRST OPTION AUTOMATICALLY */
		if (testingMode == 1)
		{
			fixedOption = 1; //Override peer choice
		}
		else
		{
			if(fgets (option, 10, stdin) == NULL ) 
	   		{
				printf("ERROR: WRONG PEER ID");	
				return -1;
			}

			fixedOption = atoi(option);
			if (fixedOption < 1 || fixedOption > count -1)
			{
				printf("Wrong option\n");
				return -1;
			}
		}
		char *peerTuple;

		strtok(peerList, " "); // Getting rid of the "OK" response

		for (i = 1; i < fixedOption; i++)
		{
			strtok(NULL, " ");
		}		

		peerTuple = strtok(NULL, " ");
		printf("You have selected option %i, which is peer %s\n", fixedOption, peerTuple);
		strtok(peerTuple,":");
		char* connectingPeer = strtok(NULL,":");
		
		printf ("Connecting to port %i, requesting file %s...\n",atoi(connectingPeer), fileName);
		if (obtainFile(atoi(connectingPeer), fileName) < 0)
		{
			printf("Error downloading file\n");
			return -1;
		}
	}

	else
	{
		printf("*Server returned error when searching for file %s\n", fileName);
		return -1;
	}

	return 0;
}


int registerFile (char* fileName)
{
    int sock;
    struct sockaddr_in server;
    char serverReply[2000];
    char request[80];
    char port[10];


    snprintf(port, sizeof( peerPort ) + 1, "%i", peerPort);

    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        printf("Could not create socket");
    }
    //puts("Socket created");
     
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( SERVER_PORT );
 
    //Connect to server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("Connection to server failed");
        return 1;
    }
     
    printf("Connected to server\n");
    
	strcpy(request, "REGISTRY");
	strcat(request, " ");
	strcat(request, fileName);
	strcat(request, " ");
	strcat(request, peerID);
	strcat(request, ":");
	strcat(request, port);

    	printf("\nThe request that is going to be sent is %s\n", request);
    	write(sock, request, strlen(request));
	read (sock , serverReply , sizeof(serverReply));
	printf ("Server reply is: %s\n", serverReply);
	return 0;
}

int checkOption (char* optionAux)
{
	int option = optionAux[0];
	int i;
	char fileName[60];
	char* fixedFileName;

	switch(option)
	{
	   case '1' :
	
		globalOpt = 2;
		showPromptMessage(globalOpt);

		//printf("Register a file\n" );
		//globalOpt = 2;
		//printf("Type in the file you want to register: "); //*/ OPTION 2

		//Ask the user for the file to search
		if(fgets (fileName, 60, stdin) == NULL ) 
		{
			printf("ERROR: WRONG FILE NAME");	
			return -1;
		}

		/*In order to get rid of that annoying '\n' escape char*/
		fixedFileName = strtok(fileName, "\n"); 
	
		/* Testing mode */
		if (testingMode == 1)
		{
			for (i = 0; i < seqRuns; i++)
			{
				printf("\n\nVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV\n");
				printf("[Execution number %i]:",i+1);
				if (registerFile(fixedFileName) == 0)
				{
					printf ("File %s registered successfully\n", fileName);
				}
				else
				{
					printf("ERROR REGISTERING FILE\n");
				}
			}
		}
		else
		{
			if (registerFile(fixedFileName) == 0)
			{
				printf ("File %s registered successfully\n", fileName);
			}
			else
			{
				printf("ERROR REGISTERING FILE\n");
			}
		}
	      	break;

	   case '2' :

		globalOpt = 3;
		showPromptMessage(globalOpt);

		//printf("Search for a file\n");
		//globalOpt = 3;
		//printf("Type in the file you want to search: "); //*/ OPTION 3

		//Ask the user for the file to search
		if(fgets (fileName, 60, stdin) == NULL ) 
		{
			printf("*Error: Wrong file name");	
			return -1;
		}

		/*Next line is to get rid of that annoying '\n' escape char*/
		fixedFileName = strtok(fileName, "\n"); 

		if (testingMode == 1)
		{
			for (i = 0; i < seqRuns; i++)
			{
				printf("\n\nVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV\n");
				printf("[Execution number %i]:",i+1);
				if (searchFile(fixedFileName) == 0)
				{
					printf("Sucess in search call\n");
				}
				else
				{
					printf("*Error when searching for the file\n");
				}
			}
		}
		else
		{
			if (searchFile(fixedFileName) == 0)
			{
				printf("Sucess in search call\n");
			}
			else
			{
				printf("*Error when searching for the file\n");
			}
		}
		break;

	   default :
	      printf("Invalid option\n" );
	}
 
   return 0;	
}



int sendFile (int sock, char* fileName)
{
    char origin[200];
    strcpy(origin, "./shared_folder/");
    strcat(origin, fileName);

    char buffer[256];
    
    FILE *fp;
    fp=fopen(origin, "r");
    int bytes_read = -1;
    if (fp != NULL)
    {
	printf ("File %s opened\n", fileName);
	while ( (bytes_read = fread (buffer, 1, sizeof (buffer), fp)) > 0)
	{
		write(sock,buffer, bytes_read);
	}

	fclose (fp);	
	close (sock);
	return 0;
    }
    else 
	{
		perror ("Error opening file");
		close (sock);
		return -1;
	}
}


void showPromptMessage (int option)
{
	/* This function triggers when the thread shows info about incoming connections into the terminal
	 * By doing this the user will have prompt message visible again instead of getting lost because of thread asynchronized messages
	 */

	switch(option)
	{
		case 1 :
			printf ("\nThis is peer %s. Available options:\n",peerID);
			printf ("\t[1] - Register a file to the index server\n");
			printf ("\t[2] - Obtain a file from another peer\n");
			printf (">>Select option number below\n");
		break;

		case 2 :
			printf ("\nOption 2: Register a file.\n");
			printf (">>Type below the file you want to register\n");
		break;

		case 3 :
			printf("\nOption 3: Search for a file\n");
			printf(">>Type below the file you want to search\n"); 
		break;

		default :
			printf("\n*Error: Bad prompt code\n"); 
	}
}

/* Obtains a file from a peer (Peer port is being used at this stage)*/
int obtainFile (int port, char* fileName)
{
    int sock;
    struct sockaddr_in server;
   
    char request[80];  
    char destination[200];
    strcpy(destination, "./shared_folder/");
    strcat(destination, fileName);


    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        printf("Could not create socket");
	return -1;
    }
    //puts("Socket created");
     
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( port  );
 
    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("connect failed. Error");
        return -1;
    }
     
    printf("Connected to port %i\n", port);

	strcpy(request, "OBTAIN");
	strcat(request, " ");
	strcat(request, fileName);
	strcat(request, " ");

    	printf("\n Requesting file %s to peer in port %i\n", fileName, port);
    	write(sock, request, strlen(request));
	
    	char buffer[256];
	char bufferAux[256];
	int bytes_read = -1;
	bytes_read = read (sock , buffer , sizeof(buffer));
	if (bytes_read == 0)
	{
		printf("Error. The file coudln't be recieved\n");
		close(sock);
		return -1;
	}

	strncpy(bufferAux,buffer,bytes_read - 1);

	FILE *fp;
	fp=fopen(destination, "w");

    if (fp != NULL)
    {
	printf ("File %s opened\n", fileName);
	
	while( (bytes_read) > 0 )
	{
		fwrite ( buffer, 1, bytes_read, fp);
		bytes_read = read (sock , buffer , sizeof(buffer));
	}
	
	printf("File %s sucessfully downloaded\n", fileName); 
	printf("VVVV FILE CONTENT (FIRST 256 BYTES) VVVV\n");
	printf("%s\n\n", bufferAux);
	fclose (fp);
    }
    else 
	{
		perror ("Error opening file");
		close(sock);
		return -1;
	}
     
    	close(sock);
	return 0;
}



int main(int argc , char *argv[])
{

    	char option[10];
	globalOpt = 1;
	clock_t begin, end;
	double time_spent;

    	/* INITIALIZATION */

	if (argc < 3 || argc == 4 || argc > 5)
	{
		printf("Usage: %s PEER_ID PORT [-t SEQUENTIAL_RUNS]\n", argv[0]);
		return 0;
	}

	peerID = argv[1];
	peerPort = atoi(argv[2]);
    

	if (argc == 5 && (strcmp(argv[3], "-t") == 0))
	{
		testingMode = 1;
		seqRuns = atoi(argv[4]);
		printf ("\n\t************************\n");
		printf ("\t***** TESTING MODE *****\n");
		printf ("\t************************\n\n");
	}

	pthread_t incoming_connections_thread;
		 
	if( pthread_create( &incoming_connections_thread , NULL ,  incoming_connections_handler , NULL ) < 0)
	{
		perror("Could not create thread");
		return 1;
	}
         
	//Now join the thread, so that we dont terminate before the thread
	//pthread_join( incoming_connections_thread , NULL);
	//puts("Handler assigned");
    

   	while(1)
    	{
		globalOpt = 1;
		showPromptMessage(globalOpt);

		/*printf ("Peer initialized. Available options:\n");
		printf("\t[1] - Register a file to the index server\n");
		printf("\t[2] - Search for a file\n");
	
		printf ("\n**SELECT OPTION NUMBER:");*/

		fgets(option, sizeof(option), stdin);

		if (testingMode == 1)
		{
			begin = clock();
			checkOption(option);
			end = clock();
			time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

			printf("\n************************************\n");
			printf("******** PERFORMANCE STATS *********\n");
			printf("************************************\n\n");

			printf("+Number of executions: %i\n", seqRuns);
			printf("+Total time: %f seconds\n", time_spent);
			printf("\n************************************\n\n");
			sleep(2); // Just to show the results before showing the prompt again

		}
		else
		{
			checkOption(option);
		}
    	}   

    	return 0;
}
