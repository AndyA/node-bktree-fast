"use strict";

const chai = require("chai");
const expect = chai.expect;
const _ = require("lodash");

const BKTree = require("..");

// stride: 2 -> 32, 16 -> 8
// pad: 2 -> 32,  16 -> 8
//  2 -> 32
// 16 ->  8

function rebase(str, inBase, outBase) {
  const mapBase = b => (b === 2 ? 32 : b === 16 ? 8 : null);
  const stride = mapBase(inBase);
  const pad = mapBase(outBase);
  if (!stride) throw new Error(`Bad inBase ${inBase}`);
  if (!pad) throw new Error(`Bad outBase ${outBase}`);
  if (str.length % stride) throw new Error(`Bad string length ${str.length}`);
  const out = [];
  for (let i = 0; i < str.length; i += stride)
    out.push(
      parseInt(str.slice(i, i + stride), inBase)
        .toString(outBase)
        .padStart(pad, "0")
    );
  return out.join("");
}

class HashMaker {
  constructor(length) {
    this.length = length;
    this._dist = {};
  }

  binToHex(binHash) {
    if (binHash.length !== this.length)
      throw new Error(
        `Hash length mismatch ${this.length} != ${binHash.length}`
      );
    return rebase(binHash, 2, 16);
  }

  hexToBin(hexHash) {
    return rebase(hexHash, 16, 2);
  }

  makeBits() {
    const bits = [];
    for (let i = 0; i < this.length; i++) bits.push(i);
    return _.shuffle(bits);
  }

  makeRandom() {
    const bits = [];
    for (let i = 0; i < this.length; i++)
      bits.push(Math.random() < 0.5 ? 1 : 0);
    return bits;
  }

  get data() {
    return (this._data =
      this._data ||
      (() => {
        const bits = this.makeBits();
        const base = this.makeRandom();
        const data = [];
        for (let stride = 0; bits.length; stride++) {
          const flip = bits.splice(0, stride);
          for (const bit of flip) base[bit] = 1 - base[bit];
          data.push(this.binToHex(base.join("")));
        }
        return data;
      })());
  }

  get random() {
    const d = this.data;
    return d[Math.floor(Math.random() * d.length)];
  }

  distance(a, b) {
    const bitCount = n => {
      n = n - ((n >> 1) & 0x55555555);
      n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
      return (((n + (n >> 4)) & 0xf0f0f0f) * 0x1010101) >> 24;
    };

    if (a === b) return 0;
    if (a > b) return this.distance(b, a);
    const key = a + "-" + b;
    return (this._dist[key] =
      this._dist[key] ||
      (() => {
        let dist = 0;
        for (let i = 0; i < a.length; i += 8) {
          const va = parseInt(a.slice(i, i + 8), 16);
          const vb = parseInt(b.slice(i, i + 8), 16);
          dist += bitCount(va ^ vb);
        }
        return dist;
      })());
  }

  query(baseKey, maxDist) {
    const out = [];
    for (const key of this.data) {
      const distance = this.distance(key, baseKey);
      if (distance <= maxDist) out.push({ key, distance });
    }
    return out.sort((a, b) => a.distance - b.distance);
  }
}

const hm = new HashMaker(512);

describe("BKTree", () => {
  it("should compute distance", () => {
    const tree = new BKTree(512);
    for (const a of hm.data)
      for (const b of hm.data)
        expect(tree.distance(a, b)).to.equal(hm.distance(a, b));
  });

  it("should walk the tree", () => {
    const tree = new BKTree(512).add(hm.data);
    const got = [];
    tree.walk((key, depth) => got.push(key));
    expect(got.sort()).to.deep.equal(hm.data.slice(0).sort());
  });

  it("should query", () => {
    const tree = new BKTree(512).add(hm.data);
    for (let dist = 0; dist <= hm.length; dist++) {
      for (const baseKey of [hm.random, hm.data[0]]) {
        const baseKey = hm.random;
        const got = [];
        tree.query(baseKey, dist, (key, distance) =>
          got.push({ key, distance })
        );
        expect(got.sort((a, b) => a.distance - b.distance)).to.deep.equal(
          hm.query(baseKey, dist)
        );
      }
    }
  });
});
