#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h>  
#include <stdlib.h>



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
	pid_t pid =fork();

	if(pid <0 ){
		perror("Can't create process"); exit(EXIT_FAILURE);
	}
	/*child*/
	else if(pid == 0){
		receive_commands_result(receivePort);
	}
	/*parent*/
	else {
		send_commands(serverName,serverPort,filename);
	}

}

void send_commands(char* serverName,int serverPort,char* filename){
	int sent_commands=0;

	FILE* fp;
    char * command = NULL;
    size_t len = 0;
    ssize_t read;

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

    fp = fopen(filename, "r");
	while(1){

    	//connected
		if ((read = getline(&command, &len, fp)) == -1){
			/*end of file*/
			fclose(fp);
			break;

		}
		write(sockfd, command, 1024); 
		sent_commands++;
		if(sent_commands == 9){
			sent_commands = 0;
			sleep(5);
		}

	}
}
void receive_commands_result(int receivePort){
	int sockfd; 

    char buffer[512];// receive up to 512 bytes

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
	while(1){
		n = recvfrom(sockfd,buffer, 512,MSG_WAITALL, ( struct sockaddr *) &cliaddr,(socklen_t*)&len);
		if(n<0)
			break;
		buffer[n] = '\0'; 
	 	printf("%s\n", buffer);
	}

}
