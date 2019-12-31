// Client side C/C++ program to demonstrate Socket programming 
#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h>  
#include <stdlib.h>
#include <string.h>

void send_commands(char* serverName,int serverPort,char* filename);
void receive_commands_result(int receivePort);

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
	int childpid;

	if((childpid=fork()<0)){
		perror("fork call");
		exit(EXIT_FAILURE);
	}
	//parent
	if(childpid>0){

		send_commands(serverName,serverPort,filename);

	}
	//child 
	else {

		receive_commands_result(receivePort);
	}
}

void send_commands(char* serverName,int serverPort,char* filename){
	int sent_commands=0;
	FILE* fp;
    char * command = NULL;
    size_t len = 0;
    ssize_t read;

    int sockfd;
 	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket call"); exit(EXIT_FAILURE);
   	}

    fp = fopen(filename, "r");
	while(1){

		/*end of file*/
		if ((read = getline(&command, &len, fp)) == -1){
			fclose(fp);
			break;

		}

		if(sent_commands == 9){
			sent_commands = 0;
			sleep(5);
		}

	}
}
void receive_commands_result(int receivePort){

	while(1){
		break;
	}

}
