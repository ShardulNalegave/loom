#include "loom/nf_chain.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "lwip/prot/ethernet.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/tcp.h"

/* Head of the NF chain linked list */
static nf_node_t *chain_head = NULL;

/* ========== Rate Limiter Data Structures ========== */

#define MAX_RATE_LIMITS 32

typedef struct {
    uint16_t port;
    uint32_t limit;           // packets per second
    uint32_t count;           // current count
    time_t last_reset;        // last time we reset the counter
} rate_limit_t;

static rate_limit_t rate_limits[MAX_RATE_LIMITS];
static int num_rate_limits = 0;

/* ========== Allowlist Data Structures ========== */

#define MAX_ALLOWED_PORTS 64

static uint16_t allowed_ports[MAX_ALLOWED_PORTS];
static int num_allowed_ports = 0;

/* ========== NF Chain Functions ========== */

void nf_chain_init(void)
{
    printf("[NF_CHAIN] Initializing NF chain\n");
    chain_head = NULL;
    
    /* Initialize rate limiter */
    memset(rate_limits, 0, sizeof(rate_limits));
    num_rate_limits = 0;
    
    /* Initialize allowlist */
    memset(allowed_ports, 0, sizeof(allowed_ports));
    num_allowed_ports = 0;
    
    /* Register default NFs */
    nf_chain_add("rate_limiter", nf_rate_limiter);
    nf_chain_add("allowlist", nf_allowlist);
    
    printf("[NF_CHAIN] Default NFs registered\n");
}

bool nf_chain_process(struct pbuf *p)
{
    nf_node_t *current = chain_head;
    
    while (current != NULL) {
        if (current->enabled) {
            if (!current->func(p)) {
                printf("[NF_CHAIN] Packet dropped by NF: %s\n", current->name);
                return false;
            }
        }
        current = current->next;
    }
    
    return true;
}

int nf_chain_add(const char *name, nf_func_t func)
{
    if (!name || !func) {
        return -1;
    }
    
    nf_node_t *new_node = (nf_node_t *)malloc(sizeof(nf_node_t));
    if (!new_node) {
        printf("[NF_CHAIN] ERROR: Failed to allocate memory for NF\n");
        return -1;
    }
    
    new_node->name = name;
    new_node->func = func;
    new_node->enabled = true;
    new_node->next = NULL;
    
    if (chain_head == NULL) {
        chain_head = new_node;
    } else {
        nf_node_t *current = chain_head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }
    
    printf("[NF_CHAIN] Added NF: %s\n", name);
    return 0;
}

int nf_chain_remove(const char *name)
{
    if (!name || chain_head == NULL) {
        return -1;
    }
    
    nf_node_t *current = chain_head;
    nf_node_t *prev = NULL;
    
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            if (prev == NULL) {
                chain_head = current->next;
            } else {
                prev->next = current->next;
            }
            
            printf("[NF_CHAIN] Removed NF: %s\n", name);
            free(current);
            return 0;
        }
        prev = current;
        current = current->next;
    }
    
    printf("[NF_CHAIN] NF not found: %s\n", name);
    return -1;
}

int nf_chain_set_enabled(const char *name, bool enabled)
{
    if (!name) {
        return -1;
    }
    
    nf_node_t *current = chain_head;
    
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            current->enabled = enabled;
            printf("[NF_CHAIN] NF %s: %s\n", name, enabled ? "enabled" : "disabled");
            return 0;
        } else {
            printf("[NF_CHAIN - Testing] current nf name = %s", current->name);
        }
        current = current->next;
    }
    
    printf("[NF_CHAIN] NF not found: %s\n", name);
    return -1;
}

void nf_chain_list(void)
{
    printf("\n=== NF Chain ===\n");
    
    if (chain_head == NULL) {
        printf("(empty)\n");
    } else {
        nf_node_t *current = chain_head;
        int index = 0;
        while (current != NULL) {
            printf("[%d] %s - %s\n", 
                   index, 
                   current->name, 
                   current->enabled ? "enabled" : "disabled");
            current = current->next;
            index++;
        }
    }
    
    printf("================\n\n");
}

void nf_chain_clear(void)
{
    nf_node_t *current = chain_head;
    
    while (current != NULL) {
        nf_node_t *next = current->next;
        free(current);
        current = next;
    }
    
    chain_head = NULL;
    printf("[NF_CHAIN] Chain cleared\n");
}

/* ========== Rate Limiter Implementation ========== */

static uint16_t get_dst_port(struct pbuf *p)
{
    if (p->tot_len < sizeof(struct eth_hdr) + sizeof(struct ip_hdr)) {
        return 0;
    }

    struct eth_hdr *eth = (struct eth_hdr *)p->payload;
    uint16_t eth_type = lwip_ntohs(eth->type);
    
    if (eth_type != ETHTYPE_IP) {
        return 0;
    }

    struct ip_hdr *ip = (struct ip_hdr *)((uint8_t *)p->payload + sizeof(struct eth_hdr));
    uint8_t proto = IPH_PROTO(ip);
    
    if (proto != 6 && proto != 17) {  // TCP or UDP
        return 0;
    }

    struct tcp_hdr *tcp = (struct tcp_hdr *)((uint8_t *)ip + (IPH_HL(ip) * 4));
    return lwip_ntohs(tcp->dest);
}

bool nf_rate_limiter(struct pbuf *p)
{
    uint16_t port = get_dst_port(p);
    if (port == 0) {
        return true;  // Not TCP/UDP, allow it
    }
    
    time_t now = time(NULL);
    
    /* Check if we have a rate limit for this port */
    for (int i = 0; i < num_rate_limits; i++) {
        if (rate_limits[i].port == port) {
            /* Reset counter if a second has passed */
            if (now > rate_limits[i].last_reset) {
                rate_limits[i].count = 0;
                rate_limits[i].last_reset = now;
            }
            
            /* Check if we're over the limit */
            if (rate_limits[i].count >= rate_limits[i].limit) {
                printf("[RATE_LIMITER] Port %u exceeded limit (%u pps)\n", 
                       port, rate_limits[i].limit);
                return false;
            }
            
            rate_limits[i].count++;
            return true;
        }
    }
    
    /* No rate limit for this port - allow */
    return true;
}

int nf_rate_limiter_set_limit(uint16_t port, uint32_t packets_per_sec)
{
    /* Check if limit already exists */
    for (int i = 0; i < num_rate_limits; i++) {
        if (rate_limits[i].port == port) {
            rate_limits[i].limit = packets_per_sec;
            printf("[RATE_LIMITER] Updated port %u: %u pps\n", port, packets_per_sec);
            return 0;
        }
    }
    
    /* Add new limit */
    if (num_rate_limits >= MAX_RATE_LIMITS) {
        printf("[RATE_LIMITER] ERROR: Max limits reached\n");
        return -1;
    }
    
    rate_limits[num_rate_limits].port = port;
    rate_limits[num_rate_limits].limit = packets_per_sec;
    rate_limits[num_rate_limits].count = 0;
    rate_limits[num_rate_limits].last_reset = time(NULL);
    num_rate_limits++;
    
    printf("[RATE_LIMITER] Added port %u: %u pps\n", port, packets_per_sec);
    return 0;
}

int nf_rate_limiter_remove_limit(uint16_t port)
{
    for (int i = 0; i < num_rate_limits; i++) {
        if (rate_limits[i].port == port) {
            /* Shift remaining limits down */
            for (int j = i; j < num_rate_limits - 1; j++) {
                rate_limits[j] = rate_limits[j + 1];
            }
            num_rate_limits--;
            printf("[RATE_LIMITER] Removed limit for port %u\n", port);
            return 0;
        }
    }
    
    printf("[RATE_LIMITER] No limit found for port %u\n", port);
    return -1;
}

void nf_rate_limiter_list(void)
{
    printf("\n=== Rate Limiter ===\n");
    
    if (num_rate_limits == 0) {
        printf("(no limits configured)\n");
    } else {
        for (int i = 0; i < num_rate_limits; i++) {
            printf("Port %u: %u pps (current: %u)\n",
                   rate_limits[i].port,
                   rate_limits[i].limit,
                   rate_limits[i].count);
        }
    }
    
    printf("====================\n\n");
}

/* ========== Allowlist Implementation ========== */

bool nf_allowlist(struct pbuf *p)
{
    /* If no ports in allowlist, allow everything */
    if (num_allowed_ports == 0) {
        return true;
    }
    
    uint16_t port = get_dst_port(p);
    if (port == 0) {
        return true;  // Not TCP/UDP, allow it
    }
    
    /* Check if port is in allowlist */
    for (int i = 0; i < num_allowed_ports; i++) {
        if (allowed_ports[i] == port) {
            return true;
        }
    }
    
    printf("[ALLOWLIST] Port %u not in allowlist, dropping\n", port);
    return false;
}

int nf_allowlist_add_port(uint16_t port)
{
    /* Check if already exists */
    for (int i = 0; i < num_allowed_ports; i++) {
        if (allowed_ports[i] == port) {
            printf("[ALLOWLIST] Port %u already in allowlist\n", port);
            return 0;
        }
    }
    
    if (num_allowed_ports >= MAX_ALLOWED_PORTS) {
        printf("[ALLOWLIST] ERROR: Max ports reached\n");
        return -1;
    }
    
    allowed_ports[num_allowed_ports] = port;
    num_allowed_ports++;
    
    printf("[ALLOWLIST] Added port %u\n", port);
    return 0;
}

int nf_allowlist_remove_port(uint16_t port)
{
    for (int i = 0; i < num_allowed_ports; i++) {
        if (allowed_ports[i] == port) {
            /* Shift remaining ports down */
            for (int j = i; j < num_allowed_ports - 1; j++) {
                allowed_ports[j] = allowed_ports[j + 1];
            }
            num_allowed_ports--;
            printf("[ALLOWLIST] Removed port %u\n", port);
            return 0;
        }
    }
    
    printf("[ALLOWLIST] Port %u not in allowlist\n", port);
    return -1;
}

void nf_allowlist_list(void)
{
    printf("\n=== Allowlist ===\n");
    
    if (num_allowed_ports == 0) {
        printf("(empty - all ports allowed)\n");
    } else {
        for (int i = 0; i < num_allowed_ports; i++) {
            printf("Port %u\n", allowed_ports[i]);
        }
    }
    
    printf("=================\n\n");
}

void nf_allowlist_clear(void)
{
    num_allowed_ports = 0;
    memset(allowed_ports, 0, sizeof(allowed_ports));
    printf("[ALLOWLIST] Cleared all ports\n");
}