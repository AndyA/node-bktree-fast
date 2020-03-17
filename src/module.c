#include <node_api.h>
#include <stdio.h>

#include "bktree.h"

typedef struct cb_context {
  napi_env env;
  napi_value callback;
  const bk_tree *tree;
} cb_context;

static void export_function(napi_env env, napi_value exports, const char *name,
                            napi_value(*func)(napi_env env, napi_callback_info info)) {
  napi_status status;
  napi_value fn;

  status = napi_create_function(env, NULL, 0, func, NULL, &fn);
  if (status != napi_ok)
    napi_throw_error(env, NULL, "Unable to wrap native function");

  status = napi_set_named_property(env, exports, name, fn);
  if (status != napi_ok)
    napi_throw_error(env, NULL, "Unable to populate exports");
}

static bk_key *get_key(napi_env env, const bk_tree *tree, napi_value arg, bk_key *key) {
  char buf[bk_hex_len(tree) + 1];
  size_t len;

  napi_status status = napi_get_value_string_latin1(env, arg, buf, bk_hex_len(tree) + 1, &len);

  if (status == napi_ok && len == bk_hex_len(tree) && !bk_hex2key(tree, buf, key))
    return key;

  napi_throw_error(env, NULL, "Can't parse hash");
  return NULL;
}

static bk_tree *get_tree(napi_env env, napi_value arg) {
  void *tree;
  napi_status status = napi_get_value_external(env, arg, &tree);
  if (status != napi_ok)
    napi_throw_error(env, NULL, "Can't get tree from arg");
  return (bk_tree *) tree;
}

static int32_t get_int32(napi_env env, napi_value arg) {
  int32_t res;
  napi_status status = napi_get_value_int32(env, arg, &res);
  if (status != napi_ok)
    napi_throw_error(env, NULL, "Can't get int from arg");
  return res;
}

static napi_value make_key_string(napi_env env, const bk_tree *tree, const bk_key *key) {
  char buf[bk_hex_len(tree) + 1];
  napi_value str;
  bk_key2hex(tree, key, buf);
  napi_status status = napi_create_string_latin1(env, buf, bk_hex_len(tree), &str);
  if (status != napi_ok)
    napi_throw_error(env, NULL, "Failed to create key string");
  return str;
}

static napi_value make_unsigned(napi_env env, unsigned v) {
  napi_value res;
  napi_status status = napi_create_int32(env, v, &res);
  if (status != napi_ok)
    napi_throw_error(env, NULL, "Failed to create int");

  return res;
}

static void fetch_args(napi_env env, napi_callback_info info,
                       napi_value *argv, size_t argc) {
  size_t argc_got = argc;
  napi_status status = napi_get_cb_info(env, info, &argc_got, argv, NULL, NULL);
  if (status != napi_ok || argc_got != argc)
    napi_throw_error(env, NULL, "Failed to parse arguments");
}

static napi_value _bk_distance(napi_env env, napi_callback_info info) {
  napi_value argv[3];
  fetch_args(env, info, argv, 3);
  bk_tree *tree = get_tree(env, argv[0]);

  bk_key a[bk_u64_len(tree)], b[bk_u64_len(tree)];

  get_key(env, tree, argv[1], a);
  get_key(env, tree, argv[2], b);

  return make_unsigned(env, bk_distance(tree, a, b));
}

static void _bk_free(napi_env env, void *data, void *hint) {
  (void) env;
  (void) hint;
  bk_free((bk_tree *) data);
}

static napi_value _bk_create(napi_env env, napi_callback_info info) {
  napi_value argv[1], res;

  fetch_args(env, info, argv, 1);
  size_t key_bits = get_int32(env, argv[0]);
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
  fetch_args(env, info, argv, 2);

  bk_tree *tree = get_tree(env, argv[0]);
  bk_key key[bk_u64_len(tree)];
  get_key(env, tree, argv[1], key);

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
  fetch_args(env, info, argv, 2);

  bk_tree *tree = get_tree(env, argv[0]);
  cb.env = env;
  cb.callback = argv[1];
  cb.tree = tree;
  bk_walk(tree, &cb, _bk_walk_callback);
  return NULL;
}

static napi_value _bk_query(napi_env env, napi_callback_info info) {
  napi_value argv[4];
  cb_context cb;
  fetch_args(env, info, argv, 4);

  bk_tree *tree = get_tree(env, argv[0]);
  bk_key key[bk_u64_len(tree)];
  get_key(env, tree, argv[1], key);
  unsigned max_dist = (unsigned) get_int32(env, argv[2]);
  cb.env = env;
  cb.callback = argv[3];
  cb.tree = tree;
  bk_query(tree, key, max_dist, &cb, _bk_walk_callback);
  return NULL;
}

napi_value Init(napi_env env, napi_value exports) {
  export_function(env, exports, "distance", _bk_distance);

  export_function(env, exports, "create", _bk_create);

  export_function(env, exports, "add", _bk_add);
  export_function(env, exports, "walk", _bk_walk);
  export_function(env, exports, "query", _bk_query);
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
