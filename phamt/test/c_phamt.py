# -*- coding: utf-8 -*-
####################################################################################################
# phamt/test/__init__.py
# Declaration of tests for the (C API) PHAMT type.
# By Noah C. Benson

from unittest import TestCase
from ..core import PHAMT

class TestPHAMT(TestCase):
    """Tests of the `phamt.PHAMT` type."""

    def test_empty(self):
        """Tests that `PHAMT.empty` has the correct properties.

        `PHAMT.empty` is the empty `PHAMT`; it should have 0 length, and it
        should not contain anything.
        """
        self.assertTrue(len(PHAMT.empty) == 0)
        self.assertFalse(any(x in PHAMT.empty for x in range(100)))
        # #TODO: test iteration

    def test_assoc(self):
        """Tests that `PHAMT.assoc` works correctly.

        The `assoc` method is used to add items to the PHAMT or to replace them.
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
        # Make sure the length is correct and it contains it's items.
        self.assertTrue(len(p) == n)
        for k in range(n):
            self.assertTrue(k in p)
            self.assertTrue(p[k] == str(k))
        # #TODO: test iteration
