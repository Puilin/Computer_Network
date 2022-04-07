#include <stdio.h>
#include <stdlib.h> // for atoi
#include <sys/socket.h>
#include <unistd.h> // for read, write
#include <string.h> // for strlen
#include <netinet/in.h> // for struct sockaddr_in
#include <arpa/inet.h> // for inet_addr

#define MAX_LINE 20

// funtion prototype
int take_input();

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
    if (setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("set sockopt(address)");
        return 1;
    }
    if (setsockopt(server_sd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt)) < 0) {
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
    client_size = sizeof(client_addr);
    client_sd = accept(server_sd, (struct sockaddr*) &client_addr, &client_size);
    if (client_sd < 0)
        perror("accept");
    
    char msg[] = "Hello client !\n";
    write(client_sd, msg, sizeof(msg));

    close(server_sd);
    close(client_sd);
    
    return 0;
}

int take_input(char **args)
{
    int port = -1;

    char buf[MAX_LINE];
    fgets(buf, MAX_LINE, stdin);
    buf[strlen(buf)-1] = 0; // ignore '\n' character

    // String parsing
    char *parsed;
    int i = 0;
    parsed = strtok(buf, " ");
    while (parsed != NULL) {
        args[i++] = parsed;
        parsed = strtok(NULL, " ");
    }

    if (args[1] != NULL) // getting port number
        port = atoi(args[1]);

    return port;
}