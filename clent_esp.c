#include "reverse_tcp.h" 
#include <string.h>
#include "espressif/esp_common.h"
#include "FreeRTOS.h"
#include "task.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <netdb.h>

#define BUFFER_SIZE 4096
#define R_HOST "192.168.43.240"
#define R_PORT 8080


int create_socket(char *_server, int _port) {
  int sockfd;
  struct sockaddr_in serverSockAddr;
  struct hostent *serverHostEnt;
  long hostAddr;
  bzero(&serverSockAddr,sizeof(serverSockAddr));
  hostAddr = inet_addr(_server);
  if ( (long)hostAddr != (long)-1){
    printf("coppy! %ld\n",hostAddr);
    bcopy(&hostAddr,&serverSockAddr.sin_addr,sizeof(hostAddr));
  }
  else
  {
        printf("resolve!\n");
    serverHostEnt = gethostbyname(_server);
    if (serverHostEnt == NULL)
    {
      printf("gethost fail\n");
      return -1;
    }
    bcopy(serverHostEnt->h_addr,&serverSockAddr.sin_addr,serverHostEnt->h_length);
  }
  serverSockAddr.sin_port = htons(_port);
  serverSockAddr.sin_family = AF_INET;
  printf("Pre Conn %s:%d", _server, _port);
  if ( (sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    printf("failed to create socket {%d}\n", sockfd);
    return -1;
  }
  
  if(connect( sockfd,
    (struct sockaddr *)&serverSockAddr,
              sizeof(serverSockAddr)) < 0 ) {
    printf("conection failed\n");
    return -1;
  }
  return sockfd;
}

void reverse_tcp_task(void *pvParameters) {
    int __attribute__((aligned(4))) remote;
    struct __attribute__((aligned(4))) sockaddr_in remote_addr;
    char __attribute__((aligned(4))) buffer[BUFFER_SIZE];
    int __attribute__((aligned(4))) bytes_received;

    retr:

    // Create socket
    remote = create_socket(R_HOST, R_PORT);

    if(remote < 0) {
        printf("Could not connect to client {%d}\n", remote);
        vTaskDelay(1000);
        printf("RETR\n");
        goto retr;

    }

    printf("CONNECTED!!!\n\n");

    while (1) {
        bytes_received = recv(remote, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }

        buffer[bytes_received] = '\0';
        printf("Received: %s\n", buffer);

        if (strstr(buffer, "CONNECT") != NULL) {
            int __attribute__((aligned(4))) client;
            // struct sockaddr_in client_addr;
            char __attribute__((aligned(4))) *token;
            char __attribute__((aligned(4))) *ip;
            int __attribute__((aligned(4))) port;

            // client = socket(AF_INET, SOCK_STREAM, 0);
            // if (client == -1) {
            //     printf("Could not create client socket");
            //     return 1;
            // }

            token = strtok(buffer, " ");
            token = strtok(NULL, " ");
            ip = strtok(token, ":");
            port = atoi(strtok(NULL, ":"));

            // client_addr.sin_addr.s_addr = inet_addr(ip);
            // client_addr.sin_family = AF_INET;
            // client_addr.sin_port = htons(port);

            // if (connect(client, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
            //     printf("Client connect error");
            //     return 1;
            // }

            client = create_socket(ip,port);

            if (client < 0){
              printf("FUCK CLINET");
              return 1;
            }

            printf("Client connected\n");

            while (1) {
                memset(buffer,0,BUFFER_SIZE);
                fd_set __attribute__((aligned(4))) read_fds;
                FD_ZERO(&read_fds);
                FD_SET(client, &read_fds);
                FD_SET(remote, &read_fds);

                if (select(FD_SETSIZE, &read_fds, NULL, NULL, NULL) == -1) {
                    printf("Select error");
                    return 1;
                }

                if (FD_ISSET(client, &read_fds)) {
                    bytes_received = recv(client, buffer, BUFFER_SIZE, 0);
                    if (bytes_received <= 0 || write(remote, buffer, bytes_received) <= 0) {
                        printf("FIRST break;\n");
                        break;
                    }
                }

                if (FD_ISSET(remote, &read_fds)) {
                    bytes_received = recv(remote, buffer, BUFFER_SIZE, 0);
                    if (bytes_received <= 0 || strncmp(buffer, "CLOSE", 5) == 0 || write(client, buffer, bytes_received) <= 0) {
                        printf("Second break;\n");
                        break;
                    }
                }
                
                vTaskDelay(100 / portTICK_PERIOD_MS );
            }
            printf("CLOSEMEFUCK!\n");
            close(client);
        }
        printf("WTF???\n");
        vTaskDelay(100 / portTICK_PERIOD_MS );
        
    }

    close(remote);

    return 0;
}


void start_reverse_tcp(const char * _server, int _port) {
  xTaskCreate(reverse_tcp_task, (const char *)"reverse_tcp_task", 6000, NULL, 1, NULL);
}