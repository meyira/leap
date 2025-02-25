# LEAP: A Fast, Lattice-based OPRF With Application to Private Set Intersection

This repository contains the prototype implementation of the OPRF Leap,
published at Eurocrypt 2025. The code is provided as-is for research purposes,
and is not optimized or secure for production purposes. 

This is the code for the full version with more tables than in the conference
version. 

## Build Instructions

`mkdir build && cd build && cmake .. && make -j` 

All binaries will be in `build/leap/tests` once built. 

## Requirements

### libOTe

Installed with `python3 build.py --boost -DENABLE_SIMPLESTOT_ASM=ON
-DENABLE_MR_KYBER=ON -DENABLE_IKNP=ON -DENABLE_SILENTOT=ON
-DENABLE_SOFTSPOKEN_OT=ON -DENABLE_PIC=ON -D FETCH_AUTO=true --relic --install
--sudo`
Used release 2.1.0 

### AVX2 instructions
Necessary. 

## Code Description for Tables in Data

### Table 2: Lattice Estimator Complexity
The exact calls made to get the data for Table 2 is in `estimate-spring.py`. 
This table is *not* included in the conference version.

### Table 3: Preprocessing Complexity 
Generated using the files in `leap/tests/baseOT`. The number of iterations can
be adjusted in `leap/tests/baseOT/iter.h`. The binaries will be in
`tests` once built. 

This table is *Table 2* in the conference version.

### Table 4: OPRF Communication and Computation Complexity
Generated using `leap/tests/oprf-ref.cpp`. The output includes the PRF output to verify correctness and hashing the OPRF output to remove the algebraic structure.
IP and port are hardcoded in the file for a simpler user interface. 

This table is *Table 3* in the conference version.

### Table 5 and Table 6: PSI communication complexity
Generated using `leap/tests/test-psi.cpp`. The concrete calls are (in two
seperate terminals, from the build folder): 

- `leap/tests/test-psi 0 127.0.0.1 12345 0` and  `leap/tests/test-psi 1 127.0.0.1 12345 0`
- `leap/tests/test-psi 0 127.0.0.1 12345 5` and  `leap/tests/test-psi 1 127.0.0.1 12345 5`
- `leap/tests/test-psi 0 127.0.0.1 12345 10` and  `leap/tests/test-psi 1 127.0.0.1 12345 10`

The default configuration is with KyberOT (ML-KEM) and IKNP OT. To switch, edit
the file `leap/psi/params.h`, where macros switch IKNP and KyberOT on via
`#define` statements. Comment the statements out to switch to SimplestOT and/or
SilentOT extension. 
The benchmarks with KyberOT and IKNP corresponds to *Table 4* in the conference version.

### Table 7: PSI comparison with other Naor-Reingold OPRFs

Finally, we compare the same code used for Table 4 with the available PSI
implementations:  

- [Cuckoo Filter implementation](https://github.com/efficient/cuckoofilter)
- [CSIDH isogeny implementations](https://github.com/meyira/OIDA) to compare PSI performance. 
- [Private Set Intersection using elliptic curves](https://github.com/contact-discovery/mobile_psi_cpp/) to compare against classical PSI with Naor-Reingold OPRFs. 

The calls are: 

- `leap/tests/test-psi 0 127.0.0.1 12345 24` and  `leap/tests/test-psi 1 127.0.0.1 12345 15`
- `leap/tests/test-psi 0 127.0.0.1 12345 24` and  `leap/tests/test-psi 1 127.0.0.1 12345 15`
- `leap/tests/test-psi 0 127.0.0.1 12345 24` and  `leap/tests/test-psi 1 127.0.0.1 12345 15`

## Acknowledgements
This project depends or references a number of other projects: 

- [SPRING code](https://github.com/cbouilla/spriiiiiiiing/) used as a base for the implementation 
- [libOTe Release 2.1.0](https://github.com/osu-crypto/libOTe/releases/tag/v2.1.0) for KyberOT
- [Falcon's NTT implementation](https://github.com/tprest/falcon.py/blob/master/ntt.py), used as a base for our more flexible NTT implementation
- [Cuckoo Filter implementation](https://github.com/efficient/cuckoofilter) for
  the PSI implementation
- [SHAKE256 implementation](https://github.com/XKCP/XKCP)
