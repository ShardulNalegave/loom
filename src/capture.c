#include "loom/capture.h"
#include <stdio.h>

/* Store the original input function */
static err_t (*original_input_fn)(struct pbuf *p, struct netif *inp) = NULL;

/* Packet statistics */
static capture_stats_t stats = {0};

/**
 * Our packet interception hook
 * This function is called for every incoming packet
 */
static err_t capture_input_hook(struct pbuf *p, struct netif *inp)
{
    if (p == NULL) {
        return ERR_OK;
    }

    /* Update statistics */
    stats.total_packets++;
    stats.total_bytes += p->tot_len;

    printf("[CAPTURE] Packet #%llu: len=%u bytes on netif=%p\n",
           (unsigned long long)stats.total_packets,
           (unsigned)p->tot_len,
           (void*)inp);

    /* TODO: Add packet processing logic here */
    /* For now, just pass everything through */

    /* Forward packet to the original lwIP stack */
    return original_input_fn(p, inp);
}

int capture_hook_init(struct netif *netif)
{
    if (!netif) {
        printf("[CAPTURE] ERROR: netif is NULL\n");
        return -1;
    }

    if (!netif->input) {
        printf("[CAPTURE] ERROR: netif->input is NULL\n");
        return -1;
    }

    /* Save the original input function */
    original_input_fn = netif->input;

    /* Install our hook */
    netif->input = capture_input_hook;

    printf("[CAPTURE] Packet capture hook installed on %c%c%d\n",
           netif->name[0], netif->name[1], netif->num);

    return 0;
}

void capture_print_stats(void)
{
    printf("\n=== Capture Statistics ===\n");
    printf("Total Packets: %llu\n", (unsigned long long)stats.total_packets);
    printf("Total Bytes:   %llu\n", (unsigned long long)stats.total_bytes);
    printf("=========================\n\n");
}

capture_stats_t capture_get_stats() {
    return stats;
}