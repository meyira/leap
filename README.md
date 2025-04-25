# LEAP: A Fast, Lattice-based OPRF With Application to Private Set Intersection

This repository contains the prototype implementation of the OPRF **Leap**,
published at Eurocrypt 2025. The code is provided as-is for research purposes,
and is not optimized or secure for production purposes. 

This implementation corresponds to the [full version](https://eprint.iacr.org/2025/333.pdf) of the paper, which includes additional tables not present in the conference
version. 

## Build Instructions

```sh
mkdir build && cd build && cmake .. && make -j
```

Once built, all binaries will be located in `build/leap/tests`. 

## Requirements

### libOTe Installation

**libOTe** is a fantastic library but can be a bit tricky to build. We recommend using
the provided Docker image: 
```sh
docker build -t leap . && docker run -v $PWD:/pwd --rm -it leap
```
Building the image takes around fie minutes as it pulls a specific version of the
libOTe and its dependencies. If you have a working installation of the libOTe on your
system, you can just run the code natively. The benchmarks use libOTe v2.1.0,
the Docker container uses 2.2.0 for compatibility reasons. 

### Building Inside Docker

The Dockerfile mounts the repository to `/pwd`. To build the library inside the
container, run:  
```sh 
mkdir build && cd build && cmake .. && make -j
``` 

### Manual installation of the libOTe

Installed with 
```sh 
python3 build.py --boost -DENABLE_SIMPLESTOT_ASM=ON -DENABLE_MR_KYBER=ON \ 
-DENABLE_IKNP=ON -DENABLE_SOFTSPOKEN_OT=ON -DENABLE_SILENTOT=ON -DLIBOTE_STD_VER=20 --relic  
```
The benchmarks use libOTe v2.1.0. 


### Benchmarking and Dockerfile Differences
Our original benchmarks were conducted on a machine with the following specifications: 

- **OS:** Ubuntu 22.04.1
- **Kernel:** 6.2.0-37-generic
- **CPU:** AMD Ryzen 9 7900X (12-Core, fixed at 4.7 GHz, benchmarks run in single process)
- **RAM:** 128 GiB
- **Dependency:** libOTe v2.1.0 (natively installed)

For the Docker image:

- We use **Ubuntu 24.04** as it provides better stability for **libOTe** and C++ dependencies.
- The Dockerfile installs the commit ID e05696d, which offers smoother dependency management.
- Despite the version difference, the functionality of the used features remains unchanged.

### AVX2 requirement

AVX2 instructions **must** be enabled.

## Code Description for Tables in Data

### Table 2: Lattice Estimator Complexity
The exact calls made to get the data for Table 2 is in `estimate-spring.py`. 
This table is *not* included in the conference version.

### Table 3: Preprocessing Complexity 
The source files can be found in `leap/tests/baseOT`. The number of iterations can
be adjusted in `leap/tests/baseOT/iter.h`, the table in the paper benchmarks `1` and `1<<13` interations. 
The following binaries in
`leap/tests` are used to generate the benchmarks:  

- `SimplestOtPreprocessing` benchmarks the iterations for SimplestOT with the
  IKNP OT extension (S.OT+IKNP line in the table). 
- `KyberOtPreprocessing` benchmarks the iterations for KyberOT with the
  IKNP OT extension (K.OT+IKNP line in the table). 
- `SilentSimplestOtPreprocessing` benchmarks the iterations for SimplestOT with the
  SilentOT extension (K.OT+Silent line in the table). 
- `SilentKyberOtPreprocessing` benchmarks the iterations for KyberOT with the
  SilentOT extension (K.OT+Silent line in the table). 

**Note**: This table is *Table 2* in the conference version.

### Table 4: OPRF Communication and Computation Complexity
The binary `build/leap/tests/oprf-ref` is generated using `leap/tests/oprf-ref.cpp`. The output includes the PRF output to verify correctness and hashing the OPRF output to remove the algebraic structure.
IP and port are hardcoded in the file for a simpler user interface. 

This table is *Table 3* in the conference version.

### Table 5 and Table 6: PSI communication complexity
Generated using `leap/tests/test-psi.cpp`. 

## Launching a second terminal in the same Docker 

  1. Get the ID of the Docker container, either by running `docker ps` and
     getting the `CONTAINER ID` of the `IMAGE` named `leap`, or from the
hostname of the first container (the hex after `root@`). 
  2. Launch the second container using `docker exec -it $ID bash`, where `$ID`
     is the ID of the docker container obtained in step 1. 

## Concrete Execution
The concrete calls are (in two
seperate terminals, from the build folder): 

- `leap/tests/test-psi 0 127.0.0.1 12345 20` and  `leap/tests/test-psi 1 127.0.0.1 12345 1`
- `leap/tests/test-psi 0 127.0.0.1 12345 20` and  `leap/tests/test-psi 1 127.0.0.1 12345 10`
- `leap/tests/test-psi 0 127.0.0.1 12345 24` and  `leap/tests/test-psi 1 127.0.0.1 12345 15`

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

- `leap/tests/test-psi 0 127.0.0.1 12345 0` and  `leap/tests/test-psi 1 127.0.0.1 12345 0`
- `leap/tests/test-psi 0 127.0.0.1 12345 5` and  `leap/tests/test-psi 1 127.0.0.1 12345 5`
- `leap/tests/test-psi 0 127.0.0.1 12345 10` and  `leap/tests/test-psi 1 127.0.0.1 12345 10`

## Acknowledgements
This project depends or references a number of other projects: 

- [SPRING code](https://github.com/cbouilla/spriiiiiiiing/) used as a base for the implementation 
- [libOTe Release 2.1.0](https://github.com/osu-crypto/libOTe/releases/tag/v2.1.0) for KyberOT
- [Falcon's NTT implementation](https://github.com/tprest/falcon.py/blob/master/ntt.py), used as a base for our more flexible NTT implementation
- [Cuckoo Filter implementation](https://github.com/efficient/cuckoofilter) for
  the PSI implementation
- [SHAKE256 implementation](https://github.com/XKCP/XKCP)
