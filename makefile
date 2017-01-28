OBJS = terminal.o main.o client.o server.o common.o
CC = clang++ 
DEBUG = -g
CFLAGS = -Wall -c $(DEBUG) -std=c++11
LFLAGS = -Wall $(DEBUG) -pthread

chat : $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o chat 

client.o: client.cpp client.h
	$(CC) $(CFLAGS) -c client.cpp

server.o: server.cpp server.h
	$(CC) $(CFLAGS) -c server.cpp

terminal.o: terminal.cpp terminal.h client.h server.h common.h
	$(CC) $(CFLAGS) -c terminal.cpp

main.o: terminal.h main.cpp
	$(CC) $(CFLAGS) -c main.cpp

clean:
	    \rm *.o chat 
tar:
	    tar cfv chat.tar *.h *.cpp *.o chat 
