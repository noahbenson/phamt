#! /bin/bash

# This script is intended for use inside the manylinux docker image in order to
# build the binary wheels for this library.
# The phamt directory should be mounted in $WORK.

PLAT="$1"
WORK=/github/home

# Compile wheels
mkdir -p /root/tmpwheels
for PYBIN in /opt/python/*/bin
do "${PYBIN}/pip" wheel ${WORK} --no-deps -w /root/tmpwheels
done

# Bundle external shared libraries into the wheels
mkdir -p ${WORK}/wheels
for w in /root/tmpwheels/*.whl
do if auditwheel show "$w"
   then auditwheel repair "$w" --plat "$PLAT" -w ${WORK}/wheels
   else cp "$w" ${WORK}/wheels
   fi
done

# Install packages and run the test
for PYBIN in /opt/python/cp3*/bin
do "${PYBIN}/pip" install phamt --no-index -f ${WORK}/wheels
   (cd "$HOME"; "${PYBIN}/python" -m unittest phamt.test)
done
