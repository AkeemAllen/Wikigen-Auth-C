CC = gcc
CFLAGS = -Wall -Wextra

all: wikigen_auth

wikigen_auth: token_util.o router.o cJSON.o request_parser.o request.o wikigen_auth.c
	$(CC) token_util.o router.o cJSON.o request_parser.o request.o wikigen_auth.c -o wikigen_auth -lcurl -ljwt -g -gdwarf-4

cJSON.o: cJSON.c cJSON.h
	$(CC) $(CFLAGS) -c cJSON.c -o cJSON.o

parser.o: request_parser.c request_parser.h
	$(CC) $(CFLAGS) -c request_parser.c -o request_parser.o

request.o: request.c request.h
	$(CC) $(CFLAGS) -c request.c -o request.o

router.o: router.c router.h
	$(CC) $(CFLAGS) -c router.c -o router.o

token_util.o: token_util.c token_util.h
	$(CC) $(CFLAGS) -c token_util.c -o token_util.o

clean:
	rm -f *.o wikigen_auth
