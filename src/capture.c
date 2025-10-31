#include "loom/capture.h"
#include "loom/nf_chain.h"
#include <stdio.h>
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/tcp.h"

static err_t (*original_input_fn)(struct pbuf *p, struct netif *inp) = NULL;

static uint16_t control_port = 0;

static capture_stats_t stats = {0};

static bool is_control_packet(struct pbuf *p)
{
    if (p->tot_len < sizeof(struct eth_hdr) + sizeof(struct ip_hdr)) {
        return false;
    }

    struct eth_hdr *eth = (struct eth_hdr *)p->payload;
    uint16_t eth_type = lwip_ntohs(eth->type);
    
    if (eth_type != ETHTYPE_IP) {
        return false;
    }

    struct ip_hdr *ip = (struct ip_hdr *)((uint8_t *)p->payload + sizeof(struct eth_hdr));
    
    if (IPH_PROTO(ip) != 6) { // 6 is TCP
        return false;
    }

    struct tcp_hdr *tcp = (struct tcp_hdr *)((uint8_t *)ip + (IPH_HL(ip) * 4));
    uint16_t dst_port = lwip_ntohs(tcp->dest);

    return (dst_port == control_port);
}

static err_t capture_input_hook(struct pbuf *p, struct netif *inp)
{
    if (p == NULL) {
        return ERR_OK;
    }

    stats.total_packets++;
    stats.total_bytes += p->tot_len;

    if (is_control_packet(p)) {
        stats.passed_packets++;
        return original_input_fn(p, inp);
    }

    bool allow = nf_chain_process(p);
    
    if (allow) {
        stats.passed_packets++;
        return original_input_fn(p, inp);
    } else {
        stats.dropped_packets++;
        printf("[CAPTURE] Packet dropped by NF chain\n");
        pbuf_free(p);
        return ERR_OK;
    }
}

int capture_hook_init(struct netif *netif, uint16_t port)
{
    if (!netif) {
        printf("[CAPTURE] ERROR: netif is NULL\n");
        return -1;
    }

    if (!netif->input) {
        printf("[CAPTURE] ERROR: netif->input is NULL\n");
        return -1;
    }

    control_port = port;

    original_input_fn = netif->input;

    netif->input = capture_input_hook;

    printf("[CAPTURE] Packet capture hook installed on %c%c%d\n",
           netif->name[0], netif->name[1], netif->num);
    printf("[CAPTURE] Control port %u will bypass NF chain\n", port);

    return 0;
}

void capture_print_stats(void)
{
    printf("\n=== Capture Statistics ===\n");
    printf("Total Packets:   %llu\n", (unsigned long long)stats.total_packets);
    printf("Total Bytes:     %llu\n", (unsigned long long)stats.total_bytes);
    printf("Passed Packets:  %llu\n", (unsigned long long)stats.passed_packets);
    printf("Dropped Packets: %llu\n", (unsigned long long)stats.dropped_packets);
    printf("=========================\n\n");
}

capture_stats_t capture_get_stats() {
    return stats;
}