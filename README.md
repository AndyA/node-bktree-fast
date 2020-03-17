# What's a BK Tree?

As an example, a common strategy for de-duplicating images is to compute
perceptual hashes for each of the images and compare those hashes with
each other. Such hashes are small compared with the images (often 32,
64, 128 bits). If the hashing function is good hashes that differ by
a small number of bits likely relate to similar images.

While the hashing operation is typically fast, searching for similar
images theoretically involves comparing the hash for a particular image
with all the other hashes.

A [Burkhard Keller tree](https://en.wikipedia.org/wiki/BK-tree) is a
data structure that greatly speeds up the search for hashes that differ
from a target hash by no more than a specified number of bits.

## What's this module?

This is a fast implementation of a BK tree written in C (with N-API). It
allows the efficient indexing and querying of hashes of any size. The
application it was written for has a trove of around 1,500,000 images.

A general purpose BK tree allows any distance metric to be used, e.g.
Levenshtein edit distance. This module only supports the Hamming
distance between hashes - i.e. the number of bits that differ.

## Installing

```sh
$ npm install bktree-fast
```

## Usage

```js
const BKTree = require("bktree-fast");

// Make a new tree for 512 bit hashes. The hash length
// must be a multiple of 64.
const tree = new BKTree(512);

// Hashes are hexadecimal strings
const a =
  "611e251612260cb60fb4afb003b142e1a36bb3db93d313c1d3cbf2c3f2d312ba" +
  "c0cdc0c5c8c5c8c5c0c5c045c0c5c0c5e0c5e0cde1cde1ddc1ddc1f9c1f9c3f9";

const b =
  "63be673600260db64fb4afb083b141e1a37bb3db93c1d3c193cbf2cbf2d392e3" +
  "c0cdc0c5c8c5c8c5c0c5c045c0c5c0c5c0c5c0cde1cde1dde1ddc1f9c1f9c3f9";

tree.add(a, b);

// Search for all entries within 10 bits of |a|
tree.query(a, 10, (hash, distance) => {
  console.log(`${hash} ${distance}`);
});
```

### Constructor

```js
const tree = new BKTree(64);
```

The constructor takes a single argument: the number of bits in each
hash. An error is thrown if this is not a multiple of 64.
  
### add(...hashes)

```js
tree.add(hash);                           // A single hash
tree.add(hash1, hash2, hash3);            // Multiple hashs
tree.add([hash1, hash2], [hash3, hash4]); // Arrays of hashs
```

Add hashes to the tree. Handles multiple arguments and any arrays are
flattened. A tree has set semantics: no values are stored against the
hashes and adding a hash more than once has no effect.

### query(hash, maxDist, callbackl)

```js
tree.query(hash, 10, (found, distance) => {
  console.log(`${found} is ${distance} bits from ${hash}`);
});
```

Query the tree to find all hashes that are within the specified Hamming
distance of the supplied hash. Searches slow down significantly when
maxDist is large.

### has(hash)

```js
if (tree.has(hash)) console.log(`Got ${hash}`);
```

Check whether the specified hash is in the tree.

### walk(callback)

```js
tree.walk((hash, depth) => {
  console.log(`${hash} is at depth ${depth} in the tree`);
});
```

Iterate all the hashes in the tree invoking the callback with each hash
and its corresponding depth in the tree.

### distance(hashA, hashB)

```js
const dist = tree.distance(hashA, hashB);
```

Compute the Hamming distance between two hashes. The return value will be
between 0 and the number of hash bits for this tree.

## License

Copyright © 2020, [Andy Armstrong](https://github.com/AndyA).
Released under the [MIT License](LICENSE).


