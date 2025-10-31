#include "loom/control.h"
#include "loom/capture.h"
#include "loom/nf_chain.h"

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
    "  LIST   - List NF chain\n"
    "  ENABLE <nf> / DISABLE <nf>\n"
    "  REMOVE <nf> / CLEAR\n"
    "\n"
    "Rate Limiter:\n"
    "  RATELIMIT SET <port> <pps>\n"
    "  RATELIMIT REMOVE <port>\n"
    "  RATELIMIT LIST\n"
    "\n"
    "Allowlist:\n"
    "  ALLOW ADD <port>\n"
    "  ALLOW REMOVE <port>\n"
    "  ALLOW LIST\n"
    "  ALLOW CLEAR\n"
    "\n"
    "  HELP / EXIT\n"
    "================================\n"
    "> ";

static void handle_client(int client_fd)
{
    char buffer[256];
    ssize_t n;

    printf("[CONTROL] New client connected\n");
    send(client_fd, welcome_msg, strlen(welcome_msg), 0);

    while ((n = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';

        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        newline = strchr(buffer, '\r');
        if (newline) *newline = '\0';

        printf("[CONTROL] Received command: '%s'\n", buffer);

        if (strcmp(buffer, "EXIT") == 0 || strcmp(buffer, "exit") == 0) {
            const char *bye = "Goodbye!\n";
            send(client_fd, bye, strlen(bye), 0);
            break;
        } 
        else if (strcmp(buffer, "HELP") == 0 || strcmp(buffer, "help") == 0) {
            send(client_fd, welcome_msg, strlen(welcome_msg), 0);
        }
        else if (strcmp(buffer, "STATS") == 0 || strcmp(buffer, "stats") == 0) {
            capture_stats_t stats = capture_get_stats();
            char response[512];
            snprintf(response, sizeof(response),
                    "\n=== Statistics ===\n"
                    "Total:   %llu packets (%llu bytes)\n"
                    "Passed:  %llu\n"
                    "Dropped: %llu\n"
                    "==================\n> ",
                    (unsigned long long)stats.total_packets,
                    (unsigned long long)stats.total_bytes,
                    (unsigned long long)stats.passed_packets,
                    (unsigned long long)stats.dropped_packets);
            send(client_fd, response, strlen(response), 0);
        }
        else if (strcmp(buffer, "LIST") == 0 || strcmp(buffer, "list") == 0) {
            nf_chain_list();
            const char *msg = "Chain listed (check console)\n> ";
            send(client_fd, msg, strlen(msg), 0);
        }
        else if (strncmp(buffer, "ENABLE ", 7) == 0) {
            if (nf_chain_set_enabled(buffer + 7, true) == 0) {
                const char *msg = "OK\n> ";
                send(client_fd, msg, strlen(msg), 0);
            } else {
                const char *msg = "ERROR: NF not found\n> ";
                send(client_fd, msg, strlen(msg), 0);
            }
        }
        else if (strncmp(buffer, "DISABLE ", 8) == 0) {
            if (nf_chain_set_enabled(buffer + 8, false) == 0) {
                const char *msg = "OK\n> ";
                send(client_fd, msg, strlen(msg), 0);
            } else {
                const char *msg = "ERROR: NF not found\n> ";
                send(client_fd, msg, strlen(msg), 0);
            }
        }
        else if (strncmp(buffer, "REMOVE ", 7) == 0) {
            if (nf_chain_remove(buffer + 7) == 0) {
                const char *msg = "OK\n> ";
                send(client_fd, msg, strlen(msg), 0);
            } else {
                const char *msg = "ERROR\n> ";
                send(client_fd, msg, strlen(msg), 0);
            }
        }
        else if (strcmp(buffer, "CLEAR") == 0) {
            nf_chain_clear();
            const char *msg = "OK\n> ";
            send(client_fd, msg, strlen(msg), 0);
        }
        /* Rate limiter commands */
        else if (strncmp(buffer, "RATELIMIT SET ", 14) == 0) {
            uint16_t port;
            uint32_t pps;
            if (sscanf(buffer + 14, "%hu %u", &port, &pps) == 2) {
                if (nf_rate_limiter_set_limit(port, pps) == 0) {
                    const char *msg = "OK\n> ";
                    send(client_fd, msg, strlen(msg), 0);
                } else {
                    const char *msg = "ERROR\n> ";
                    send(client_fd, msg, strlen(msg), 0);
                }
            } else {
                const char *msg = "ERROR: Usage: RATELIMIT SET <port> <pps>\n> ";
                send(client_fd, msg, strlen(msg), 0);
            }
        }
        else if (strncmp(buffer, "RATELIMIT REMOVE ", 17) == 0) {
            uint16_t port;
            if (sscanf(buffer + 17, "%hu", &port) == 1) {
                nf_rate_limiter_remove_limit(port);
                const char *msg = "OK\n> ";
                send(client_fd, msg, strlen(msg), 0);
            } else {
                const char *msg = "ERROR: Usage: RATELIMIT REMOVE <port>\n> ";
                send(client_fd, msg, strlen(msg), 0);
            }
        }
        else if (strcmp(buffer, "RATELIMIT LIST") == 0) {
            nf_rate_limiter_list();
            const char *msg = "Listed (check console)\n> ";
            send(client_fd, msg, strlen(msg), 0);
        }
        /* Allowlist commands */
        else if (strncmp(buffer, "ALLOW ADD ", 10) == 0) {
            uint16_t port;
            if (sscanf(buffer + 10, "%hu", &port) == 1) {
                if (nf_allowlist_add_port(port) == 0) {
                    const char *msg = "OK\n> ";
                    send(client_fd, msg, strlen(msg), 0);
                } else {
                    const char *msg = "ERROR\n> ";
                    send(client_fd, msg, strlen(msg), 0);
                }
            } else {
                const char *msg = "ERROR: Usage: ALLOW ADD <port>\n> ";
                send(client_fd, msg, strlen(msg), 0);
            }
        }
        else if (strncmp(buffer, "ALLOW REMOVE ", 13) == 0) {
            uint16_t port;
            if (sscanf(buffer + 13, "%hu", &port) == 1) {
                nf_allowlist_remove_port(port);
                const char *msg = "OK\n> ";
                send(client_fd, msg, strlen(msg), 0);
            } else {
                const char *msg = "ERROR: Usage: ALLOW REMOVE <port>\n> ";
                send(client_fd, msg, strlen(msg), 0);
            }
        }
        else if (strcmp(buffer, "ALLOW LIST") == 0) {
            nf_allowlist_list();
            const char *msg = "Listed (check console)\n> ";
            send(client_fd, msg, strlen(msg), 0);
        }
        else if (strcmp(buffer, "ALLOW CLEAR") == 0) {
            nf_allowlist_clear();
            const char *msg = "OK\n> ";
            send(client_fd, msg, strlen(msg), 0);
        }
        else {
            char response[256];
            snprintf(response, sizeof(response), "Unknown: %s\n> ", buffer);
            send(client_fd, response, strlen(response), 0);
        }
    }

    printf("[CONTROL] Client disconnected\n");
    close(client_fd);
}

/* Rest remains the same... */
static void control_server_thread(void *arg)
{
    int port = (int)(intptr_t)arg;
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("[CONTROL] ERROR: Could not create socket\n");
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("[CONTROL] ERROR: Could not bind to port %d\n", port);
        close(server_fd);
        return;
    }

    if (listen(server_fd, 5) < 0) {
        printf("[CONTROL] ERROR: Could not listen on port %d\n", port);
        close(server_fd);
        return;
    }

    printf("[CONTROL] Server listening on port %d\n", port);

    while (1) {
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_fd < 0) {
            printf("[CONTROL] ERROR: Could not accept connection\n");
            continue;
        }

        handle_client(client_fd);
    }

    close(server_fd);
}

int control_server_init(int port)
{
    printf("[CONTROL] Initializing control server on port %d...\n", port);

    sys_thread_t thread = sys_thread_new("control_srv", 
                                          control_server_thread, 
                                          (void*)(intptr_t)port,
                                          4096,
                                          3);
    
    if (thread == NULL) {
        printf("[CONTROL] ERROR: Could not create control server thread\n");
        return -1;
    }

    printf("[CONTROL] Control server thread started\n");
    return 0;
}