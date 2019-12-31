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
//...

//global variables...


pid_t parent;
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

typedef struct server_worker_message{
	char cmd[1024];
	struct sockaddr_in receiver_addr;
	
}server_worker_msg;
//...