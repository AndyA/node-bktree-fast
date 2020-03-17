/* bktree.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "bktree.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define NODE_PAYLOAD_SIZE(tree, size) \
  (MAX(sizeof(bk_node *), bk_byte_len(tree) + sizeof(bk_node *) * size))
#define NODE_SIZE(tree, size) \
  (sizeof(bk_node) + NODE_PAYLOAD_SIZE(tree, size))
#define NODE_KEY(tree, node) ((bk_key *) ((bk_node *) (node))+1)
#define NODE_NEXT(tree, node) ((bk_node **) NODE_KEY(tree, node))
#define NODE_SLOT(tree, node) ((bk_node **) (NODE_KEY(tree, node) + bk_u64_len(tree)))

static unsigned alloc_size(const bk_tree *tree, unsigned size) {
  unsigned actual = 1;
  while (actual < size) actual <<= 1;
  return MIN(actual, tree->key_bits);
}

static bk_node *alloc_node(bk_tree *tree, unsigned size) {
  size_t sz = NODE_SIZE(tree, size);
  bk_node *node = calloc(1, sz);
  if (!node) return NULL;
  node->size = size;
  return node;
}

static bk_node *get_node(bk_tree *tree, unsigned size) {
  if (tree->pool[size]) {
    bk_node *nd = tree->pool[size];
    tree->pool[size] = *NODE_NEXT(tree, nd);
    memset(NODE_SLOT(tree, nd), 0, sizeof(bk_node *) * size);
    *NODE_NEXT(tree, nd) = NULL;
    return nd;
  }
  return alloc_node(tree, size);
}

static void free_node(bk_tree *tree, bk_node *node) {
  if (node) {
    free_node(tree, *NODE_NEXT(tree, node));
    free(node);
  }
}

static void free_pool(bk_tree *tree) {
  for (unsigned i = 0; i <= bk_key_len(tree); i++) {
    free_node(tree, tree->pool[i]);
    tree->pool[i] = NULL;
  }
}

static void release_node(bk_tree *tree, bk_node *node) {
  *NODE_NEXT(tree, node) = tree->pool[node->size];
  tree->pool[node->size] = node;
}

static void release_deep(bk_tree *tree, bk_node *node) {
  if (!node) return;
  bk_node **slot = NODE_SLOT(tree, node);
  for (unsigned i = 0; i < node->size; i++)
    release_deep(tree, slot[i]);

  release_node(tree, node);
}

void bk_free(bk_tree *tree) {
  if (tree) {
    release_deep(tree, tree->root);
    free_pool(tree);
    free(tree);
  }
}

bk_tree *bk_new(size_t key_bits) {
  bk_tree *tree = calloc(1, sizeof(bk_tree));
  if (!tree) return NULL;
  tree->key_bits = key_bits;
  tree->pool = calloc(sizeof(bk_node *), key_bits + 1);
  if (!tree->pool) {
    free(tree);
    return NULL;
  }
  return tree;
}

unsigned bk_distance(const bk_tree *tree, const bk_key *a, const bk_key *b) {
  unsigned dist = 0;
  unsigned len = bk_u64_len(tree);
  for (unsigned i = 0; i < len; i++) {
    uint64_t av = a[i];
    uint64_t bv = b[i];
    if (av != bv) dist += __builtin_popcountll(a[i] ^ b[i]);
  }
  return dist;
}

static bk_node *add(bk_tree *tree, bk_node *node, const bk_key *key) {
  if (!node) {
    node = get_node(tree, 0);
    if (node) memcpy(NODE_KEY(tree, node), key, bk_byte_len(tree));
    return node;
  }

  int dist = bk_distance(tree, NODE_KEY(tree, node), key) - 1;
  if (dist < 0) {
    tree->size--;
    return node; // exact?
  }

  if (dist >= (int) node->size) {
    bk_node *new = get_node(tree, alloc_size(tree, dist + 1));
    if (!new) return NULL;
    memcpy(NODE_KEY(tree, new), NODE_KEY(tree, node), NODE_PAYLOAD_SIZE(tree, node->size));
    release_node(tree, node);
    node = new;
  }

  bk_node **slot = NODE_SLOT(tree, node);
  bk_node *new = add(tree, slot[dist], key);
  if (!new) return NULL;
  slot[dist] = new;
  return node;
}

int bk_add(bk_tree *tree, const bk_key *key) {
  bk_node *new = add(tree, tree->root, key);
  if (!new) return 1;
  tree->root = new;
  tree->size++;
  return 0;
}

static void walk(const bk_tree *tree, const bk_node *node, void *ctx, unsigned depth,
                 void (*callback)(const bk_key *key, unsigned depth, void *ctx)) {
  bk_node **slot = NODE_SLOT(tree, node);
  callback(NODE_KEY(tree, node), depth, ctx);
  for (unsigned i = 0; i < node->size; i++)
    if (slot[i]) walk(tree, slot[i], ctx, depth + 1, callback);
}

void bk_walk(const bk_tree *tree, void *ctx,
             void (*callback)(const bk_key *key, unsigned depth, void *ctx)) {
  if (tree->root)
    walk(tree, tree->root, ctx, 0, callback);
}

static void query(const bk_tree *tree, const bk_node *node,
                  const bk_key *key, unsigned max_dist, void *ctx,
                  void (*callback)(const bk_key *key, unsigned distance, void *ctx)) {
  unsigned dist = bk_distance(tree, NODE_KEY(tree, node), key);

  if (dist <= max_dist) {
    callback(NODE_KEY(tree, node), dist, ctx);
    if (max_dist == 0) return;
  }

  int min = MAX(0, (int) dist - (int) max_dist - 1);
  int max = MIN(dist + max_dist + 1, node->size);

  bk_node **slot = NODE_SLOT(tree, node);

  for (int i = min; i < max; i++)
    if (slot[i])
      query(tree, slot[i], key, max_dist, ctx, callback);
}

void bk_query(const bk_tree *tree, const bk_key *key, unsigned max_dist, void *ctx,
              void (*callback)(const bk_key *key, unsigned distance, void *ctx)) {
  if (tree->root)
    query(tree, tree->root, key, max_dist, ctx, callback);
}

const char *bk_key2hex(const bk_tree *tree, const bk_key *key, char *buf) {
  char *out = buf;
  unsigned len = bk_u64_len(tree);
  for (unsigned i = 0; i < len; i++) {
    sprintf(out, "%016" PRIx64, key[i]);
    out += 16;
  }
  return buf;
}

int bk_hex2key(const bk_tree *tree, const char *hex, bk_key *key) {
  char buf[17], *end;
  buf[16] = '\0';
  unsigned len = bk_u64_len(tree);
  for (unsigned i = 0; i < len; i++) {
    memcpy(buf, hex + 16 * i, 16);
    key[i] = (uint64_t) strtoull(buf, &end, 16);
    if (end != buf + 16) return 1;
  }

  return 0;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
