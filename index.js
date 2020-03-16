"use strict";

const bkt = require("./build/Release/module");
const _ = require("lodash");

class BKTree {
  constructor() {
    this.tree = bkt.create();
  }

  distance(a, b) {
    return bkt.distance(this.tree, a, b);
  }

  add(...keys) {
    for (const key of _.flatten(keys)) bkt.add(this.tree, key);
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
