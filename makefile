CC = gcc
CFLAGS = -Wall -Wextra

all: wikigen_auth

wikigen_auth: cJSON.o request_parser.o request.o wikigen_auth.c
	$(CC) cJSON.o request_parser.o request.o wikigen_auth.c -o wikigen_auth -lcurl -g -gdwarf-4

cJSON.o: cJSON.c cJSON.h
	$(CC) $(CFLAGS) -c cJSON.c -o cJSON.o

parser.o: request_parser.c request_parser.h
	$(CC) $(CFLAGS) -c request_parser.c -o request_parser.o

request.o: request.c request.h
	$(CC) $(CFLAGS) -c request.c -o request.o

clean:
	rm -f *.o wikigen_auth
