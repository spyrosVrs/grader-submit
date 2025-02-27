#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "80"
#define BUFFER_SIZE 4096

void send_http_post_request(const char *hostname, const char *path, const char *cookies, const char *body)
{
    int sock;
    struct addrinfo hints, *res, *p;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname, PORT, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == -1) {
            perror("socket");
            continue;
        }
        if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
            perror("connect");
            close(sock);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Could not connect to %s\n", hostname);
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    int body_length = strlen(body);
    char request[BUFFER_SIZE];
    int req_len = snprintf(request, sizeof(request),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Cookie: %s\r\n"
             "Content-Type: application/x-www-form-urlencoded\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             path, hostname, cookies, body_length, body);

    if (req_len >= sizeof(request)) {
        fprintf(stderr, "Request too long for buffer\n");
        close(sock);
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    if (send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        close(sock);
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    char response[BUFFER_SIZE];
    ssize_t received;
    while ((received = recv(sock, response, sizeof(response) - 1, 0)) > 0) {
        response[received] = '\0';
        printf("%s", response);
    }
    if (received < 0) {
        perror("recv");
    }

    close(sock);
    freeaddrinfo(res);
}

char *get_cookie_string(const char *value)
{
    char *buffer = malloc(256);
    if (buffer == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    snprintf(buffer, 256, "PHPSESSID=%s;", value);
    return buffer;
}

char *get_formdata_string(const char *task, const char *filename)
{
    char *buffer = malloc(256);
    if (buffer == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    snprintf(buffer, 256, "task=%s&lang=C%%2B%%2B&filename=%s", task, filename);
    return buffer;
}

int main()
{
    char param1[50] = {0};
    char param2[50] = {0};
    char param3[50] = {0};

    FILE *file = fopen("config.txt", "r");
    if (file == NULL) {
        perror("Error opening config.txt");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "cookie=", 7) == 0) {
            sscanf(line, "cookie=%49s", param1);
        }
        else if (strncmp(line, "task=", 5) == 0) {
            sscanf(line, "task=%49s", param2);
        }
        else if (strncmp(line, "filename=", 9) == 0) {
            sscanf(line, "filename=%49s", param3);
        }
    }
    fclose(file);

    if (param1[0] == '\0' || param2[0] == '\0' || param3[0] == '\0') {
        fprintf(stderr, "Missing required parameters in config.txt\n");
        exit(EXIT_FAILURE);
    }

    const char *hostname = "grader.softlab.ntua.gr";
    const char *path = "/filesubmitcourses.php";

    char *cookies = get_cookie_string(param1);
    char *form_data = get_formdata_string(param2, param3);

    send_http_post_request(hostname, path, cookies, form_data);

    free(cookies);
    free(form_data);

    return 0;
}
