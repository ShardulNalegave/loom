#ifndef LOOM_NF_CHAIN_H
#define LOOM_NF_CHAIN_H

#include "lwip/pbuf.h"
#include <stdbool.h>
#include <stdint.h>

/* NF function pointer type */
typedef bool (*nf_func_t)(struct pbuf *p);

/* NF chain node */
typedef struct nf_node {
    const char *name;
    nf_func_t func;
    bool enabled;
    struct nf_node *next;
} nf_node_t;

/**
 * Initialize the NF chain
 */
void nf_chain_init(void);

/**
 * Process a packet through the NF chain
 * 
 * @param p  The packet buffer
 * @return true to allow packet, false to drop
 */
bool nf_chain_process(struct pbuf *p);

/**
 * Add an NF to the end of the chain
 */
int nf_chain_add(const char *name, nf_func_t func);

/**
 * Remove an NF from the chain by name
 */
int nf_chain_remove(const char *name);

/**
 * Enable/disable an NF without removing it
 */
int nf_chain_set_enabled(const char *name, bool enabled);

/**
 * List all NFs in the chain (prints to console)
 */
void nf_chain_list(void);

/**
 * Clear all NFs from the chain
 */
void nf_chain_clear(void);

/* Individual NF functions */
bool nf_rate_limiter(struct pbuf *p);
bool nf_allowlist(struct pbuf *p);

/* Rate limiter configuration */
int nf_rate_limiter_set_limit(uint16_t port, uint32_t packets_per_sec);
int nf_rate_limiter_remove_limit(uint16_t port);
void nf_rate_limiter_list(void);

/* Allowlist configuration */
int nf_allowlist_add_port(uint16_t port);
int nf_allowlist_remove_port(uint16_t port);
void nf_allowlist_list(void);
void nf_allowlist_clear(void);

#endif /* LOOM_NF_CHAIN_H */