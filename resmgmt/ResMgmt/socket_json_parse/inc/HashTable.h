#ifndef _HASH_TABLE_H
#define _HASH_TABLE_H

#include "dealsoc_types.h"

#define HASH_TABLE_SIZE  8192  /* 2的幂, 便于位运算取模; ~15K条目的每链~2节点 */

typedef struct hash_node {
	char              key[MAX_CH_NAME_LEN];
	hash_val_t        val;
	struct hash_node *next;
} hash_node_t;

typedef struct {
	hash_node_t *buckets[HASH_TABLE_SIZE];
} hash_table_t;

/* FNV-1a hash */
RES_API uint32_t hash_fnv1a(const char *str);

RES_API void     hash_init(hash_table_t *ht);
RES_API void     hash_insert(hash_table_t *ht, const char *key, uint32_t outer_idx, uint32_t dut_no);
RES_API int      hash_find(hash_table_t *ht, const char *key, uint32_t *outer_idx, uint32_t *dut_no);
RES_API void     hash_destroy(hash_table_t *ht);

#endif /* _HASH_TABLE_H */
