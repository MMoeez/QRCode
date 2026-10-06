#!/bin/bash

clang++ -O2 run-tests.cpp QrCode.cpp QrSegment.cpp BitBuffer.cpp ../src/qrcode.c -o test && ./test
clang++ -O2 run-tests.cpp QrCode.cpp QrSegment.cpp BitBuffer.cpp ../src/qrcode.c -o test -D LOCK_VERSION=3 && ./test

