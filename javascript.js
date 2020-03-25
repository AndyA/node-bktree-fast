"use strict";

const BKHammingTree = require("./hamming");
const _ = require("lodash");

class BKHammingTreeJS extends BKHammingTree {
  constructor(keyBits, opt) {
    super(keyBits, opt);
  }

  distance(a, b) {
    const bitCount = n => {
      n = n - ((n >> 1) & 0x55555555);
      n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
      return (((n + (n >> 4)) & 0xf0f0f0f) * 0x1010101) >> 24;
    };

    if (a === b) return 0;

    let dist = 0;
    for (let i = 0; i < a.length; i += 8) {
      const va = parseInt(a.slice(i, i + 8), 16);
      const vb = parseInt(b.slice(i, i + 8), 16);
      dist += bitCount(va ^ vb);
    }
    return dist;
  }

  query(key, maxDist, cb) {
    return super.query(this.padKey(key), maxDist, cb);
  }

  add(...keys) {
    return super.add(_.flatten(keys).map(key => this.padKey(key)));
  }
}

module.exports = BKHammingTreeJS;
