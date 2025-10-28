#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uk/essentials.h>
#include <uk/sched.h>

/* Network stack includes - try both paths */
#if __has_include("lwip/init.h")
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "lwip/etharp.h"
#include "lwip/tcpip.h"
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/tcp.h"
#else
/* Fallback to system includes */
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/ip_addr.h>
#include <lwip/tcp.h>
#include <lwip/ip.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>
#include <lwip/prot/ethernet.h>
#include <lwip/prot/ip4.h>
#include <lwip/prot/tcp.h>
#endif

int main(int argc __unused, char *argv[] __unused) {
    /* Get default network interface */
    struct netif *netif = netif_default;
    if (!netif) {
        printf("[ERROR] No network interface available!\n");
        printf("[INFO] Waiting for network initialization...\n");
        
        /* Wait for netif to become available */
        int retries = 10;
        while (!netif && retries > 0) {
            for (volatile int i = 0; i < 50000000; i++);
            netif = netif_default;
            retries--;
        }
        
        if (!netif) {
            printf("[ERROR] Network interface still not available. Exiting.\n");
            return -1;
        }
    }
    
    printf("[NET] Interface: %c%c%d\n", netif->name[0], netif->name[1], netif->num);
    
    netif_set_up(netif);
    netif_set_link_up(netif);

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
    
    while(1) {
        uk_sched_yield();
    }
    
    return 0;
}
