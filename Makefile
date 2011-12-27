CC=g++
CFLAGS= -g -Wall -Werror

all: proxy

proxy: proxy.c
	$(CC) $(CFLAGS) -o proxy.o -c proxy.c
	$(CC) $(CFLAGS) -o proxy proxy.o

clean:
	rm -f proxy *.o

tar:
	tar -cvzf cos461_ass2_$(USER).tgz proxy.c README Makefile
