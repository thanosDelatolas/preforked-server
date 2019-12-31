#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h> /* for sockaddr_in */
#include <arpa/inet.h> /* for hton * */
#include <signal.h> 
#include "remoteServer.h"
#include <sys/wait.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO
#include <errno.h>  

#define max_clients 10
int portNumber,numChildren;
int client_socket[max_clients];

int main(int argc,char *argv[]){

	

	//first input => ./remoteServer
	if(argc!=3){ perror("Invalid number of input"); exit(EXIT_FAILURE); }

	portNumber = atoi(argv[1]);
	numChildren = atoi(argv[2]);

	if(numChildren < 1){ perror("At least one child is required"); exit(EXIT_FAILURE); }

	parent = getpid();

	//signals handlers....
	signal(SIGPIPE,signal_handler);
	//...

    //create pipe...

    if (pipe(pipe_fds) == -1){
    	perror("pipe call"); exit(EXIT_FAILURE);
    }
    

    int msg_size =sizeof(server_worker_msg);
   	pid_t temp;
   
     /* create numChildren child-process*/
    for (int i = 0; i < numChildren; i++){
    	temp = fork();

    	if(temp < 0){
    		perror("Can't create process"); exit(EXIT_FAILURE);
    	}

    	/*child*/
    	else if( temp == 0 ){
    		
			child_function(i,msg_size); //never ends
    	}
    	/*parent*/
    	else{

    	}   
	} 
	//only father is here
	server_function(msg_size);
	
}
/* 
	@brief create_TCP_SOCKET
	return by refernce: server_fd,server_addr
*/
void create_TCP_SOCKET(int* server_fd_ret,struct sockaddr_in* server_addr_ret){
	int server_fd;
	struct sockaddr_in server_addr;

	//create socket...
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) { 
        perror("socket failed");  exit(EXIT_FAILURE); 
    } 


    server_addr.sin_family = AF_INET ;
	server_addr.sin_addr.s_addr = htonl ( INADDR_ANY ); // receive connections from any address
	server_addr.sin_port = htons ( portNumber );

	/* to set the socket options- to reuse the given port multiple times */
	int reuse=1;
	

	if(setsockopt(server_fd,SOL_SOCKET,SO_REUSEPORT,(const char*)&reuse,sizeof(reuse))<0){
		perror("setsockopt(SO_REUSEPORT) failed\n"); exit(EXIT_FAILURE);
	}

	/* bind the socket to known port */

	if(bind(server_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
		perror("Error in binding"); exit(EXIT_FAILURE);
	}

	/* place the socket in passive mode and make the server ready to accept the requests and also 
	   specify the max no. of connections
 	*/

 	if(listen(server_fd,1000)<0){
 		perror("Can't create listener..."); exit(EXIT_FAILURE);
 	}
 	*server_fd_ret = server_fd;
    *server_addr_ret = server_addr;

}

void server_function(int msg_size){
	int new_socket; 
	struct sockaddr_in server_address,client_address;
    int server_fd;
    int addrlen = sizeof(server_address); 
    char buffer[1024];

    /* 
	   call create_TCP_SOCKET to create a socket, bind it and place it in passive mode
       once the call returns call accept on listening socket to accept the incoming requests

    */

    create_TCP_SOCKET(&server_fd, &server_address);
	
	//set of socket descriptors  
    fd_set readfds; 
   

    //maximum socket descriptor
    int max_sd=-1; 
    int activity,sd;
    //initialise all client_socket[] to 0 so not checked  
    for (int i = 0; i < max_clients; i++)   
    {   
        client_socket[i] = 0;   
    } 

	while(1){

		//clear the socket set  
        FD_ZERO(&readfds);

        //add server socket to set  
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;  

        for ( int i = 0 ; i < max_clients ; i++){   
            //socket descriptor  
            sd = client_socket[i];   
                 
            //if valid socket descriptor then add to read list  
            if(sd > 0)   
                FD_SET( sd , &readfds);   
                 
            //highest file descriptor number, need it for the select function  
            if(sd > max_sd)   
                max_sd = sd;   
        }

        //wait for an activity on one of the sockets , timeout is NULL ,  
        //so wait indefinitely  
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);   
       
        if ((activity < 0) && (errno!=EINTR))   
        {   
            perror("select error");  exit(EXIT_FAILURE);  
        }  
        //If something happened on the server socket ,  
        //then its an incoming connection  
        if (FD_ISSET(server_fd, &readfds)){

    		if ((new_socket = accept(server_fd, (struct sockaddr *)&client_address,(socklen_t*)&addrlen))<0) { 
				perror("accept"); exit(EXIT_FAILURE); 
			} 
			recv( new_socket , buffer, 1024,0); 
			if(strlen(buffer)> 100 )
				continue;

			//create the message
			server_worker_msg msg;
			strcpy(msg.cmd,buffer);
			msg.sd = new_socket;
			memcpy(&msg.receiver_addr,&client_address,sizeof(client_address));
		
			//declare character buffer (byte array)
			char buffer_msg[msg_size];
	 	
	 		//serialize the struct
  			memcpy(buffer_msg,&msg,msg_size);
  		
  			//write in pipe the serialized struct
			write(pipe_fds[1],buffer_msg,msg_size);

			//add the new socket
			for (int i = 0; i < max_clients; i++) {
				 if( client_socket[i] == 0 ){
				 	client_socket[i] = new_socket;
				 	break;
				 }
			}
        }

        //remove the closed sockets from the set
        for (int i = 0; i < max_clients; i++) {
        	sd = client_socket[i]; 

        	if (sd!=0 && !FD_ISSET( sd , &readfds)){
        		client_socket[i] = 0; 
        		//printf("hiii\n" );
        	}
        	
        }


	}
    

}

void signal_handler(int signum){ 
	switch(signum){
		case SIGPIPE:
			break; //do nothing
	}
}

void child_function(int this,int msg_size){
	
	close(pipe_fds[1]); // child doesn't write...
	char task[msg_size]; 
	int valdread,length;
	int working = 0;

	while(1){
		
		//wait for a task...
		if(!working){

			valdread=read(pipe_fds[0],task,msg_size);
			if(valdread > 0)
				working = 1;
		}
		//wait to read bytes
		if(!working)
			continue;

		/*task assigned!*/

		task[valdread]='\0';

		//create the struct
		server_worker_msg msg;

		//desirialize and create the struct
		memcpy(&msg ,task, msg_size);

		//buffer has the command
		char buffer [1024]={0};
		strcpy(buffer,msg.cmd);


		trim(buffer);
		length = strlen(buffer);
		
		int commands_number;
		char** pipelined_commands = create_commnads_array(buffer,length,&commands_number);

		
		int i=0;
		int valid=0;
		char command_name[100]="";
		//check if all commands are valid
		for(i=0;i<commands_number;i++){
			strcpy(command_name,pipelined_commands[i]);
			valid = valid_command(command_name);
			if(!valid)
				break;
			i++;

		}
		//invalid name 
		if(!valid){
			printf("Invalid command %s\n", command_name);
		}
		else{
			//concatenate the commands 
			char final_command[100];
			strcpy(final_command,pipelined_commands[0]);
			for(i=1;i<commands_number;i++){
				strcat(final_command , " | ");
				strcat(final_command,pipelined_commands[i]);
			}

			//execute the command 
			FILE* fp = execute(final_command);

			//command can't be executed because of arguments
			if(fp == NULL){

			}
			//command is executed
			else{
				char path[1035];
				/* Read the output a line at a time - output it. */
  				while (fgets(path, sizeof(path), fp) != NULL) {
    				printf("%s\n", path);
  				}

			  /* close */
			  pclose(fp);


			}

		}
		/*task finished!*/
		working= 0;
		//close the served socket and remove it from the array
		close(msg.sd);
	}
	
	
}



/*
	Given a pipelined command which may include ';' returns an array of all pipelined commands.
*/

char** create_commnads_array(char* command,int length,int* commands_number){

	char** pipelined_commands= NULL;
	int num_of_commands = 0;
	// Extract the first token
	char* token = strtok(command, "|"); 

	/* split string and append tokens to pipelined_commands 
	* realloc function is used to resize a block of memory that was previously allocated
	*/

	while (token!=NULL) {

  		pipelined_commands = realloc (pipelined_commands, sizeof (char*) * ++num_of_commands);

  		if (pipelined_commands == NULL)
			exit (-1); /* memory allocation failed */

		pipelined_commands[num_of_commands-1] = token;

		token = strtok (NULL, "|");
	}


	//for each pipelined command get only the first command which ends with ';' and trim it
	for (int i = 0; i < num_of_commands; i++){
		pipelined_commands[i] = strtok(pipelined_commands[i],";");

		trim(pipelined_commands[i]);

	}
	*commands_number = num_of_commands;

	return pipelined_commands;

}
/*
* check if a command is valid
*/
int valid_command(char* command){
	//get the name of the command
	char* command_name = strtok(command, " ");

	for (int i = 0; i < 7; i++){
		if(strcmp(command_name, server_commands[i])==0){
			return 1;
		}
		
	}
	return 0;
}

FILE* execute(char* command){

	FILE *fp;
	/* Open the command for reading. */
  	fp = popen(command, "r");
  	// if fp == NULL command can't be executed 
  	return fp;
    

}
/**
 * Remove leading and trailing white space characters
 */
void trim(char * str)
{
    int index, i;

    /*
     * Trim leading white spaces
     */
    index = 0;
    while(str[index] == ' ' || str[index] == '\t' || str[index] == '\n')
    {
        index++;
    }

    /* Shift all trailing characters to its left */
    i = 0;
    while(str[i + index] != '\0')
    {
        str[i] = str[i + index];
        i++;
    }
    str[i] = '\0'; // Terminate string with NULL


    /*
     * Trim trailing white spaces
     */
    i = 0;
    index = -1;
    while(str[i] != '\0')
    {
        if(str[i] != ' ' && str[i] != '\t' && str[i] != '\n')
        {
            index = i;
        }

        i++;
    }

    /* Mark the next character to last non white space character as NULL */
    str[index + 1] = '\0';
}
