#! /bin/bash

# This script is intended for use inside the manylinux docker image in order to
# build the binary wheels for this library.
# The phamt directory should be mounted in /work.

PLAT="$1"

# Compile wheels
mkdir -p /root/tmpwheels
for PYBIN in /opt/python/*/bin
do "${PYBIN}/pip" wheel /work --no-deps -w /root/tmpwheels
done

# Bundle external shared libraries into the wheels
mkdir -p /work/wheels
for w in /root/tmpwheels/*.whl
do if auditwheel show "$w"
   then auditwheel repair "$w" --plat "$PLAT" -w /work/wheels
   else cp "$w" /work/wheels
   fi
done

# Install packages and run the test
for PYBIN in /opt/python/cp3*/bin
do "${PYBIN}/pip" install phamt --no-index -f /work/wheels
   (cd "$HOME"; "${PYBIN}/python" -m unittest phamt.test)
done
