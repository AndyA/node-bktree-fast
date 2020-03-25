"use strict";

const BKTree = require("./base");

class BKHammingTree extends BKTree {
  constructor(keyBits, opt) {
    super(opt);
    if (keyBits % 64)
      throw new Error(`Key size must be a multiple of 64, got ${keyBits}`);
    this.keyBits = keyBits;
  }

  padKey(key) {
    return key.padStart(this.keyBits / 4, "0");
  }
}

module.exports = BKHammingTree;
