#ifndef LOOM_NF_CHAIN_H
#define LOOM_NF_CHAIN_H

#include "lwip/pbuf.h"
#include <stdbool.h>
#include <stdint.h>

typedef bool (*nf_func_t)(struct pbuf *p);

typedef struct nf_node {
    const char *name;
    nf_func_t func;
    bool enabled;
    struct nf_node *next;
} nf_node_t;

void nf_chain_init(void);

bool nf_chain_process(struct pbuf *p);

int nf_chain_add(const char *name, nf_func_t func);

int nf_chain_remove(const char *name);

int nf_chain_set_enabled(const char *name, bool enabled);

void nf_chain_list(void);

void nf_chain_clear(void);

bool nf_rate_limiter(struct pbuf *p);
bool nf_allowlist(struct pbuf *p);

int nf_rate_limiter_set_limit(uint16_t port, uint32_t packets_per_sec);
int nf_rate_limiter_remove_limit(uint16_t port);
void nf_rate_limiter_list(void);

int nf_allowlist_add_port(uint16_t port);
int nf_allowlist_remove_port(uint16_t port);
void nf_allowlist_list(void);
void nf_allowlist_clear(void);

#endif /* LOOM_NF_CHAIN_H */