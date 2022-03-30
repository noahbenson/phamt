# -*- coding: utf-8 -*-
####################################################################################################
# phamt/test/__init__.py
# Declaration of tests for the (C API) PHAMT type.
# By Noah C. Benson

import sys
from unittest import TestCase
from ..c_core import PHAMT, THAMT

class TestPHAMT(TestCase):
    """Tests of the `phamt.PHAMT` type."""

    MIN_INT = -(2 ** (sys.hash_info[0] - 1))
    MAX_INT = (2 ** (sys.hash_info[0] - 1) - 1)

    def make_random_pair(self, n=100, minint=None, maxint=None):
        """Peforms a random set of operations on both a dict and a PHAMT and
        returns both.

        The dict is made with values that are weakrefs to the same objects that
        make up the PHAMT values. They should otherwise be equal.

        Also returns a list of (ordered) weakref references to the values that
        were inserted (some may be garbage collected already, that is fine).
        """
        from random import (randint, choice)
        from weakref import ref
        if minint is None: minint = TestPHAMT.MIN_INT
        if maxint is None: maxint = TestPHAMT.MAX_INT
        d = {}
        u = PHAMT.empty
        inserts = []
        for ii in range(n):
            # What to do? assoc or dissoc?
            if len(d) == 0 or randint(0,3):
                # assoc:
                v = set([ii])
                vr = ref(v)
                k = randint(minint, maxint)
                u = u.assoc(k, v)
                d[k] = vr
                inserts.append(vr)
            else:
                # Choose k frmo in d
                k = choice(list(d))
                u = u.dissoc(k)
                del d[k]
            # At every step, these must be equal.
            self.assertEqual(len(d), len(u))
            for (k,v) in d.items():
                self.assertTrue(k in u)
                self.assertTrue(v() is not None)
                self.assertTrue(u[k] is v())
        return (u, d, inserts)
    def make_random_pair_transient(self, n=100, minint=None, maxint=None):
        """Peforms a random set of operations on both a dict and a THAMT and
        returns both.

        The dict is made with values that are weakrefs to the same objects that
        make up the THAMT values. They should otherwise be equal.

        Also returns a list of (ordered) weakref references to the values that
        were inserted (some may be garbage collected already, that is fine).

        At random intervals during its construction, the THAMT is persisted and
        checked against the dict.
        """
        from random import (randint, choice)
        from weakref import ref
        if minint is None: minint = TestPHAMT.MIN_INT
        if maxint is None: maxint = TestPHAMT.MAX_INT
        d = {}
        u = THAMT()
        inserts = []
        lns = ["START"]
        for ii in range(n):
            # What to do? assoc or dissoc?
            if len(d) == 0 or randint(0,3):
                # assoc:
                v = set([ii])
                vr = ref(v)
                k = randint(minint, maxint)
                lns.append(f"   assoc {k}")
                u[k] = v
                d[k] = vr
                inserts.append(vr)
            else:
                # Choose k from in d
                k = choice(list(d))
                lns.append(f"   dissoc {k}")
                del u[k]
                del d[k]
            # Ocassionally, we persist and check the THAMT.
            try:
                if not randint(0, n // 10):
                    p = u.persistent()
                    self.assertEqual(len(d), len(p))
                    for (k,v) in d.items():
                        self.assertTrue(k in u)
                        self.assertTrue(v() is not None)
                        self.assertTrue(p[k] is v())
                # At every step, these must be equal.
                self.assertEqual(len(d), len(u))
                for (k,v) in d.items():
                    self.assertTrue(k in u)
                    self.assertTrue(v() is not None)
                    self.assertTrue(u[k] is v())
                for (k,v) in u:
                    self.assertTrue(k in d)
                    self.assertTrue(d[k]() is v)
            except Exception:
                for ln in lns: print(ln)
                raise
        return (u, d, inserts)
    def test_empty(self):
        """Tests that `PHAMT.empty` has the correct properties.

        `PHAMT.empty` is the empty `PHAMT`; it should have 0 length, and it
        should not contain anything.
        """
        self.assertTrue(len(PHAMT.empty) == 0)
        self.assertFalse(any(x in PHAMT.empty for x in range(100)))
    def test_iteration(self):
        """Tests that `PHAMT` iteration works correctly.
        """
        import gc
        (u, d, assocs) = self.make_random_pair(500)
        # First, iterate over d:
        n = 0
        for (k,v) in u:
            self.assertTrue(k in d)
            self.assertEqual(d[k](), v)
            n += 1
        self.assertEqual(n, len(d))
        # if we delete u, all the assocs should get gc'ed (i.e., iteration
        # shouldn't interfe with garbage collection).
        del u
        (k,v) = (None,None)
        gc.collect()
        for r in assocs:
            self.assertEqual(r(), None)
    def test_edit(self):
        """Tests that `PHAMT.assoc` and `PHAMT.dissoc` work correctly.

        The `assoc` method is used to add items to the PHAMT or to replace them.
        The `dissoc` method is used to remove items.
        """
        nought = PHAMT.empty
        p1 = nought.assoc(10, '10')
        p2 = p1.assoc(2000, '2000')
        p3 = p2.assoc(-50000, '-50000')
        # They should all have (and retain) correct lengths.
        self.assertTrue(len(nought) == 0)
        self.assertTrue(len(p1) == 1)
        self.assertTrue(len(p2) == 2)
        self.assertTrue(len(p3) == 3)
        # They should all have the keys they were given.
        self.assertTrue(10 not in nought and
                        10 in p1 and
                        10 in p2 and
                        10 in p3)
        self.assertTrue(2000 not in nought and
                        2000 not in p1 and
                        2000 in p2 and
                        2000 in p3)        
        self.assertTrue(-50000 not in nought and
                        -50000 not in p1 and
                        -50000 not in p2 and
                        -50000 in p3)
        # They should all have the correct values also.
        self.assertTrue(p1[10] == '10' and p2[10] == '10' and p3[10] == '10')
        self.assertTrue(p2[2000] == '2000' and p3[2000] == '2000')
        self.assertTrue(p3[-50000] == '-50000')
        # When looking up invalid values, they should raise errors.
        with self.assertRaises(KeyError):
            nought[10]
        with self.assertRaises(KeyError):
            p1[2000]
        with self.assertRaises(KeyError):
            p2[-50000]
        # Let's assoc up something more complex and see how it does.
        p = nought
        n = 100000
        for k in range(n):
            p = p.assoc(k, str(k))
        # Make sure the length is correct and it contains its items.
        self.assertEqual(len(p), n)
        for k in range(n):
            self.assertTrue(k in p)
            self.assertTrue(p[k] == str(k))
        # A more complicated (random) test that includes dissoc.
        (u, d, assocs) = self.make_random_pair(1000)
        self.assertTrue(len(u) == len(d))
        for (k,v) in d.items():
            self.assertTrue(k in u)
            self.assertTrue(u[k] is d[k]())
    def test_thamt(self):
        """Tests that THAMT objects work correctly.
        """
        import gc
        for k in range(5):
            (u, d, assocs) = self.make_random_pair_transient(500)
            # First, iterate over d::
            n = 0
            for (k,v) in u:
                self.assertTrue(k in d)
                self.assertEqual(d[k](), v)
                n += 1
            self.assertEqual(n, len(d))
            # Then check that these stay equal after persisting u.
            p = u#.persistent()
            n = 0
            for (k,v) in u:
                self.assertTrue(k in d)
                self.assertEqual(d[k](), v)
                n += 1
            self.assertEqual(n, len(d))
            n = 0
            for (k,v) in p:
                self.assertTrue(k in d)
                self.assertEqual(d[k](), v)
                n += 1
            self.assertEqual(n, len(d))
            # if we delete u, all the assocs should get gc'ed (i.e., iteration
            # shouldn't interfere with garbage collection).
            del u
            del p
            (k,v) = (None,None)
            gc.collect()
            for r in assocs:
                self.assertEqual(r(), None)
    def test_gc(self):
        """Tests that PHAMT objects are garbage collectable and allow their
        values to be garbage collected.
        """
        import gc
        from weakref import ref
        s = set([])
        sr = ref(s)
        u1 = PHAMT.empty.assoc(0, s)
        del s
        self.assertTrue(sr() is not None)
        del u1
        gc.collect()
        self.assertTrue(sr() is None)
        # Test using random dict/PHAMT inserts and deletes.
        (u, d, assocs) = self.make_random_pair(500)
        # if we delete u, all the assocs should get gc'ed.
        del u
        gc.collect()
        for r in assocs:
            self.assertEqual(r(), None)

