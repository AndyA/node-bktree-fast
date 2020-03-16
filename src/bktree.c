/* bktree.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "bktree.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* We only use the powers of two - but hey */
/* FIXME The *2 is a workaround for the fact that max dist might be 513 */
static bk_node *pool[BK_KEY_LEN * 2];

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

static void release_deep(bk_node *node) {
  if (!node) return;
  bk_node **slot = NODE_SLOT(node);
  for (unsigned i = 0; i < node->size; i++)
    release_deep(slot[i]);

  release_node(node);
}

static void free_pool() {
  for (unsigned i = 0; i < BK_KEY_LEN; i++) {
    free_node(pool[i]);
    pool[i] = NULL;
  }
}

unsigned bk_distance(const bk_tree *tree, const bk_key *a, const bk_key *b) {
  unsigned dist = 0;
  for (unsigned i = 0; i < BK_U64_LEN; i++)
    dist += __builtin_popcountll(a->key[i] ^ b->key[i]);
  return dist;
}

static bk_node *add(bk_tree *tree, bk_node *node, const bk_key *key) {
  if (!node) {
    node = get_node(1);
    memcpy(&node->key, key, sizeof(bk_key));
    return node;
  }

  unsigned dist = bk_distance(tree, &node->key, key);
  if (dist == 0) return node; // exact?

  if (dist >= node->size) {
    bk_node *new = get_node(get_size(dist + 1));
    new->key = node->key;
    memcpy(NODE_SLOT(new), NODE_SLOT(node), sizeof(bk_node *) * node->size);
    release_node(node);
    node = new;
  }

  bk_node **slot = NODE_SLOT(node);
  slot[dist] = add(tree, slot[dist], key);
  return node;
}

void bk_add(bk_tree *tree, const bk_key *key) {
  tree->root = add(tree, tree->root, key);
}

static void walk(const bk_tree *tree, const bk_node *node, void *ctx, unsigned depth,
                 void (*callback)(const bk_key *key, unsigned depth, void *ctx)) {
  bk_node **slot = NODE_SLOT(node);
  callback(&node->key, depth, ctx);
  for (unsigned i = 0; i < node->size; i++)
    if (slot[i]) walk(tree, slot[i], ctx, depth + 1, callback);
}

void bk_walk(const bk_tree *tree, void *ctx,
             void (*callback)(const bk_key *key, unsigned depth, void *ctx)) {
  walk(tree, tree->root, ctx, 0, callback);
}

static void query(const bk_tree *tree, const bk_node *node,
                  const bk_key *key, unsigned max_dist, void *ctx,
                  void (*callback)(const bk_key *key, unsigned distance, void *ctx)) {
  unsigned dist = bk_distance(tree, &node->key, key);

  if (dist <= max_dist)
    callback(&node->key, dist, ctx);

  int min = MAX(0, (int) dist - (int) max_dist);
  int max = MIN(dist + max_dist, node->size - 1);;

  bk_node **slot = NODE_SLOT(node);

  for (int i = min; i <= max; i++)
    if (slot[i])
      query(tree, slot[i], key, max_dist, ctx, callback);
}

void bk_query(const bk_tree *tree, const bk_key *key, unsigned max_dist, void *ctx,
              void (*callback)(const bk_key *key, unsigned distance, void *ctx)) {
  query(tree, tree->root, key, max_dist, ctx, callback);
}

bk_tree *bk_new(void) {
  return calloc(1, sizeof(bk_tree));
}

void bk_free(bk_tree *tree) {
  if (tree) {
    release_deep(tree->root);
    free(tree);
    free_pool();
  }
}

const char *bk_key2hex(const bk_tree *tree, const bk_key *key, char *buf) {
  char *out = buf;
  for (unsigned i = 0; i < BK_U64_LEN; i++) {
    sprintf(out, "%016" PRIx64, key->key[i]);
    out += 16;
  }
  return buf;
}

int bk_hex2key(const bk_tree *tree, const char *hex, bk_key *key) {
  char buf[17], *end;
  buf[16] = '\0';
  for (unsigned i = 0; i < BK_U64_LEN; i++) {
    memcpy(buf, hex + 16 * i, 16);
    key->key[i] = (uint64_t) strtoull(buf, &end, 16);
    if (end != buf + 16) return 1;
  }

  return 0;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
