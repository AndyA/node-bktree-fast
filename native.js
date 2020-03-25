"use strict";

const bkt = require("./build/Release/bkapi");
const _ = require("lodash");
const BKHammingTree = require("./hamming");

class BKHammingTreeNative extends BKHammingTree {
  constructor(keyBits, opt) {
    super(keyBits, opt);
    this.tree = bkt.create(keyBits);
  }

  get size() {
    return bkt.size(this.tree);
  }

  distance(a, b) {
    return bkt.distance(this.tree, a, b);
  }

  add(...keys) {
    for (const key of _.flatten(keys)) bkt.add(this.tree, this.padKey(key));
    return this;
  }

  walk(cb) {
    bkt.walk(this.tree, cb);
    return this;
  }

  query(key, maxDist, cb) {
    bkt.query(this.tree, this.padKey(key), maxDist, cb);
    return this;
  }
}

module.exports = BKHammingTreeNative;
