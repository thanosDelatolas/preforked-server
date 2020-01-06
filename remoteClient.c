#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h>  
#include <stdlib.h>

#define PACKET_SIZE 512
#define UPD_CMD_SIZE PACKET_SIZE - 2*sizeof(int)


typedef struct 
{
	char command_name[100];
	int answered; //for 2 or more same commands
}command_struct;

typedef struct{
	int command_num;
	int last;
	char command_result[UPD_CMD_SIZE];
}udp_msg;



void send_commands(char* serverName,int serverPort,command_struct** commands_array, int num_of_commands);
void receive_commands_result(int receivePort,command_struct** commands_array, int num_of_commands);
void send_receive_port(int receivePort, int serverPort);

command_struct** create_commands_array(char* filename,int* n);



char ** commands = NULL;

int main(int argc, char *argv[]) 
{ 	
	//first input => ./remoteClient
	if (argc != 5) { perror("Invalid number of input"); exit(EXIT_FAILURE); }

	char* serverName = argv[1];
	int serverPort =  atoi(argv[2]);
	int receivePort = atoi(argv[3]);
	char* filename = argv[4];

	if( access( filename, R_OK ) == -1 ){
		printf("Can't find file %s\n",filename );
		exit(EXIT_FAILURE);
	}
	
	int num_of_commands=-1;

	command_struct** commands_array=create_commands_array(filename,&num_of_commands);

	send_receive_port(receivePort,serverPort);

	pid_t pid =fork();

	if(pid <0 ){
		perror("Can't create process"); exit(EXIT_FAILURE);
	}
	/*child*/
	else if(pid == 0){
		send_commands(serverName,serverPort,commands_array , num_of_commands);
	}
	/*parent*/
	else {
		receive_commands_result(receivePort,commands_array,num_of_commands);
	}

	exit(EXIT_SUCCESS);

}
void send_receive_port(int receivePort, int serverPort){

	struct sockaddr_in servaddr;

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(serverPort); 


    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
   		perror("socket call"); exit(EXIT_FAILURE);
	}

   	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) { 
    	perror("connection with the server failed...\n"); 
    	exit(EXIT_FAILURE); 
	} 

	//send receivePort to the server
	char port_message[1024] = {0};
	snprintf(port_message, 1024, "receivePort:%d", receivePort); 
	write(sockfd, port_message, 1024);
	close(sockfd);

}
void send_commands(char* serverName,int serverPort,command_struct** commands_array, int num_of_commands){
	int sent_commands=0;


    struct sockaddr_in servaddr;

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(serverPort); 


    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
   		perror("socket call"); exit(EXIT_FAILURE);
	}

   	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) { 
    	perror("connection with the server failed...\n"); 
    	exit(EXIT_FAILURE); 
	} 
	

   	int i=0;
	while(i<num_of_commands){

		//connected
		write(sockfd, commands_array[i] -> command_name, 1024); 
		sent_commands++;
		if(sent_commands == 9){
			sent_commands = 0;
			sleep(5);
		}
		i++;

	}
	close(sockfd);
}
void receive_commands_result(int receivePort,command_struct** commands_array, int num_of_commands){
	int sockfd; 

    char buffer[PACKET_SIZE]; // receive up to 512 bytes

    struct sockaddr_in servaddr, cliaddr;

    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); exit(EXIT_FAILURE); 
    } 

    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 

    /* to set the socket options- to reuse the given port multiple times */
	int reuse=1;
	

	if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEPORT,(const char*)&reuse,sizeof(reuse))<0){
		perror("setsockopt(SO_REUSEPORT) failed\n"); exit(EXIT_FAILURE);
	}

    // Filling server information 
    servaddr.sin_family    = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(receivePort); 

    // Bind the socket with the server address 
    if ( bind(sockfd, (struct sockaddr *)&servaddr,sizeof(servaddr)) < 0 ){ 
        perror("bind failed"); exit(EXIT_FAILURE); 
    } 

    int len = sizeof(cliaddr);
    int n;
    int received_commands=0;

    char cmd_res[UPD_CMD_SIZE]={0};

    FILE* fp = NULL;
    char filePath[20] = {0};

    int new_cmd=1;
	while(1){
		n = recvfrom(sockfd,buffer, PACKET_SIZE,MSG_WAITALL, ( struct sockaddr *) &cliaddr,(socklen_t*)&len);
		
		udp_msg msg;
		buffer[n] = '\0';
		//desirialize and create the struct
		memcpy(&msg ,buffer, PACKET_SIZE);

		strcpy(cmd_res,msg.command_result);

		if(new_cmd == 1){
			//create the name of the file
			snprintf(filePath, 20, "output.%d.%d", receivePort,(msg.command_num+1)); 
			fclose(fopen(filePath, "w"));//clear the file if already exists
			fp = fopen(filePath, "a");
			new_cmd = 0;
		}

		if(msg.last == 1) {
			fputs(cmd_res, fp);
			received_commands++;
			fclose(fp);
			new_cmd =1;

		}
		else{
			fputs(cmd_res, fp);
		}

		if(received_commands == num_of_commands){
			break;
		}
	}

	close(sockfd);

}


/*
 create commands array
*/

command_struct** create_commands_array(char* filename,int* n){
	FILE* fp;
    char * command = NULL;
    size_t len = 0;
    ssize_t read;
    command_struct** commands_array = NULL;

    int num_of_commands=0;

    fp = fopen(filename, "r");
    while(1){

    	if ((read = getline(&command, &len, fp)) == -1){
			/*end of file*/
			fclose(fp);
			break;

		}
		if(strlen(command) > 100 ){
			strcpy(command,"invalid");
		}

		commands_array = (command_struct**)realloc (commands_array, sizeof (command_struct*) * (num_of_commands+1));
		commands_array[num_of_commands] = (command_struct*)malloc(sizeof(command_struct));

		commands_array[num_of_commands] -> answered = 0;
		strcpy(commands_array[num_of_commands] -> command_name,command);
		num_of_commands ++;


    }
    *n = num_of_commands;
    return commands_array;
}
