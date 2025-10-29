
#include "loom/control.h"
#include "loom/capture.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "lwip/sys.h"

static const char welcome_msg[] = 
    "\n"
    "================================\n"
    "  NFV Control Server\n"
    "================================\n"
    "Commands:\n"
    "  STATS  - Show packet statistics\n"
    "  HELP   - Show this message\n"
    "  EXIT   - Close connection\n"
    "================================\n"
    "> ";

/**
 * Handle a single client connection
 */
static void handle_client(int client_fd)
{
    char buffer[256];
    ssize_t n;

    printf("[CONTROL] New client connected\n");

    /* Send welcome message */
    send(client_fd, welcome_msg, strlen(welcome_msg), 0);

    /* Read and echo commands */
    while ((n = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';

        /* Remove trailing newlines */
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        newline = strchr(buffer, '\r');
        if (newline) *newline = '\0';

        printf("[CONTROL] Received command: '%s'\n", buffer);

        /* Process commands */
        if (strcmp(buffer, "EXIT") == 0 || strcmp(buffer, "exit") == 0) {
            const char *bye = "Goodbye!\n";
            send(client_fd, bye, strlen(bye), 0);
            break;
        } 
        else if (strcmp(buffer, "HELP") == 0 || strcmp(buffer, "help") == 0) {
            send(client_fd, welcome_msg, strlen(welcome_msg), 0);
        }
        else if (strcmp(buffer, "STATS") == 0 || strcmp(buffer, "stats") == 0) {
            /* TODO: Get actual stats from capture module */
            capture_stats_t stats = capture_get_stats();

            const char *str1 = "\n=== Capture Statistics ===\n";
            send(client_fd, str1, strlen(str1), 0);

            char buff1[100];
            snprintf(buff1, sizeof(buff1), "Total Packets: %llu\n", (unsigned long long)stats.total_packets);
            send(client_fd, buff1, strlen(buff1), 0);

            char buff2[100];
            snprintf(buff2, sizeof(buff2), "Total Bytes:   %llu\n", (unsigned long long)stats.total_bytes);
            send(client_fd, buff2, strlen(buff2), 0);

            const char *str2 = "=========================\n\n> ";
            send(client_fd, str2, strlen(str2), 0);
        }
        else {
            /* Echo back */
            char response[512];
            snprintf(response, sizeof(response), "Echo: %s\n> ", buffer);
            send(client_fd, response, strlen(response), 0);
        }
    }

    printf("[CONTROL] Client disconnected\n");
    close(client_fd);
}

/**
 * Control server thread
 */
static void control_server_thread(void *arg)
{
    int port = (int)(intptr_t)arg;
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    /* Create socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("[CONTROL] ERROR: Could not create socket\n");
        return;
    }

    /* Set socket options */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Bind to port */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("[CONTROL] ERROR: Could not bind to port %d\n", port);
        close(server_fd);
        return;
    }

    /* Start listening */
    if (listen(server_fd, 5) < 0) {
        printf("[CONTROL] ERROR: Could not listen on port %d\n", port);
        close(server_fd);
        return;
    }

    printf("[CONTROL] Server listening on port %d\n", port);

    /* Accept connections */
    while (1) {
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_fd < 0) {
            printf("[CONTROL] ERROR: Could not accept connection\n");
            continue;
        }

        /* Handle client (blocking - for MVP, we only handle one at a time) */
        handle_client(client_fd);
    }

    close(server_fd);
}

int control_server_init(int port)
{
    printf("[CONTROL] Initializing control server on port %d...\n", port);

    /* Start control server in a separate thread */
    sys_thread_t thread = sys_thread_new("control_srv", 
                                          control_server_thread, 
                                          (void*)(intptr_t)port,
                                          4096,   /* stack size */
                                          3);     /* priority */
    
    if (thread == NULL) {
        printf("[CONTROL] ERROR: Could not create control server thread\n");
        return -1;
    }

    printf("[CONTROL] Control server thread started\n");
    return 0;
}