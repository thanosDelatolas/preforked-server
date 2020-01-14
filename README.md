# preforked-server
remoteServer can accept multiple `tcp clients` using `select` function.  
remoteClient reads linux commands from a file and sends them to server.  
Commands are sent using `tcp`  
Commands results are received using `udp`
  
@Brief remoteServer   
remoteServer forks `numChildren` children processes which will execute commands. Parent reads the commands from clients and writes the commands to a `pipe`. All children processes are reading from this `pipe`.  
Children processes execute the command and send the result in `datagram packets` to the remoteClient who sent the command to the server. 
  
@Brief remoteClient  
remoteClient forks a child process. Child process sends the commands to the remoteServer.  
Parent process waits the `datagram packets` and create a file for each command result.  
  
To compile just type make  
./remoteServer 8080 10  
./remoteClient server 8080 4000 commands_file  
make clean  

