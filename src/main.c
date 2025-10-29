#include <stdio.h>
#include <unistd.h>
#include <uk/sched.h>
#include "lwip/netif.h"
#include "loom/capture.h"
#include "loom/control.h"

#define CONTROL_PORT 9000

int main(void)
{
    printf("\n");
    printf("====================================\n");
    printf("  NFV Unikernel - Loom\n");
    printf("====================================\n\n");

    /* Get default network interface */
    struct netif *netif = netif_default;
    if (!netif) {
        printf("[ERROR] No network interface available!\n");
        return -1;
    }
    
    printf("[NET] Interface: %c%c%d\n", 
           netif->name[0], netif->name[1], netif->num);

    /* Wait for network initialization */
    printf("[NET] Waiting for network initialization...\n");
    sleep(3);
    
    /* Print network configuration */
    char ip_str[16], nm_str[16], gw_str[16];
    ip4addr_ntoa_r(netif_ip4_addr(netif), ip_str, sizeof(ip_str));
    ip4addr_ntoa_r(netif_ip4_netmask(netif), nm_str, sizeof(nm_str));
    ip4addr_ntoa_r(netif_ip4_gw(netif), gw_str, sizeof(gw_str));
    
    printf("\n[NET] ===== Network Configuration =====\n");
    printf("[NET] IP Address: %s\n", ip_str);
    printf("[NET] Netmask:    %s\n", nm_str);
    printf("[NET] Gateway:    %s\n", gw_str);
    printf("[NET] ====================================\n\n");

    /* Initialize packet capture hook */
    if (capture_hook_init(netif) < 0) {
        printf("[ERROR] Failed to initialize capture hook\n");
        return -1;
    }

    /* Initialize control server */
    if (control_server_init(CONTROL_PORT) < 0) {
        printf("[ERROR] Failed to initialize control server\n");
        return -1;
    }

    printf("\n====================================\n");
    printf("  System Ready!\n");
    printf("====================================\n");
    printf("  Control: telnet %s %d\n", ip_str, CONTROL_PORT);
    printf("====================================\n\n");

    /* Main loop */
    while (1) {
        uk_sched_yield();
    }

    return 0;
}