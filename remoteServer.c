#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h> /* for sockaddr_in */
#include <arpa/inet.h> /* for hton * */
#include <signal.h> 
#include <sys/wait.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO


#define max_clients 20 
#define PACKET_SIZE 512
#define UPD_CMD_SIZE PACKET_SIZE - 2*sizeof(int)

//Functions...

void create_TCP_SOCKET(int* server_fd_ret,struct sockaddr_in* server_addr_ret);
void server_function(int msg_size);
void child_function(int this,int msg_size);
void signal_handler(int signum);
void trim(char * str);
char** create_commnads_array(char* command,int length,int* commands_number);
int count_occurances(char* string,int length,char ch);
int valid_command(char* command);
FILE* execute(char* command);
char* create_udp_packet(char* command_result,int last,int command_code);
void close_reading_sockets();

//...

//gloabl variables

int portNumber,numChildren;


typedef struct{
	char cmd[100];
	int port;
	int command_code;	
	
}server_worker_msg;

typedef struct{
	int command_num;
	int last;
	char command_result[UPD_CMD_SIZE];
}udp_msg;

typedef struct {
  int socket;
  int command_code;
} connectlist_node;

/* Array of connected sockets so we know who we are talking to */
connectlist_node connection_list[max_clients];


int pipe_fds[2];
char* server_commands[7]={
	"ls",
	"cat",
	"cut",
	"grep",
	"tr",
	"end",
	"timeToStop"

};
//...

int main(int argc,char *argv[]){

	

	//first input => ./remoteServer
	if(argc!=3){ perror("Invalid number of input"); exit(EXIT_FAILURE); }

	portNumber = atoi(argv[1]);
	numChildren = atoi(argv[2]);

	if(numChildren < 1){ perror("At least one child is required"); exit(EXIT_FAILURE); }

	//signals handlers....
	signal(SIGPIPE,signal_handler);
	signal(SIGUSR1,signal_handler);//command end
	signal(SIGUSR2,signal_handler);//command timeToStop
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

    int receivePort=-1;

	fd_set socks;        /* Socket file descriptors we want to wake up for, using select() */
	int highsock;	     /* Highest  file descriptor, needed for select() */

    int readsocks;

    int valread;
	//initialize all fds to 0
    for (int i = 0; i < max_clients; i++){
    	connection_list[i].socket = 0;
    	connection_list[i].command_code = -1;
    }

    
  	/*main server loop runs forever*/
	while(1){
		//clear the set
		FD_ZERO(&socks);

		//add the server_fd to accept new connections
		FD_SET(server_fd,&socks);
		highsock = server_fd;

		/* Loops through all the possible connections and adds those sockets to the fd_set */

		for (int i = 0; i < max_clients; i++){

			if(connection_list[i].socket!=0)
				FD_SET(connection_list[i].socket,&socks);
			if(connection_list[i].socket > highsock)
				highsock = connection_list[i].socket;
		}

		/* select() returns the number of sockets that are readable */

		readsocks = select(highsock+1, &socks,NULL,NULL,NULL);

		if(readsocks < 0){
			perror("select"); exit(EXIT_FAILURE);
		}


		/*read from sockets*/

		/* If a client is trying to connect() to our listening
		socket, select() will consider that as the socket
		being 'readable'. Thus, if the listening socket is
		part of the fd_set, we need to accept a new connection. */

		if (FD_ISSET(server_fd,&socks)){
			if ((new_socket = accept(server_fd, (struct sockaddr *)&client_address,(socklen_t*)&addrlen))<0) { 
				perror("accept"); exit(EXIT_FAILURE);
			}

			for (int i = 0; i < max_clients; i++){
			 	if(connection_list[i].socket == 0 ){
			 		connection_list[i].socket = new_socket;
			 		break;
			 	}
		 	} 
		}

		/* Now check connectlist for available data */

		for (int i = 0; i < max_clients; i++){

			if (FD_ISSET(connection_list[i].socket,&socks)){
				valread=read( connection_list[i].socket , buffer, 1024);
				if (valread <= 0){
					close(connection_list[i].socket);
					connection_list[i].socket = 0;
					connection_list[i].command_code = -1;
					continue;
				}

			 	//get the receivePort from client
    			char *ptr = strtok(buffer, ":");
				if(strcmp(buffer,"receivePort")==0){
					ptr = strtok(NULL, ":");
					receivePort=atoi(ptr);
					continue;
				}
				if(strlen(buffer) > 100)
					continue;
	
				connection_list[i].command_code ++;

				//create the message
				server_worker_msg msg;

				//copy the command
				strcpy(msg.cmd,buffer);

				msg.port = receivePort;

				msg.command_code = connection_list[i].command_code;
				//declare character buffer (byte array)
				char buffer_msg[msg_size];
	 	
	 			//serialize the struct
  				memcpy(buffer_msg,&msg,msg_size);

  				//write in pipe the serialized struct
				write(pipe_fds[1],buffer_msg,msg_size);

			}
		}
	}
}
	

void signal_handler(int signum){ 
	switch(signum){
		case SIGPIPE:
			break; //do nothing
		case SIGUSR1:
			close_reading_sockets();
			break;
		case SIGUSR2:
			break;
	}
}

void child_function(int this,int msg_size){
	
	close(pipe_fds[1]); // child doesn't write...
	char task[msg_size]; 
	int valdread,length;
	int working = 0;

	char invalid_command[8] = "";
	//socket to send the command's result
	int sockfd;
			  	
  	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {

  		perror("socket creation failed"); exit(EXIT_FAILURE); 
  	}

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

		if(strcmp(buffer,"end")==0){
			kill(getppid(),SIGUSR1);
			working=0;
			continue;
		}
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
		//address to send the command's result
		struct sockaddr_in servaddr;

	  	memset(&servaddr, 0, sizeof(servaddr)); 
      
	    // Filling server information 
	    servaddr.sin_family = AF_INET; 
	   	servaddr.sin_addr.s_addr = INADDR_ANY;
		servaddr.sin_port = htons(msg.port);
		//...
		char* udp_buf;

		//invalid name 
		if(!valid){
			udp_buf = create_udp_packet(invalid_command,1,msg.command_code);

			sendto(sockfd, udp_buf, PACKET_SIZE , MSG_CONFIRM, 
						(struct sockaddr *) &servaddr,sizeof(servaddr));
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
				udp_buf = create_udp_packet(invalid_command,1,msg.command_code);

				sendto(sockfd, udp_buf, PACKET_SIZE , MSG_CONFIRM, 
						(struct sockaddr *) &servaddr,sizeof(servaddr));

			}
			//command is executed
			else{

				char command_result[UPD_CMD_SIZE] = {0};
				int c;
				int n=0;
				

				while (1){	
    				//last packet
    				if((c = fgetc(fp)) == EOF){
    					command_result[n+1] = '\0';

    					//send the sruct
    					udp_buf = create_udp_packet(command_result,1,msg.command_code);

    					sendto(sockfd, udp_buf, PACKET_SIZE , MSG_CONFIRM, 
								(struct sockaddr *) &servaddr,sizeof(servaddr));
  						break;
    				}
        			command_result[n++] = (char) c;

        			if(n == UPD_CMD_SIZE-1 ){
        				command_result[n+1] ='\0';

        				//send the struct
        				udp_buf = create_udp_packet(command_result,0,msg.command_code);

        				sendto(sockfd, udp_buf, PACKET_SIZE, MSG_CONFIRM, 
  								(struct sockaddr *) &servaddr,sizeof(servaddr));

        				n = 0;
  						for (int i = 0; i < 512; ++i){
  							command_result[i] = 0;
  						}
        			}
    			}
    			
				
			  	/* close */
			 	pclose(fp);

			}

		}
		/*task finished!*/
		working= 0;

	}
	
	
}


char* create_udp_packet(char* command_result,int last,int command_code){
	udp_msg msg;

	strcpy(msg.command_result,command_result);
	msg.last = last;
	msg.command_num = command_code;

	//declare character buffer (byte array)
	static char buffer_msg[PACKET_SIZE];
	 	
	//serialize the struct
	memcpy(buffer_msg,&msg,PACKET_SIZE);

	return buffer_msg;
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

void close_reading_sockets(){
	for(int i = 0; i< max_clients ;i++){
		if(connection_list[i].socket != -1){
			close(connection_list[i].socket);
			connection_list[i].socket = -1;
			connection_list[i].command_code = -1;
		}
	}
}

/*function to get size of the file.*/

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


