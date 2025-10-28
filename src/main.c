
#include <stdio.h>
#include <string.h>

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/api.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "loom/capture.h"

static err_t (*default_netif_input_fn)(struct pbuf *p, struct netif *inp) = NULL;

static err_t hook_input(struct pbuf *p, struct netif *inp) {
    if (p == NULL) {
        return ERR_OK;
    }

    printf("[CAPTURE HOOK] received pkt tot_len=%u on netif %p\n",
           (unsigned)p->tot_len, (void*)inp);

    return default_netif_input_fn(p, inp);
}

static int hook_netif(struct netif *n) {
    if (!n) return -1;
    default_netif_input_fn = n->input;
    n->input = hook_input;
    return 0;
}

// ======================================================================================================================================

#define CONTROL_PORT 9000

static const char reply[] = "Hello, World\n";

static void tcp_ctrl_thread(void *arg) {
    LWIP_UNUSED_ARG(arg);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        printf("[ctrl] could not create control socket\n");
        return;
    }
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CONTROL_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("[ctrl] could not bind control socket\n");
        close(srv);
        return;
    }

    if (listen(srv, 10) < 0) {
        printf("[ctrl] could not start listening on control socket\n");
        close(srv);
        return;
    }

    printf("[ctrl] control server listening on port %d\n", CONTROL_PORT);

    while (1) {
        int cfd;
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        cfd = accept(srv, (struct sockaddr *)&cli, &clen);
        if (cfd < 0) {
            printf("[ctrl] could not accept a connection\n");
            continue;
        }

        printf("[ctrl] accepted a new connection\n");
        send(cfd, reply, strlen(reply), 0);
        close(cfd);
    }

    close(srv);
}

// ======================================================================================================================================

int main(void) {
    struct netif *netif = netif_default;
    if (!netif) {
        printf("[ERROR] No network interface available!\n");
        return -1;
    }
    
    printf("[NET] Interface: %c%c%d\n", netif->name[0], netif->name[1], netif->num);

    sleep(3);    
    char ip_str[16], nm_str[16], gw_str[16];
    
    ip4addr_ntoa_r(netif_ip4_addr(netif), ip_str, sizeof(ip_str));
    ip4addr_ntoa_r(netif_ip4_netmask(netif), nm_str, sizeof(nm_str));
    ip4addr_ntoa_r(netif_ip4_gw(netif), gw_str, sizeof(gw_str));
    
    printf("\n[NET] ===== Network Configuration =====\n");
    printf("[NET] IP Address: %s\n", ip_str);
    printf("[NET] Netmask:    %s\n", nm_str);
    printf("[NET] Gateway:    %s\n", gw_str);
    printf("[NET] ====================================\n\n");

    hook_netif(netif);

    sys_thread_new("tcp_ctrl", tcp_ctrl_thread, NULL, 4096, 3);

    while (1) {
        uk_sched_yield();
    }

    return 0;
}
