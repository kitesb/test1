#include <stdlib.h>
#include <string.h>

#include "HashTable.h"
#include "internal.h"
#include "log_mgmt.h"

/* FNV-1a 32-bit hash */
uint32_t hash_fnv1a(const char *str)
{
	uint32_t hash = 2166136261u;
	const char *p;

	for (p = str; *p != '\0'; p++) {
		hash ^= (uint32_t)(unsigned char)(*p);
		hash *= 16777619u;
	}

	return hash;
}

void hash_init(hash_table_t *ht)
{
	uint32_t i;

	for (i = 0; i < HASH_TABLE_SIZE; i++) {
		ht->buckets[i] = NULL;
	}
}

void hash_insert(hash_table_t *ht, const char *key, uint32_t outer_idx, uint32_t dut_no)
{
	uint32_t idx;
	size_t key_len;
	hash_node_t *node;

	idx = hash_fnv1a(key) & (HASH_TABLE_SIZE - 1);

	node = (hash_node_t *)malloc(sizeof(hash_node_t));
	if (node == NULL) {
		DBG_ERR("hash_insert: malloc failed for key=%s\n", key);
		return;
	}

	key_len = strlen(key);
	if (key_len >= MAX_CH_NAME_LEN) {
		key_len = MAX_CH_NAME_LEN - 1;
	}
	memcpy(node->key, key, key_len);
	node->key[key_len] = '\0';
	node->val.outer_idx = outer_idx;
	node->val.dut_no    = dut_no;
	node->next = ht->buckets[idx];
	ht->buckets[idx] = node;
}

int hash_find(hash_table_t *ht, const char *key, uint32_t *outer_idx, uint32_t *dut_no)
{
	uint32_t idx;
	hash_node_t *node;

	idx = hash_fnv1a(key) & (HASH_TABLE_SIZE - 1);

	for (node = ht->buckets[idx]; node != NULL; node = node->next) {
		if (strcmp(node->key, key) == 0) {
			*outer_idx = node->val.outer_idx;
			*dut_no    = node->val.dut_no;
			return 1;
		}
	}

	return 0;
}

void hash_destroy(hash_table_t *ht)
{
	uint32_t i;

	for (i = 0; i < HASH_TABLE_SIZE; i++) {
		hash_node_t *node = ht->buckets[i];
		while (node != NULL) {
			hash_node_t *next = node->next;
			free(node);
			node = next;
		}
		ht->buckets[i] = NULL;
	}
}
