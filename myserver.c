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
void send_data(int client_sd, char *ct, char *file_name);

int main()
{
    int port; // port number
    int server_sd, client_sd; // socket descriptor
    struct sockaddr_in server_addr; // socket address structure (server)
    struct sockaddr_in client_addr; // socket address structure (client)
    socklen_t client_size;
    char args[2][12]; // placeholder for command line

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
        close(client_sd);
    }

    shutdown(client_sd, SHUT_RDWR);
    
    return 0;
}

/**
 * @brief take input from user with command '%myserver [port]'
 * 
 * @param args placeholder for command line
 * @return int 
 */
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

/**
 * @brief function handles request in server. Also prints request message from client
 * 
 * @param client_sd client socket descriptor
 */
void handle_request(int client_sd)
{
    char *parsed = NULL;

    char method[10];
    char file_name[30];
    char content_type[15];

    char msg[2048]; // message buffer

    int received = recv(client_sd, msg, 2047, 0);

    printf("-------Request message from Client-------\n");
    printf("%s", msg);
    printf("------------\n");

    /* < -- parsing request line into method, file name, content-type -- > */

    parsed = strtok(msg, "\n");

    // looking for substring "HTTP/" -> checking the file type is http
    if (strstr(parsed, "HTTP/") == NULL) {
        return;
    }

    // check requested method is GET -> we're just accepting only GET
    strcpy(method, strtok(parsed, " /"));
    if (strcmp(method, "GET") != 0) {
        return;
    }

    strcpy(file_name, strtok(NULL, " /"));
    strcpy(content_type, get_content_type(file_name));

    send_data(client_sd, content_type, file_name);

}

/**
 * @brief Get the content type object of html
 * 
 * @param filename requested by client
 * @return MIME Type
 */
char *get_content_type(char *filename)
{
    char extension[100];
    char file_name[100];

    strcpy(file_name, filename);
    strtok(file_name, "."); // discard filename
    strcpy(extension, strtok(NULL, ".")); // take extension (ex .html)

    if (strcmp(extension, "html") == 0 || strcmp(extension, "htm"))
        return "text/html";
    else if (strcmp(extension, "gif") == 0) {
        return "image/gif";
    }
    else if (strcmp(extension, "jpeg") == 0) {
        return "image/jpeg";
    }
    else if (strcmp(extension, "mp3") == 0) {
        return "audio/mpeg";
    }
    else if (strcmp(extension, "pdf") == 0) {
        return "application/pdf";
    }
    else {
        return "text/plain";
    }
}

/**
 * @brief write data message on client
 * 
 * @param client_sd 
 * @param ct content-type
 * @param file_name requested by client
 */
void send_data(int client_sd, char *ct, char *file_name)
{
    char protocol[] = "HTTP/1.1 200 OK\n\n";
    char server[] = "Server:My Own Server!\r\n";
    char *content_type;
    char buf[1024];

    FILE *cfd;

    int fd, len;

    content_type = ct;

    // file to send
    if ((fd = open(file_name, O_RDONLY)) == -1) {
        write(client_sd, "HTTP/1.1 404 Not Found\n", 23);
        return;
    }

    // send header to packet
    send(client_sd, protocol, 17, 0);

    while(1) {
        len = read(fd, buf, 1024);
        if (len == 0) break;
        write(client_sd, buf, len);
    }
}
