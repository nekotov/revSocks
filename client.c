#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#define BUFFER_SIZE 4096
#define R_HOST "127.0.0.1"
#define R_PORT 8080

int main() {
    int remote;
    struct sockaddr_in remote_addr;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Create socket
    remote = socket(AF_INET, SOCK_STREAM, 0);
    if (remote == -1) {
        printf("Could not create socket");
        return 1;
    }

    // Prepare the sockaddr_in structure
    remote_addr.sin_addr.s_addr = inet_addr(R_HOST);
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(R_PORT);

    // Connect to remote server
    if (connect(remote, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
        printf("Connect error");
        return 1;
    }

    printf("Connected\n");

    while (1) {
        bytes_received = recv(remote, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }

        buffer[bytes_received] = '\0';
        printf("Received: %s\n", buffer);

        if (strncmp(buffer, "CONNECT", 7) == 0) {
            int client;
            struct sockaddr_in client_addr;
            char *token;
            char *ip;
            int port;

            client = socket(AF_INET, SOCK_STREAM, 0);
            if (client == -1) {
                printf("Could not create client socket");
                return 1;
            }

            token = strtok(buffer, " ");
            token = strtok(NULL, " ");
            ip = strtok(token, ":");
            port = atoi(strtok(NULL, ":"));

            client_addr.sin_addr.s_addr = inet_addr(ip);
            client_addr.sin_family = AF_INET;
            client_addr.sin_port = htons(port);

            if (connect(client, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                printf("Client connect error");
                return 1;
            }

            printf("Client connected\n");

            while (1) {
                fd_set read_fds;
                FD_ZERO(&read_fds);
                FD_SET(client, &read_fds);
                FD_SET(remote, &read_fds);

                if (select(FD_SETSIZE, &read_fds, NULL, NULL, NULL) == -1) {
                    printf("Select error");
                    return 1;
                }

                if (FD_ISSET(client, &read_fds)) {
                    bytes_received = recv(client, buffer, BUFFER_SIZE, 0);
                    if (bytes_received <= 0 || send(remote, buffer, bytes_received, 0) <= 0) {
                        break;
                    }
                }

                if (FD_ISSET(remote, &read_fds)) {
                    bytes_received = recv(remote, buffer, BUFFER_SIZE, 0);
                    if (bytes_received <= 0 || strncmp(buffer, "CLOSE", 5) == 0 || send(client, buffer, bytes_received, 0) <= 0) {
                        break;
                    }
                }
            }
            printf("clinet close!\n");
            write(remote, "WeCloseIt", 10);
            close(client);
        }else 
        if (strncmp(buffer, "gDOMAIN", 7) == 0) {
            printf("domain!\n");
            unsigned int szDomain;
            printf("to read\n");
            read(remote, &szDomain, sizeof(szDomain));
            szDomain = ntohl(szDomain);
            printf("strlen = %u\n", szDomain);
            char* domain = malloc(szDomain*sizeof(char)+1);
            read(remote, domain, szDomain);
            domain[szDomain+1] = 0;
            printf("Looking up (%.*s)\n", szDomain, domain);

            struct hostent *host_info;
            char ip_buffer[INET_ADDRSTRLEN];

            host_info = gethostbyname(domain);
            if (host_info == NULL) {
                printf("Failed to get host by name.\n");
                return 1;
            }

            inet_ntop(AF_INET, (void *)host_info->h_addr_list[0], ip_buffer, sizeof(ip_buffer));

            printf("IP address: %s\n", ip_buffer);
            
            // sending strlen
            int sz_ip = strlen(ip_buffer);
            write(remote, &sz_ip, sizeof(sz_ip));

            write(remote, &ip_buffer, strlen(ip_buffer));
        }
    }

    close(remote);

    return 0;
}
