"use strict";

const bkt = require("./build/Release/module");
const _ = require("lodash");

class BKTree {
  constructor(keyBits) {
    if (keyBits % 64)
      throw new Error(`Key size must be a multiple of 64, got ${keyBits}`);
    this.keyBits = keyBits;
    this.tree = bkt.create(keyBits);
  }

  get size() {
    return bkt.size(this.tree);
  }

  padKey(key) {
    return key.padStart(this.keyBits / 4, "0");
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

  find(key, maxDist) {
    const found = [];
    this.query(key, maxDist, (hash, distance) =>
      found.push({ hash, distance })
    );
    return found.sort((a, b) => a.distance - b.distance);
  }

  has(key) {
    let found = false;
    this.query(key, 0, () => (found = true));
    return found;
  }
}

module.exports = BKTree;
