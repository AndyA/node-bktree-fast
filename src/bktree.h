/* bktree.h */

#ifndef BKTREE_H_
#define BKTREE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef uint64_t bk_key;

typedef struct bk_node {
  unsigned size; /* number of slots in this node */
  struct bk_node *next; /* in free pool */
  /* |key| follows */
  /* |size| child pointers follow */
} bk_node;

typedef struct bk_tree {
  size_t key_bits;
  bk_node *root;
  bk_node **pool;
} bk_tree;

#define bk_key_len(tree) ((tree)->key_bits)
#define bk_hex_len(tree) (bk_key_len(tree) / 4)
#define bk_byte_len(tree) (bk_key_len(tree) / 8)
#define bk_u64_len(tree) (bk_key_len(tree) / 64)

int bk_hex2key(const bk_tree *tree, const char *hex, bk_key *key);
const char *bk_key2hex(const bk_tree *tree, const bk_key *key, char *buf);

unsigned bk_distance(const bk_tree *tree, const bk_key *a, const bk_key *b);

int bk_add(bk_tree *tree, const bk_key *key);

void bk_walk(const bk_tree *tree, void *ctx,
             void (*callback)(const bk_key *key, unsigned depth, void *ctx));

void bk_query(const bk_tree *tree, const bk_key *key, unsigned max_dist, void *ctx,
              void (*callback)(const bk_key *key, unsigned distance, void *ctx));

bk_tree *bk_new(size_t key_bits);
void bk_free(bk_tree *tree);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
