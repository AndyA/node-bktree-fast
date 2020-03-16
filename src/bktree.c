/* bktree.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "bktree.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* We only use the powers of two - but hey */
static bk_node *pool[BK_KEY_LEN];

#define NODE_SIZE(size) (sizeof(bk_node) + sizeof(bk_node *) * size)
#define NODE_SLOT(node) ((bk_node **) ((node)+1))

static bk_node *alloc_node(unsigned size) {
  size_t sz = NODE_SIZE(size);
  bk_node *node = calloc(1, sz);
  node->size = size;
  return node;
}

static void free_node(bk_node *node) {
  if (node) {
    free_node(node->next);
    free(node);
  }
}

static bk_node *get_node(unsigned size) {
  if (pool[size]) {
    bk_node *nd = pool[size];
    pool[size] = nd->next;
    memset(NODE_SLOT(nd), 0,  sizeof(bk_node *) * size);
    nd->next = NULL;
    return nd;
  }
  return alloc_node(size);
}

static void release_node(bk_node *node) {
  node->next = pool[node->size];
  pool[node->size] = node;
}

static unsigned get_size(unsigned size) {
  unsigned actual = 1;
  while (actual < size) actual <<= 1;
  return actual;
}

unsigned bk_distance(const bk_key *a, const bk_key *b) {
  unsigned dist = 0;
  for (unsigned i = 0; i < BK_U64_LEN; i++)
    dist += __builtin_popcountll(a->key[i] ^ b->key[i]);
  return dist;
}

bk_node *bk_add(bk_node *node, const bk_key *key) {
  if (!node) {
    node = get_node(1);
    memcpy(&node->key, key, sizeof(bk_key));
    return node;
  }

  unsigned dist = bk_distance(&node->key, key);
  if (dist == 0) return node; // exact?

  if (dist >= node->size) {
    bk_node *new = get_node(get_size(dist + 1));
    new->key = node->key;
    memcpy(NODE_SLOT(new), NODE_SLOT(node), sizeof(bk_node *) * node->size);
    release_node(node);
    node = new;
  }

  bk_node **slot = NODE_SLOT(node);
  slot[dist] = bk_add(slot[dist], key);
  return node;
}

static void walk(const bk_node *node, void *ctx, unsigned depth,
                 void (*callback)(const bk_key *key, unsigned depth, void *ctx)) {
  bk_node **slot = NODE_SLOT(node);
  callback(&node->key, depth, ctx);
  for (unsigned i = 0; i < node->size; i++)
    if (slot[i]) walk(slot[i], ctx, depth + 1, callback);
}

void bk_walk(const bk_node *node, void *ctx,
             void (*callback)(const bk_key *key, unsigned depth, void *ctx)) {
  walk(node, ctx, 0, callback);
}

void bk_query(const bk_node *node, const bk_key *key, unsigned max_dist, void *ctx,
              void (*callback)(const bk_key *key, unsigned distance, void *ctx)) {
  unsigned dist = bk_distance(&node->key, key);

  if (dist <= max_dist)
    callback(&node->key, dist, ctx);

  int min = MAX(0, (int) dist - (int) max_dist);
  int max = MIN(dist + max_dist, node->size - 1);;

  bk_node **slot = NODE_SLOT(node);

  for (int i = min; i <= max; i++)
    if (slot[i])
      bk_query(slot[i], key, max_dist, ctx, callback);
}

bk_tree *bk_new(void) {
  return calloc(1, sizeof(bk_tree));
}

void bk_free(bk_tree *tree) {
  if (tree) {
    bk_release(tree->root);
    free(tree);
    bk_free_pool();
  }
}

void bk_release(bk_node *node) {
  if (!node) return;
  bk_node **slot = NODE_SLOT(node);
  for (unsigned i = 0; i < node->size; i++)
    bk_release(slot[i]);

  release_node(node);
}

void bk_free_pool() {
  for (unsigned i = 0; i < BK_KEY_LEN; i++) {
    free_node(pool[i]);
    pool[i] = NULL;
  }
}

const char *bk_key2hex(const bk_key *key, char *buf) {
  char *out = buf;
  for (unsigned i = 0; i < BK_U64_LEN; i++) {
    sprintf(out, "%016" PRIx64, key->key[i]);
    out += 16;
  }
  return buf;
}

int bk_hex2key(const char *hex, bk_key *key) {
  char buf[17], *end;
  buf[16] = '\0';
  for (unsigned i = 0; i < BK_U64_LEN; i++) {
    memcpy(buf, hex + 16 * i, 16);
    key->key[i] = (uint64_t) strtoull(buf, &end, 16);
    if (end != buf + 16) return 1;
  }

  return 0;
}

void bk_dump_key(const bk_key *key) {
  char buf[BK_HEX_LEN + 1];
  printf("%s", bk_key2hex(key, buf));
}

void bk_dump(const bk_node *node, unsigned depth) {
  if (!node) return;
  for (unsigned i = 0; i < depth; i++) printf("  ");
  bk_dump_key(&node->key);
  printf(" %d\n", node->size);

  bk_node **slot = NODE_SLOT(node);
  for (unsigned d = 0; d < node->size; d++)
    bk_dump(slot[d], depth + 1);
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */