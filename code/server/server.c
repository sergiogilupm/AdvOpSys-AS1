/*
* File: server.c
* Author: Sergio Gil Luque
* Version: 1.0
* Date: 09-21-15
*/

#include<stdio.h>
#include<string.h>   
#include<sys/socket.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<stdlib.h>
#include <unistd.h>

#include "uthash.h" // For Hashtable implementation

#define PORT 5000


/* Hash table struct */
/* This is a double level hash table. First level -> Files ; Second level -> Peers that contain that file */
typedef struct fileStruct {
  char name[50];
  struct fileStruct *sub;
  int val;
  UT_hash_handle hh;
} fileStruct_t;


fileStruct_t *registeredFiles = NULL;


/* Look for file in the hashtable */
struct fileStruct *find_file(char* fileName) {

    struct fileStruct *s;

    HASH_FIND_STR (registeredFiles, fileName, s );  /* s: output pointer */
    return s;
}

/*Look for the peer provided inside a file*/
struct fileStruct *findPeerInFile(char* peerName, char* fileName) {

    	struct fileStruct *s;
    	struct fileStruct *i;

    	HASH_FIND_STR (registeredFiles, fileName, s );  /* s: pointer to file struct */

	for(i=s->sub; i != NULL; i=(struct fileStruct*)(i->hh.next)) 
	{
		if (strcmp(i->name, peerName)==0)
		{
			printf("Peer %s already registered file %s. Skipping...\n", peerName, fileName);
			return i;
		}
	}

    	return NULL;
}


/* Look if a file is registered */
char* searchFile (char *fileName)
{
	struct fileStruct *s;
	struct fileStruct *i;
	char *reply = malloc(2000 * sizeof(char));
	char valStr[10];
	s = find_file(fileName);

	if (s == NULL)
	{
		printf ("ERROR: FILE NOT FOUND");
		strncpy(reply, "ERROR", 5);
	}

	else
	{
		strncpy(reply, "OK", 2);
		for(i=s->sub; i != NULL; i=(struct fileStruct*)(i->hh.next)) 
		{
			snprintf(valStr, sizeof(i->val) + 1, "%i", i->val);
			strncat(reply, " ", 1);
			strncat(reply, i->name, sizeof(i->name));
			strncat(reply, ":", 1);
			strncat(reply, valStr, sizeof(valStr) + 1);
			strncat(reply, " ", 1);
		}
		return reply;
	}	
	return reply;
}

/* Only for debugging purposes */
void print_users() 
{
	struct fileStruct *s;
	struct fileStruct *i;

	for(s=registeredFiles; s != NULL; s=(struct fileStruct*)(s->hh.next)) 
	{
		for(i=s->sub; i != NULL; i=(struct fileStruct*)(i->hh.next)) 
		{
	        	printf("file %s: peer %s, and port %i\n", s->name, i->name, i->val);
		}
	}
}


/* Add a file into the hashtable. If the file already exists, it will only add the peer in the second level hashtable */
int addFile (char* newFileName, char *newPeer, int peerPort) {

   	// Find file in hashTable. If it doesnt exist, create new entry and go deeper. If exists, just go deeper
    	struct fileStruct *aux;
	struct fileStruct *aux2;

    	aux = find_file(newFileName); //Checking if the file already exists in the hashtable
	if (aux == NULL)
	{
		/* Adding file in first level of the hash table */
		aux = malloc(sizeof(struct fileStruct));
    		strcpy(aux->name, newFileName);
    		aux->sub = NULL;
		aux->val = 0;
    		HASH_ADD_STR( registeredFiles, name, aux );

		/* Adding peer in second level of hashtable */
		fileStruct_t *s = malloc(sizeof(*s));
		strcpy(s->name, newPeer);
		s->sub = NULL;
		s->val = peerPort;
		HASH_ADD_STR(aux->sub, name, s);
	}
	else
	{
		aux2 = findPeerInFile(newPeer, newFileName); //Checking if the peer has already registered that file before
		if (aux2 == NULL)
		{
			fileStruct_t *s = malloc(sizeof(*s));
			strcpy(s->name, newPeer);
			s->sub = NULL;
			s->val = peerPort;
			HASH_ADD_STR(aux->sub, name, s);
		}
	}

	return 0;
}



void *connection_handler(void *socket_desc)
{
    char request_received[2000];
    char *serverReply;
    char *message;
    int sock = *(int*)socket_desc;
     
    read (sock, request_received , 255);
    printf ("A request has been received by the server: %s\n", request_received);

	message = strtok(request_received ," ");

	// mesage can be REGISTRY or SEARCH request

	if (strcmp(message,"REGISTRY")==0) /* Check for errors and exceptions */
	{	
		printf("Registry request\n");
		char *fileName = strtok(NULL, " ");
		char *peerAndPortCouple = strtok(NULL, " ");
		char *peerID = strtok (peerAndPortCouple, ":");
		int port = atoi (strtok (NULL, ":"));
		if ( addFile(fileName, peerID, port) == 0) 
		{
			printf ("SUCCESS\n");
			//print_users(); //For debug purposes
			serverReply = "OK";
			write(sock, serverReply, sizeof(serverReply));
		}
		else
		{
			printf ("*Error: The file was not registered\n");
			serverReply = "ERROR FILE_NOT_REGISTERED";
			write(sock, serverReply, sizeof(serverReply));
		}
     		
	} 

	else if (strcmp(message,"SEARCH")==0)   
	{
		printf("Search request\n");
		char *fileName = strtok(NULL, " ");
		serverReply = searchFile(fileName);
		if ( strcmp (serverReply, "ERROR") != 0) 
		{
			printf ("SUCCESS\n");
			// printf ("Search call returned: %s\n", serverReply); //For debug purposes
			write(sock, serverReply, strlen(serverReply));
		}
		else
		{
			printf ("*Error: File %s was not found\n", fileName);
			serverReply = "ERROR FILE_NOT_FOUND";
			write(sock, serverReply, sizeof(serverReply));
		}

	}

	else
	{
			printf ("*Error: Bad request\n");
			serverReply = "ERROR BAD_REQUEST";
			write(sock, serverReply, sizeof(serverReply));
	}

    	//Free the socket pointer
    	free(socket_desc);
     
    	return (void *) 0;
}


void *incoming_connections_handler (void* data)
{
     
    printf ("Initializing incoming connection handler...\n");
     
    int IN_socket_desc , IN_new_socket , c , *IN_new_sock;
    struct sockaddr_in IN_server , client;
     
    //Create socket
    IN_socket_desc = socket(AF_INET , SOCK_STREAM , 0);

    if (IN_socket_desc == -1)
    {
        perror("Could not create socket");
    }
     
    //Prepare the sockaddr_in structure
    IN_server.sin_family = AF_INET;
    IN_server.sin_addr.s_addr = INADDR_ANY;
    IN_server.sin_port = htons( PORT );
     
    //Bind
    if( bind(IN_socket_desc,(struct sockaddr *) &IN_server , sizeof(IN_server)) < 0)
    {
        perror("Bind failed");
        return (void *) 1;
    }
     
    //Listen
    listen(IN_socket_desc , 1000); // The server can handle 1000 simulteaneous connections
     
    //Accept and incoming connection
    printf("Waiting for incoming connections in port %i...\n", PORT);
    c = sizeof(struct sockaddr_in);

    while( (IN_new_socket = accept(IN_socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        puts("New connection accepted");
         
        pthread_t sniffer_thread;
        IN_new_sock = malloc(1);
        *IN_new_sock = IN_new_socket;
         
        if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) IN_new_sock) < 0)
        {
            perror("Could not create thread");
            return (void *) 1;
        }
    }
     
    if (IN_new_socket<0)
    {
        perror("accept failed");
        return (void *) 1;
    }
     
    return (void *) 0;
}



int main(int argc , char *argv[])
{
	/* INITIALIZATION */

	printf ("Initializing server...\n");
	printf ("Listening port is %i\n", PORT);
	pthread_t incoming_connections_thread;
		 
	if( pthread_create( &incoming_connections_thread , NULL ,  incoming_connections_handler , NULL ) < 0)
	{
		perror("Could not create thread");
		return 1;
	}
	    
	printf("Server ready\n");

	//Now join the thread, so that we dont terminate before the thread
	pthread_join( incoming_connections_thread , NULL);

	return 0;
}



