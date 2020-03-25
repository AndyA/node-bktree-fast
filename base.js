"use strict";

const _ = require("lodash");

class BKTree {
  constructor(opt) {
    this.opt = Object.assign({}, opt || {});
    this.tree = null;
    this._size = 0;
  }

  get size() {
    return this._size;
  }

  distance(a, b) {
    const { distance } = this.opt;
    if (distance) return distance(a, b);
    throw new Error("No distance function");
  }

  add(...keys) {
    const _add = (node, key) => {
      if (!node) {
        this._size++;
        return { key, children: [] };
      }
      const dist = this.distance(node.key, key) - 1;
      if (dist < 0) return node; // Already got it
      node.children[dist] = _add(node.children[dist], key);
      return node;
    };

    this.tree = _.flatten(keys).reduce(
      (node, key) => _add(node, key),
      this.tree
    );

    return this;
  }

  query(key, maxDist, cb) {
    const _query = node => {
      const dist = this.distance(node.key, key);
      if (dist <= maxDist) {
        cb(node.key, dist);
        if (maxDist === 0) return;
      }
      const min = Math.max(1, dist - maxDist);
      const max = dist + maxDist;
      node.children
        .slice(min - 1, max)
        .filter(Boolean)
        .map(_query);
    };
    if (this.tree) _query(this.tree);
    return this;
  }

  walk(cb) {
    const _walk = (node, depth) => {
      cb(node.key, depth);
      node.children.filter(Boolean).map(child => _walk(child, depth + 1));
    };
    if (this.tree) _walk(this.tree, 0);
    return this;
  }

  find(key, maxDist) {
    const found = [];
    this.query(key, maxDist, (key, distance) => found.push({ key, distance }));
    return found.sort((a, b) => a.distance - b.distance);
  }

  has(key) {
    return this.find(key, 0).length > 0;
  }
}

module.exports = BKTree;
