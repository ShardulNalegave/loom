#ifndef LOOM_CAPTURE_H
#define LOOM_CAPTURE_H

#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/err.h"
#include <stdbool.h>

typedef struct {
    uint64_t total_packets;
    uint64_t total_bytes;
    uint64_t passed_packets;
    uint64_t dropped_packets;
} capture_stats_t;

/**
 * Initialize packet capture hook on the given network interface
 * 
 * @param netif  The network interface to hook into
 * @param control_port  Port number for control server (packets to this port bypass NF chain)
 * @return 0 on success, -1 on failure
 */
int capture_hook_init(struct netif *netif, uint16_t control_port);

/**
 * Print packet statistics
 */
void capture_print_stats(void);

/**
 * Get packet statistics
 */
capture_stats_t capture_get_stats();

#endif /* LOOM_CAPTURE_H */