/* bktree.h */

#ifndef BKTREE_H_
#define BKTREE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BK_KEY_LEN 512
#define BK_HEX_LEN (BK_KEY_LEN/4)
#define BK_U64_LEN (BK_KEY_LEN/64)

typedef struct bk_key {
  uint64_t key[BK_U64_LEN];
} bk_key;

typedef struct bk_node {
  bk_key key;
  unsigned size; /* number of slots in this node */
  struct bk_node *next; /* in free pool */
  /* |size| child pointers follow */
} bk_node;

typedef struct bk_tree {
  bk_node *root;
} bk_tree;

int bk_hex2key(const char *hex, bk_key *key);
const char *bk_key2hex(const bk_key *key, char *buf);

unsigned bk_distance(const bk_key *a, const bk_key *b);

bk_node *bk_add(bk_node *node, const bk_key *key);
void bk_free_pool();
void bk_release(bk_node *node);

void bk_walk(const bk_node *node, void *ctx,
             void (*callback)(const bk_key *key, unsigned depth, void *ctx));

void bk_query(const bk_node *node, const bk_key *key,
              unsigned max_dist, void *ctx,
              void (*callback)(const bk_key *key, unsigned distance, void *ctx));

void bk_dump_key(const bk_key *key);
void bk_dump(const bk_node *node, unsigned depth);

bk_tree *bk_new(void);
void bk_free(bk_tree *tree);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
