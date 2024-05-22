#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>

#define BIND_IP_ADDR "127.0.0.1"
#define BIND_PORT 8000
#define MAX_RECV_LEN 1048576
#define MAX_SEND_LEN 1048576
#define MAX_PATH_LEN 1024
#define MAX_HOST_LEN 1024
#define MAX_CONN 20

#define MAX_CONN 20
#define MAX_THREAD 200
#define MAX_QUEUE_SIZE 1024

#define HTTP_STATUS_200 "200 OK"

#define ERR_INVALID_METHOD -2
#define ERR_NOT_FOUND -3

int parse_request(int client_socket, ssize_t *req_len, char *req, struct stat *file_type);

void handle_clnt(int clnt_sock);