// server.c
#include "server.h"
#include "thread.h"

int parse_request(int client_socket, ssize_t *req_len, char *req, struct stat *file_type)
{
    char *buf = (char *)malloc(MAX_RECV_LEN * sizeof(char));
    ssize_t buf_len = 0;
    char *end_of_header = NULL;

    // 不断尝试读取，直到读取到两个换行符（\r\n\r\n）时才算读取完成
    while ((end_of_header = strstr(buf, "\r\n\r\n")) == NULL) {
        buf_len = read(client_socket, buf + *req_len, MAX_RECV_LEN - *req_len - 1);
        if (buf_len < 0) {
            perror("read");
            free(buf);
            return -1;
        }
        *req_len += buf_len;
        buf[*req_len] = '\0';
    }

    // 将 buf 的内容复制到 req 中
    strncpy(req, buf, *req_len);
    req[*req_len] = '\0';

    // 验证请求的有效性
    if (strncmp(buf, "GET ", 4) != 0) {
        perror("Invalid request");
        free(buf);
        return ERR_INVALID_METHOD;
    }

    // 解析请求的路径
    char *path_start = buf + 4; // 跳过 "GET "
    char *path_end = strchr(path_start, ' ');
    if (path_end == NULL) {
        perror("Invalid request");
        free(buf);
        return -1;
    }
    size_t path_len = path_end - path_start;
    char path[MAX_PATH_LEN];
    path[0] = '.'; // 在路径首位插入一个 '.'
    strncpy(path + 1, path_start, path_len); // 将路径复制到 path 中，从第二个字符开始
    //去掉末尾的"/"
    if (path[path_len] == '/' && path_len > 1) {
        path_len--;
    }
    path[path_len + 1] = '\0'; // 注意现在路径的长度增加了 1

    // 检查路径是否试图访问当前目录之外的文件
    if (strstr(path, "../") != NULL || strstr(path, "..\\") != NULL) {
        perror("Path traversal attempt");
        free(buf);
        return -1;
    }

    // 尝试打开路径指向的文件，并获取文件的状态信息
    int file_fd = open(path, O_RDONLY);
    if (file_fd < 0) {
        perror("open");
        free(buf);
        return ERR_NOT_FOUND;
    }
    if (fstat(file_fd, file_type) < 0) {
        perror("fstat");
        close(file_fd);
        free(buf);
        return -1;
    }

    // 检查请求的资源是否为目录
    if (S_ISDIR(file_type->st_mode)) {
        perror("Requested resource is a directory");
        close(file_fd);
        free(buf);
        return -1;
    }

    free(buf);
    return file_fd;
}

void handle_clnt(int clnt_sock)
{
    // 读取客户端发送来的数据，并解析
    char *req = (char *)malloc(MAX_RECV_LEN * sizeof(char));
    if (req == NULL){
        perror("req_buf malloc failed\n");
        free(req);
        exit(1);
    }
    req[0] = '\0';

    ssize_t req_len = 0;
    struct stat file_type;
    int file_fd = parse_request(clnt_sock, &req_len, req, &file_type);

    if (file_fd == ERR_INVALID_METHOD) {
        // 请求方法无效，发送错误响应
        char* response = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
        write(clnt_sock, response, strlen(response));
    } else if (file_fd == ERR_NOT_FOUND) {
        // 文件未找到，发送错误响应
        char* response = "HTTP/1.0 404 Not Found\r\n\r\n";
        write(clnt_sock, response, strlen(response));
    } else if (file_fd < 0) {
        // 其他错误，发送错误响应
        char* response = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
        write(clnt_sock, response, strlen(response));
    } else {
        // 解析请求成功，读取文件内容并发送
        char* response = (char*) malloc((MAX_SEND_LEN + 1) * sizeof(char)); // +1 for null terminator
        sprintf(response,
            "HTTP/1.0 200 OK\r\nContent-Length: %zd\r\n\r\n",
            file_type.st_size + 1);

        size_t response_len = strlen(response);
        if (write(clnt_sock, response, response_len) == -1) {
            perror("write response failed, 200 OK");
        }

        // 循环读取文件内容并返回文件内容
        char* file_buf = (char*) malloc((MAX_SEND_LEN + 1) * sizeof(char)); // +1 for null terminator
        ssize_t bytes_read;
        while ((bytes_read = read(file_fd, file_buf, MAX_SEND_LEN)) > 0) {
            if (write(clnt_sock, file_buf, bytes_read) == -1) {
                perror("write file failed");
            }
        }

        write(clnt_sock, "\n", 1);//手动添加换行符，防止文件的最后一行输出到下一个命令行的行首和* Closing connection 0之后

        if (bytes_read == -1) {
            perror("read file failed");
        }

        free(file_buf);
        free(response);
        close(file_fd);
    }

    // 关闭客户端套接字
    close(clnt_sock);

    // 释放内存
    free(req);
}

int main(){
    // 创建套接字，参数说明：
    //   AF_INET: 使用 IPv4
    //   SOCK_STREAM: 面向连接的数据传输方式
    //   IPPROTO_TCP: 使用 TCP 协议
    int serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // 设置 SO_REUSEADDR 选项，避免server无法重启
	int opt = 1;
	setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 将套接字和指定的 IP、端口绑定
    //   用 0 填充 serv_addr（它是一个 sockaddr_in 结构体）
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    //   设置 IPv4
    //   设置 IP 地址
    //   设置端口
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(BIND_IP_ADDR);
    serv_addr.sin_port = htons(BIND_PORT);
    //   绑定
    bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    // 使得 serv_sock 套接字进入监听状态，开始等待客户端发起请求
    listen(serv_sock, MAX_CONN);

    // 接收客户端请求，获得一个可以与客户端通信的新的生成的套接字 clnt_sock
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);

    ThreadPool *pool = ThreadPool_Create(MAX_THREAD, MAX_QUEUE_SIZE);

    while (1) // 一直循环
    {
		// 当没有客户端连接时， accept() 会阻塞程序执行，直到有客户端连接进来
		int clnt_socket = accept(serv_sock, (struct sockaddr *)&clnt_addr,
								   &clnt_addr_size);
		// 处理客户端的请求
		// handle_clnt(client_socket);
		if (clnt_socket != -1) { ThreadPool_Add(pool, clnt_socket); }
    }

    // 实际上这里的代码不可到达，可以在 while 循环中收到 SIGINT 信号时主动 break
    // 关闭套接字
    close(serv_sock);
    return 0;
}