#include <stdio.h>
#include <stdlib.h> // for atoi
#include <sys/socket.h>
#include <unistd.h> // for read, write
#include <string.h> // for strlen
#include <netinet/in.h> // for struct sockaddr_in
#include <arpa/inet.h> // for inet_addr
#include <fcntl.h> // for O_RDONLY

#define MAX_LINE 20
#define REQ_LINE 200

// funtion prototype
int take_input();
void handle_request(int client_sd);
char *get_content_type(char *filename);
void send_data(FILE *client_w, char *ct, char *file_name);
void send_error(FILE *client_w);

int main()
{
    int port; // port number
    int server_sd, client_sd; // socket descriptor
    struct sockaddr_in server_addr; // socket address structure (server)
    struct sockaddr_in client_addr; // socket address structure (client)
    socklen_t client_size;
    char args[2][12]; // placeholder for command line
    pthread_t tid;

    port = take_input(args);
    printf("set port : %d\n", port);

    if (port == -1) {
        fprintf(stdout, "invalid port format");
        return 1;
    }

    if ((server_sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) { // create socket
        perror("socket");
        return 1;
    }
    
    // set the packet address
    memset(&server_addr, 0, sizeof(server_addr)); // initializing the structure
    server_addr.sin_family = AF_INET; // type : ipv4
    server_addr.sin_port = htons(port); // port
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  /* ip address
    (INADDR_ANY : use available ip from lan card */

    /* for reusing address and port
    because the func bind() binds the address and port only once */
    int opt = 1;
    if (setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("set sockopt(address)");
        return 1;
    }
    if (setsockopt(server_sd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("set sockopt(port)");
        return 1;
    }

    // bind socket with server address
    if (bind(server_sd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return 1;
    }
    
    // keep listening with wait queue which has size of 10
    if (listen(server_sd, 10) < 0) {
        perror("listen");
        return 1;
    }

    // accept request
    while (1) {
        client_size = sizeof(client_addr);
        client_sd = accept(server_sd, (struct sockaddr*) &client_addr, &client_size);
        if (client_sd < 0)
            perror("accept");
        // Request logging
        printf("Request from %s:%d\n", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);

        handle_request(client_sd);
    }

    close(server_sd);
    
    return 0;
}

int take_input(char **args)
{
    int port = -1;

    char line[MAX_LINE];
    fgets(line, MAX_LINE, stdin);
    line[strlen(line)-1] = 0; // ignore '\n' character

    // String parsing
    char *parsed;
    int i = 0;
    parsed = strtok(line, " ");
    while (parsed != NULL) {
        args[i++] = parsed;
        parsed = strtok(NULL, " ");
    }

    if (args[1] != NULL) // getting port number
        port = atoi(args[1]);

    return port;
}

void handle_request(int client_sd)
{
    FILE *client_read;
    FILE *client_write;
    char line[REQ_LINE];

    char method[10];
    char file_name[30];
    char content_type[15];

    char msg[2048]; // message buffer

    int received = recv(client_sd, msg, 2047, 0);

    printf("-------Request message from Client-------\n");
    printf("%s", msg);
    printf("------------\n");

    /* < -- parsing request line into method, filename, content-type -- > */
    client_read = fdopen(client_sd, "r");
    client_write = fdopen(dup(client_sd), "w");

    fgets(line, REQ_LINE, client_read);
    // looking for substring "HTTP/" -> checking the file type is http
    if (strstr(line, "HTTP/") == NULL) {
        send_error(client_write);
        fclose(client_read);
        fclose(client_write);
        return;
    }

    // check requested method is GET
    strcpy(method, strtok(line, " /"));
    if (strcmp(method, "GET") != 0) {
        send_error(client_write);
        fclose(client_read);
        fclose(client_write);
        return;
    }

    strcpy(file_name, strtok(NULL, " /"));
    strcpy(content_type, get_content_type(file_name));
    fclose(client_read); // read done


    send_data(client_write, content_type, file_name);

}

char *get_content_type(char *filename)
{
    char extension[100];
    char file_name[100];

    strcpy(file_name, filename);
    strtok(file_name, "."); // discard filename
    strcpy(extension, strtok(NULL, ".")); // take extension (ex .html)

    if (strcmp(extension, "html") == 0 || strcmp(extension, "htm"))
        return "text/html";
    else {
        return "text/plain";
    }
}

// write data message on client
void send_data(FILE *client_w, char *ct, char *file_name)
{
    char protocol[] = "HTTP/1.0 200 OK\r\n";
    char server[] = "Server:My Own Server!\r\n";
    char *content_type;
    char buf[2048];

    content_type = ct;
    printf("content-type : %s\n", content_type);

    // file to send
    FILE *obj = fopen(file_name, "r");
    int fd = fileno(client_w);
    if (obj == NULL) {
        printf("file not found\n");
        send_error(client_w);
        fclose(client_w);
        return;
    }

    // send header to packet
    send(fd, "HTTP/1.0 200 OK\r\n", 17, 0);
    // fputs(protocol, client_w);
    fputs(server, client_w);
    fputs(content_type, client_w);

    while (fgets(buf, 2048, obj) != NULL) {
        write(fd, buf, 2048);
        fflush(client_w);
    }

    fflush(client_w);
    fclose(client_w);
}

// write error message on client
void send_error(FILE *client_w)
{
    char protocol[] = "HTTP/1.0 400 Bad Request\r\n";
    char server[] = "Server:My Own Server!\r\n";
    char content_type[] = "Content-type:text/html\r\n\r\n";
    char content[] = "<html><head><title>오류가 발생했습니다. 파일명을 확인해주세요.</title></head></html>";

    // write header
    fputs(protocol, client_w);
    fputs(server, client_w);
    fputs(content_type, client_w);
    fflush(client_w);
}