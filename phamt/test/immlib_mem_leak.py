import gc, tracemalloc
import numpy as np
from immlib import calc, plan

@calc('b')
def f(a): return a * 2
@calc('c')
def g(b): return b + 1
p = plan(f=f, g=g)

def run(n):
    for _ in range(n):
        pd = p(a=np.ones(int(1e6)))
        pd['c']

run(5)
gc.collect()
tracemalloc.start()
base = tracemalloc.get_traced_memory()[0]
run(200)
gc.collect()
print("MB retained after 200 plandicts + gc.collect():",
      (tracemalloc.get_traced_memory()[0] - base) / 1e6)
