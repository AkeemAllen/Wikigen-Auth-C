#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define PORT 8080

int main(int argc, char const *argv[]) {
  int status, valread, client_fd;
  struct sockaddr_in serv_addr;
  // char* hello = "GET /authorize?code=1234 HTTP/1.1";
  const char *hello = "POST /create_jwt HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: 45\r\n"
                      "User-Agent: Mozilla/5.0\r\n"
                      "Accept: application/json\r\n"
                      "\r\n"
                      "{\"username\":\"john\",\"email\":\"john@example.com\"}";

  char buffer[1024] = {0};
  if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("\n Socket creation error \n");
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  // Convert IPv4 and IPv6 addresses from text to binary
  // form
  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    printf("\nInvalid address/ Address not supported \n");
    return -1;
  }

  if ((status = connect(client_fd, (struct sockaddr *)&serv_addr,
                        sizeof(serv_addr))) < 0) {
    printf("\nConnection Failed \n");
    return -1;
  }

  // subtract 1 for the null
  // terminator at the end
  send(client_fd, hello, strlen(hello), 0);
  printf("Hello message sent\n");
  valread = read(client_fd, buffer, 1024 - 1);
  printf("%s\n", buffer);

  // closing the connected socket
  close(client_fd);
  return 0;
}
