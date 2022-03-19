# phamt

An optimized C implementation of a Persistent Hash Array Mapped Trie (PHAMT)
data structure for Python.

This repository contains a C implementation for a PHAMT type. The type maps
integer keys to arbitrary values efficiently, and does so with
immutable/persistent data structures. It is intended for use in building more
user-friendly persistent types such as persistent dictionaries and persistent
lists.

## Example Usage

```python
>>> from phamt import PHAMT

# Start with the empty PHAMT.
>>> nothing = PHAMT.empty

# Add some key-value pairs to it.
>>> items = nothing.assoc(42, "item 1").assoc(-3, "item 2")

# Lookup the items.
>>> items[42]
"item 1"

>>> items[-3]
"item 2"

# Iterate over the items.
>>> for (k,v) in items:
        print(f"key: {k}; value: {v}")
key: 42; value: item 1
key: -3; value: item 2

# Remove items.
>>> item = items.dissoc(42)

>>> item.get(42, "not found")
"not found"
```

## Current Status

The `phamt` library is currently under development. This section breaks down
the features and goals of the project and where they stand.

### Tested Features

`PHAMT` is working and is reasonably well tested overall. This includes updating
PHAMTs, iterating over PHAMTs, creating PHAMTs, and querying PHAMTs. The current
tests ensure that garbage collection is working correctly for deleted PHAMT
components as well.

### Written Features

`THAMT` is written and works for some examples; though most examples produce
segmentation faults somewhere in the operation. No tests are currently written
for THAMT.

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
  * Implement GitHub Actions testing for a variety of computer architectures.
  * Write a companion Python-only version.
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

