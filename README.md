![Build Status](https://github.com/noahbenson/phamt/actions/workflows/tests.yml/badge.svg)

# phamt

An optimized C implementation of a Persistent Hash Array Mapped Trie (PHAMT)
data structure for Python.

This repository contains a C implementation for a PHAMT type. The type maps
integer keys to arbitrary values efficiently, and does so with
immutable/persistent data structures. It is intended for use in building more
user-friendly persistent types such as persistent dictionaries and persistent
lists.

## Example Usage

### PHAMT Usage

```python
# The phamt library contains 2 objects: the PHAMT and THAMT classes.
>>> from phamt import PHAMT, THAMT

# The PHAMT class contains the only instance of an empty PHAMT.
>>> nothing = PHAMT.empty

# We can add key/value pairs to a PHAMT object by using the assoc method.
>>> items = nothing.assoc(42, "item 1")
>>> items = items.assoc(-3, "item 2")
>>> items = items.assoc(9999, "item 3")

# We can lookup the items just like in a normal dictionary.
>>> items[42]
"item 1"

>>> items[-3]
"item 2"

>>> items[9999]
"item 3"

# We can remove keys using the dissoc method.
>>> items = items.dissoc(9999)

>>> item.get(9999, "not found")
"not found"

# Iterate over the items.
>>> for (k,v) in items:
        print(f"key: {k}; value: {v}")
key: 42; value: item 1
key: -3; value: item 2

# PHAMTs cannot be edited, however.
>>> items[10] = 10
TypeError: 'phamt.core.PHAMT' object does not support item assignment
```

### THAMT Usage

```python
# The THAMT class can be created from any PHAMT. (This is O(1).)
>>> tmp = THAMT(items)

# THAMTs are also dictionaries like PHAMTs.
>>> tmp[42]
"item 1"

>>> tmp[-3]
"item 2"

# Unlike PHAMTs, THAMTs can be edited in-place. This does not modify the
# original PHAMT object.
>>> tmp[10] = 10

>>> tmp[10]
10

# THAMTs also support iteration.
>>> for (k,v) in tmp:
        print(f"key: {k}; value: {v}")
key: 42; value: item 1
key: -3; value: item 2
key: 10; value: 10

# THAMTs can be efficiently turned into PHAMTs.
>>> edited_items = tmp.persistent()
```

## Current Status

The `phamt` library is currently under development. This section breaks down
the features and goals of the project and where they stand.

### Tested Features

`PHAMT` is working and is reasonably well tested overall. This includes updating
PHAMTs, iterating over PHAMTs, creating PHAMTs, and querying PHAMTs. The current
tests ensure that garbage collection is working correctly for deleted PHAMT
components as well.

`THAMT` is also written and is reasonably well tested overall.

Both `PHAMT` and `THAMT` have companion Python-only implementations (see `phamt.py_core`), but these are not intended to be fast or space-efficient; in fact, they are a couple orders of magnitude slower than the C implementations.

### Goals

* `PHAMT`
  * `PHAMT.from_list(q)` (or `from_iter`?) should efficiently create a `PHAMT`
    whose keys are the numbers `0` through `len(q)-1` and whose values are the
    elements in `q`. This can be done slightly more efficiently using a
    bottom-up approach than can be done using transients (though they are likely
    almost as fast).
  * `PHAMT(d)` for a `dict` with integer keys, `d`, should efficiently allocate
    a `PHAMT` with matching keys/values as `d`.
* General
  * Write/publish a benchmarks notebook to compare runtimes performace of
    `PHAMT` and `THAMT` to other similar Python data structures.
  * Write more documentation.
  * Publish to PyPI.

## License

MIT License

Copyright (c) 2022 Noah C. Benson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

