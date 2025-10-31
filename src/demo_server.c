
#include "loom/demo_server.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "lwip/sys.h"

static void demo_server_thread(void *arg)
{
    int port = (int)(intptr_t)arg;
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("[DEMO SERVER] ERROR: Could not create socket\n");
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("[DEMO SERVER] ERROR: Could not bind to port %d\n", port);
        close(server_fd);
        return;
    }

    if (listen(server_fd, 5) < 0) {
        printf("[DEMO SERVER] ERROR: Could not listen on port %d\n", port);
        close(server_fd);
        return;
    }

    printf("[DEMO SERVER] Server listening on port %d\n", port);

    while (1) {
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_fd < 0) {
            printf("[DEMO SERVER] ERROR: Could not accept connection\n");
            continue;
        }

        const char *msg = "Hello, World!\n";
        send(client_fd, msg, strlen(msg), 0);
        close(client_fd);
    }

    close(server_fd);
}

int demo_server_init(int port)
{
    printf("[DEMO SERVER] Initializing demo server on port %d...\n", port);

    sys_thread_t thread = sys_thread_new("demo_srv", 
                                          demo_server_thread, 
                                          (void*)(intptr_t)port,
                                          4096,
                                          3);
    
    if (thread == NULL) {
        printf("[DEMO SERVER] ERROR: Could not create demo server thread\n");
        return -1;
    }

    printf("[DEMO SERVER] Demo server thread started\n");
    return 0;
}