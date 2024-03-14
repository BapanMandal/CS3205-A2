#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// #include <fcntl.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_STRING "Server: Ubuntu 22.04 LTS/0.1\r\n"
#define MAX_REQUEST_SIZE 1024
#define BUFFER_SIZE 1024
#define DEFAULT_PORT 8888
#define ROOT_DIR "http_server/web2"

typedef struct
{
    int fd;
    char *root_dir;
} http_request_t;

void send_error(int fd, int status_code, char *message)
{
    char buf[MAX_REQUEST_SIZE];
    sprintf(buf,
            "HTTP/1.1 %d %s\r\n"
            "%s\r\n",
            status_code, message, SERVER_STRING);
    send(fd, buf, strlen(buf), 0);
}

void send_file(int fd, char *root_dir, char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        char buf[MAX_REQUEST_SIZE];
        sprintf(buf,
            "HTTP/1.1 404 Not found\r\n"
            "Server: SimpleServer/0.1\r\n"
            "Content-Type: text/html\r\n\r\n");
        send(fd, buf, strlen(buf), 0);

        memset(buf, 0, sizeof(buf));
        strcpy(buf, root_dir);
        strcat(buf, "/404.html");

        FILE *fp = fopen(buf, "rb");
        if (fp == NULL)
        {
            send_error(fd, 404, "Not found");
            return;
        }

        size_t bytes_read;
        while ((bytes_read = fread(buf, 1, sizeof(buf), fp)) > 0)
        {
            send(fd, buf, bytes_read, 0);
        }
        fclose(fp);

        return;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp)+1;
    fseek(fp, 0, SEEK_SET);

    char buf[BUFFER_SIZE], response_header[256];
    char content_type[128] = "text/plain";

    if (strstr(path, ".html") != NULL)
        strcpy(content_type, "text/html");
    else if (strstr(path, ".css") != NULL)
        strcpy(content_type, "text/css");
    else if (strstr(path, ".js") != NULL)
        strcpy(content_type, "application/javascript");
    else if (strstr(path, ".png") != NULL)
        strcpy(content_type, "image/png");
    else if (strstr(path, ".jpg") || strstr(path, ".jpeg"))
        strcpy(content_type, "image/jpeg");

    sprintf(response_header,
            "HTTP/1.1 200 OK\r\n"
            "Server: SimpleServer/0.1\r\n"
            // "Content-Length: %ld\r\n"
            "Content-Type: %s\r\n\r\n",
            /*file_size,*/ content_type);

    send(fd, response_header, strlen(response_header), 0);

    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), fp)) > 0)
    {
        send(fd, buf, bytes_read, 0);
    }
    fclose(fp);
}


void *handle_request(void *args)
{
    http_request_t *req = (http_request_t *)args;
    int fd = req->fd;
    char *root_dir = req->root_dir;

    char request[MAX_REQUEST_SIZE];
    int ret = recv(fd, request, MAX_REQUEST_SIZE, 0);

    if (ret < 0)
    {
        fprintf(stderr, "Error receiving request\n");
        close(fd);
        return NULL;
    }

    char method[MAX_REQUEST_SIZE], path[MAX_REQUEST_SIZE];
    sscanf(request, "%s %s HTTP/1.1", method, path);

    if (strcmp(method, "GET") != 0)
    {
        send_error(fd, 501, "Not implemented");
        close(fd);
        return NULL;
    }

    if (strcmp(path, "/") == 0)
    {
        strcpy(path, "/index.html");
    }

    char full_path[MAX_REQUEST_SIZE];
    sprintf(full_path, "%s%s", root_dir, path);

    send_file(fd, root_dir, full_path);

    close(fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
    if (argc == 2)
    {
        port = atoi(argv[1]);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        fprintf(stderr, "Error creating socket\n");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "Error binding\n");
        return 1;
    }

    if (listen(server_fd, 10) < 0)
    {
        fprintf(stderr, "Error listening\n");
        return 1;
    }

    printf("Server running on port %d...\n", port);

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

        if (client_fd < 0)
        {
            fprintf(stderr, "Error accepting connection\n");
            continue;
        }

        http_request_t *req = (http_request_t *)malloc(sizeof(http_request_t));
        req->fd = client_fd;
        req->root_dir = ROOT_DIR;

        pthread_t thread;
        pthread_create(&thread, NULL, handle_request, req);
        pthread_detach(thread);
    }

    close(server_fd);
    return 0;
}
