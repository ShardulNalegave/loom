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

int capture_hook_init(struct netif *netif, uint16_t control_port);

void capture_print_stats(void);

capture_stats_t capture_get_stats();

#endif /* LOOM_CAPTURE_H */