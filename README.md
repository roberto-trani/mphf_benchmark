A Benchmark of Minimal Perfect Hash Functions Algorithms
----

This project aims to assess the performance of various *minimal perfect hash functions* algorithms.

**Definition**:
Given a set *S* of *n* distinct keys, a function *f* that bijectively maps the keys of *S* into the range
{0,...,*n*−1} is called a *minimal perfect hash function* (MPHF) for *S*.
Algorithms that find such functions when *n* is large and retain constant evaluation time are of practical interest.
For instance, search engines and databases typically use minimal perfect hash functions to quickly assign identifiers to static sets of variable-length keys such as strings.
The challenge is to design an algorithm which is efficient in three different aspects: time to find *f* (construction time), time to evaluate *f* on a key of *S* (lookup time), and space of representation for *f*.

This benchmark reports the three main efficiency aspects of each algorithm:  construction time, lookup time, and space of representation.
The minimal perfect hash functions are built using as keys either random 64-bits integers or line-delimited strings, which are read from the standard input.

This repository has been created during the development of [*PTHash: Revisiting FCH Minimal Perfect Hashing*]() [1], a C++ library implementing fast and compact minimal perfect hash functions.

**Tested Algorithms**

Currently implemented/included algorithms are:
* PTHash [1] [https://github.com/jermp/pt_hash](https://github.com/jermp/pthash)
* FCH [2]
* CHD [3] [https://github.com/bonitao/cmph](https://github.com/bonitao/cmph)
* EMPHF [4] [https://github.com/ot/emphf](https://github.com/ot/emphf)
* BBHash [5] [github.com/rizkg/BBHash](github.com/rizkg/BBHash)
* RecSplit [6] [https://github.com/vigna/sux](https://github.com/vigna/sux)


Compiling the code
----
The code depends on several git submodules. If you have cloned the repository without --recursive, you will need to perform the following command before building:
```
git submodule update --init --recursive
```

To build the code:
```
bash configure.sh
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

Usage
----

Execute `./mphf_benchmark -h` to print an help message describing all command line parameters, whose output is as follows
```
Usage: ./mphf_benchmark [-h,--help] algorithm [-n num_keys] [--num_construction_runs num_construction_runs] [--num_lookup_runs num_lookup_runs] [--verbose] [--seed seed]

 algorithm
        The name of the algorithm to run. One among `fch`, `chd`, `bbhash`, `emphf`, `recsplit`, `pthash`.
 [-n num_keys]
        The number of 64-bit random keys to use for the test. If it is not provided, then keys are read from the input (one per line).
 [--num_construction_runs num_construction_runs]
        Number of times to perform the construction. (default: 1)
 [--num_lookup_runs num_lookup_runs]
        Number of times to perform the lookup test. (default: 1)
 [--verbose]
        Verbose output during construction. (default: false)
 [--seed seed]
        Seed used for construction. (default: 0)
 [-h,--help]
        Print this help text and silently exits.
```

To actually use the benchmark, and reproduce Table 5 of the paper [*PTHash: Revisiting FCH Minimal Perfect Hashing*]() [1] (except [GOV](https://github.com/vigna/Sux4J), which is Java-based), execute the following commands
```
./mphf_benchmark all -n 100000000 --num_construction_runs 5 --num_lookup_runs 5 --seed 1234567890
./mphf_benchmark all -n 1000000000 --num_construction_runs 5 --num_lookup_runs 5 --seed 1234567890
```

The following table summarizes the output of the second command, which employs 1 billion 64-bit random keys, on a server machine equipped with Intel i9-9900K cores (@3.60 GHz), 64 GB of RAM DDR3 (@2.66 GHz), and running Linux 5 (64 bits)

| Method      | construction<br>(secs) | space<br>(bits/key) | lookup<br>(ns/key) |
| :--- | ---: | ---: | ---: |
| FCH, c=4    | 15904 | 4.00 | 35 |
| FCH, c=5    |  2937 | 5.00 | 35 |
| FCH, c=6    |  2133 | 5.00 | 35 |
| FCH, c=7    |  1221 | 7.00 | 35 |
| CHD, 𝜆=4    |  1972 | 2.17 | 419 |
| CHD, 𝜆=5    |  5964 | 2.07 | 417 |
| CHD, 𝜆=6    | 23746 | 2.01 | 416 |
| EMPHF       |   374 | 2.61 | 199 |
| GOV         |   875 | 2.23 | 175 |
| BBHASH, 𝛾=1 |   253 | 3.06 | 170 |
| BBHASH, 𝛾=2 |   152 | 3.71 | 143 |
| BBHASH, 𝛾=5 |   100 | 6.87 | 113 |
| RecSplit, l=5, 𝑏=5   |   233 | 2.95 | 220 |
| RecSplit, l=8, 𝑏=100 |   936 | 1.80 | 204 |
| RecSplit, l=12, 𝑏=9  |  5700 | 2.23 | 197 |
| PTHash, C-C, 𝛼=0.99, 𝑐=7 |  1042 | 3.23 | 37 |
| PTHash, D-D, 𝛼=0.88, 𝑐=7 |   308 | 3.94 | 64 |
| PTHash, EF, 𝛼=0.99, 𝑐=6  |  1799 | 2.17 | 101 |
| PTHash, D-D, 𝛼=0.94, 𝑐=7 |   689 | 2.99 | 55 |

Authors
-------
* [Giulio Ermanno Pibiri](http://pages.di.unipi.it/pibiri/), <giulio.ermanno.pibiri@isti.cnr.it>
* [Roberto Trani](), <roberto.trani@isti.cnr.it>


References
-------
* [1] Giulio Ermanno Pibiri and Roberto Trani. *PTHash: Revisiting FCH Minimal Perfect Hashing*. In Proceedings of the 44th International
Conference on Research and Development in Information Retrieval (SIGIR). 2021.
* [2] Edward A Fox, Qi Fan Chen, and Lenwood S Heath. *A faster algorithm for constructing minimal perfect hash functions*. In Proceedings of the 15th International Conference on Research and Development in Information Retrieval (SIGIR). 1992.
* [3] Djamal Belazzougui, Fabiano C Botelho, and Martin Dietzfelbinger. *Hash, displace, and compress*. In European Symposium on Algorithms. Springer. 2009.
* [4] Djamal Belazzougui, Paolo Boldi, Giuseppe Ottaviano, Rossano Venturini, and Sebastiano Vigna. *Cache-oblivious peeling of random hypergraphs*. In 2014 Data Compression Conference. IEEE. 2014.
* [5] Antoine Limasset, Guillaume Rizk, Rayan Chikhi, and Pierre Peterlongo. *Fast and scalable minimal perfect hashing for massive key sets*. In 16th International Symposium on Experimental Algorithms. 2017.
* [6] Emmanuel Esposito, Thomas Mueller Graf, and Sebastiano Vigna. *Recsplit: Minimal perfect hashing via recursive splitting*. In 2020 Proceedings of the Twenty-Second Workshop on Algorithm Engineering and Experiments (ALENEX). SIAM. 2020.
