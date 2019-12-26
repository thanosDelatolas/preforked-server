//Functions...

void create_TCP_SOCKET(int* server_fd_ret,struct sockaddr_in* server_addr_ret);
void server_function();
void child_function(int this);
void signal_handler(int signum);
void trim(char * str);
char** create_commnads_array(char* command,int length,int* commands_number);
int count_occurances(char* string,int length,char ch);
int valid_command(char* command);
FILE* execute(char* command);
//...

//global variables...


pid_t* workers_array;

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
//...