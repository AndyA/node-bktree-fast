#include <node_api.h>
#include <stdio.h>

#include "bktree.h"

typedef struct cb_context {
  napi_env env;
  napi_value callback;
  const bk_tree *tree;
} cb_context;

static int export_function(napi_env env, napi_value exports, const char *name,
                           napi_value(*func)(napi_env env, napi_callback_info info)) {
  napi_value fn;

  if (napi_ok != napi_create_function(env, NULL, 0, func, NULL, &fn) ||
      napi_ok != napi_set_named_property(env, exports, name, fn)) {
    napi_throw_error(env, NULL, "Unable to export function");
    return 1;
  }

  return 0;
}

static int fetch_args(napi_env env, napi_callback_info info,
                      napi_value *argv, size_t argc) {
  size_t argc_got = argc;
  napi_status status = napi_get_cb_info(env, info, &argc_got, argv, NULL, NULL);

  if (status != napi_ok || argc_got != argc) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return 1;
  }

  return 0;
}

static bk_key *get_key(napi_env env, const bk_tree *tree, napi_value arg, bk_key *key) {
  char buf[bk_hex_len(tree) + 1];
  size_t len;

  if (napi_ok == napi_get_value_string_latin1(env, arg, buf, bk_hex_len(tree) + 1, &len) &&
      len == bk_hex_len(tree) && !bk_hex2key(tree, buf, key))
    return key;

  napi_throw_error(env, NULL, "Can't parse hash");
  return NULL;
}

static bk_tree *get_tree(napi_env env, napi_value arg) {
  void *tree = NULL;

  if (napi_ok != napi_get_value_external(env, arg, &tree))
    napi_throw_error(env, NULL, "Can't get tree from arg");

  return (bk_tree *) tree;
}

static int get_int32(napi_env env, napi_value arg, int32_t *out) {
  if (napi_ok != napi_get_value_int32(env, arg, out)) {
    napi_throw_error(env, NULL, "Can't get int from arg");
    return 1;
  }

  return 0;
}

static napi_value make_key_string(napi_env env, const bk_tree *tree, const bk_key *key) {
  char buf[bk_hex_len(tree) + 1];
  napi_value str = NULL;

  bk_key2hex(tree, key, buf);
  if (napi_ok != napi_create_string_latin1(env, buf, bk_hex_len(tree), &str))
    napi_throw_error(env, NULL, "Failed to create key string");

  return str;
}

static napi_value make_unsigned(napi_env env, unsigned v) {
  napi_value res = NULL;

  if (napi_ok != napi_create_int32(env, v, &res))
    napi_throw_error(env, NULL, "Failed to create int");

  return res;
}

static napi_value _bk_distance(napi_env env, napi_callback_info info) {
  napi_value argv[3];

  if (fetch_args(env, info, argv, 3)) return NULL;

  bk_tree *tree = get_tree(env, argv[0]);
  if (!tree) return NULL;

  bk_key a[bk_u64_len(tree)], b[bk_u64_len(tree)];

  if (get_key(env, tree, argv[1], a) && get_key(env, tree, argv[2], b))
    return make_unsigned(env, bk_distance(tree, a, b));

  return NULL;
}

static void _bk_free(napi_env env, void *data, void *hint) {
  (void) env;
  (void) hint;
  bk_free((bk_tree *) data);
}

static napi_value _bk_create(napi_env env, napi_callback_info info) {
  napi_value argv[1], res;
  int32_t key_bits;

  if (fetch_args(env, info, argv, 1)) return NULL;
  if (get_int32(env, argv[0], &key_bits)) return NULL;

  bk_tree *tree = bk_new(key_bits);
  if (tree) {
    napi_status status = napi_create_external(env, tree, _bk_free, NULL, &res);
    if (status == napi_ok) return res;
  }

  napi_throw_error(env, NULL, "Can't create tree");
  return NULL;
}

static napi_value _bk_add(napi_env env, napi_callback_info info) {
  napi_value argv[2];
  if (fetch_args(env, info, argv, 2)) return NULL;

  bk_tree *tree = get_tree(env, argv[0]);
  if (!tree) return NULL;

  bk_key key[bk_u64_len(tree)];

  if (!get_key(env, tree, argv[1], key)) return NULL;

  if (bk_add(tree, key))
    napi_throw_error(env, NULL, "Can't add node");

  return NULL;
}

static void _bk_walk_callback(const bk_key *key, unsigned dist, void *ctx) {
  cb_context *cb = (cb_context *) ctx;
  napi_value argv[2], res;

  argv[0] = make_key_string(cb->env, cb->tree, key);
  argv[1] = make_unsigned(cb->env, dist);
  napi_status status = napi_call_function(cb->env, cb->callback, cb->callback, 2, argv, &res);

  if (status != napi_ok)
    napi_throw_error(cb->env, NULL, "Failed to invoke callback");
}

static napi_value _bk_walk(napi_env env, napi_callback_info info) {
  napi_value argv[2];
  cb_context cb;

  if (fetch_args(env, info, argv, 2)) return NULL;;

  bk_tree *tree = get_tree(env, argv[0]);
  if (!tree) return NULL;

  cb.env = env;
  cb.callback = argv[1];
  cb.tree = tree;

  bk_walk(tree, &cb, _bk_walk_callback);
  return NULL;
}

static napi_value _bk_query(napi_env env, napi_callback_info info) {
  napi_value argv[4];
  int32_t max_dist;
  cb_context cb;

  if (fetch_args(env, info, argv, 4)) return NULL;

  bk_tree *tree = get_tree(env, argv[0]);
  if (!tree) return NULL;

  bk_key key[bk_u64_len(tree)];

  if (!get_key(env, tree, argv[1], key)) return NULL;
  if (get_int32(env, argv[2], &max_dist)) return NULL;

  cb.env = env;
  cb.callback = argv[3];
  cb.tree = tree;

  bk_query(tree, key, max_dist, &cb, _bk_walk_callback);
  return NULL;
}

napi_value Init(napi_env env, napi_value exports) {
  if (export_function(env, exports, "distance", _bk_distance) ||
      export_function(env, exports, "create", _bk_create) ||
      export_function(env, exports, "add", _bk_add) ||
      export_function(env, exports, "walk", _bk_walk) ||
      export_function(env, exports, "query", _bk_query))
    return NULL;
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
