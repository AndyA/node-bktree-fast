#include <node_api.h>
#include <stdio.h>

#include "bktree.h"

typedef struct cb_context {
  napi_env env;
  napi_value callback;
} cb_context;

static void install_function(napi_env env, napi_value exports, const char *name,
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

static bk_key get_key(napi_env env, napi_value arg) {
  char buf[BK_HEX_LEN + 1];
  size_t len;
  bk_key key;

  napi_status status = napi_get_value_string_latin1(env, arg, buf, BK_HEX_LEN + 1, &len);

  if (status == napi_ok && len == BK_HEX_LEN && !bk_hex2key(buf, &key))
    return key;

  napi_throw_error(env, NULL, "Can't parse hash");
  return key;
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

static napi_value make_key_string(napi_env env, const bk_key *key) {
  char buf[BK_HEX_LEN + 1];
  napi_value str;
  bk_key2hex(key, buf);
  napi_status status = napi_create_string_latin1(env, buf, BK_HEX_LEN, &str);
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
  napi_value argv[2];

  fetch_args(env, info, argv, 2);

  bk_key a = get_key(env, argv[0]);
  bk_key b = get_key(env, argv[1]);

  return make_unsigned(env, bk_distance(&a, &b));
}

static void _bk_free(napi_env env, void *data, void *hint) {
  (void) env;
  (void) hint;
  bk_free((bk_tree *) data);
}

static napi_value _bk_create(napi_env env, napi_callback_info info) {
  napi_value res;
  bk_tree *tree = bk_new();
  if (tree) {
    napi_status status = napi_create_external(env, tree, _bk_free, NULL, &res);
    if (status == napi_ok)
      return res;
  }

  napi_throw_error(env, NULL, "Can't create tree");
  return res;
}

static napi_value _bk_add(napi_env env, napi_callback_info info) {
  napi_value argv[2];
  fetch_args(env, info, argv, 2);

  bk_tree *tree = get_tree(env, argv[0]);
  bk_key key = get_key(env, argv[1]);

  tree->root = bk_add(tree->root, &key);
  return argv[0];
}

static void _bk_walk_callback(const bk_key *key, unsigned dist, void *ctx) {
  cb_context *cb = (cb_context *) ctx;
  napi_value argv[2], res;

  argv[0] = make_key_string(cb->env, key);
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
  bk_walk(tree->root, &cb, _bk_walk_callback);
  return argv[0];
}

static napi_value _bk_query(napi_env env, napi_callback_info info) {
  napi_value argv[4];
  cb_context cb;
  fetch_args(env, info, argv, 4);

  bk_tree *tree = get_tree(env, argv[0]);
  bk_key key = get_key(env, argv[1]);
  unsigned max_dist = (unsigned) get_int32(env, argv[2]);
  cb.env = env;
  cb.callback = argv[3];
  bk_query(tree->root, &key, max_dist, &cb, _bk_walk_callback);
  return argv[0];
}

napi_value Init(napi_env env, napi_value exports) {
  install_function(env, exports, "distance", _bk_distance);
  install_function(env, exports, "create", _bk_create);
  install_function(env, exports, "add", _bk_add);
  install_function(env, exports, "walk", _bk_walk);
  install_function(env, exports, "query", _bk_query);
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
