"use strict";

const bkt = require("./build/Release/module");

class BKTree {
  constructor() {
    this.tree = bkt.create();
  }

  static distance(a, b) {
    return bkt.distance(a, b);
  }

  add(key) {
    bkt.add(this.tree, key);
    return this;
  }

  walk(cb) {
    bkt.walk(this.tree, cb);
  }

  query(key, maxDist, cb) {
    bkt.query(this.tree, key, maxDist, cb);
  }
}

module.exports = BKTree;
