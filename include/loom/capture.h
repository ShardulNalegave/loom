
#ifndef LOOM_CAPTURE_H
#define LOOM_CAPTURE_H

#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/err.h"

typedef struct {
    uint64_t total_packets;
    uint64_t total_bytes;
} capture_stats_t;

/**
 * Initialize packet capture hook on the given network interface
 * 
 * @param netif  The network interface to hook into
 * @return 0 on success, -1 on failure
 */
int capture_hook_init(struct netif *netif);

/**
 * Print packet statistics
 */
void capture_print_stats(void);

/**
 * Get packet statistics
 */
capture_stats_t capture_get_stats();

#endif /* LOOM_CAPTURE_H */