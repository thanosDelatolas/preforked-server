CC = gcc
# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall compiler warnings
CFLAGS  = -g -Wall

# the build target executable:
TARGET_SERVER = remoteServer
TARGET_CLIENT = remoteClient

all: $(TARGET_SERVER) $(TARGET_CLIENT)


$(TARGET_SERVER): $(TARGET_SERVER).c
	$(CC)	$(CFLAGS) -o $(TARGET_SERVER) $(TARGET_SERVER).c

$(TARGET_CLIENT): $(TARGET_CLIENT).c
	$(CC)	$(CFLAGS) -o $(TARGET_CLIENT) $(TARGET_CLIENT).c



	 

clean:
	-rm -f *.o
	-rm -f $(TARGET_SERVER)
	-rm -f $(TARGET_CLIENT)
