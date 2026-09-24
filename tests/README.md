# PmLogLib tests

Plain C, no test framework: these are meant to run on a device where the
only thing that can be relied on is a C library.

    cmake -D ENABLE_TESTS=ON <source dir>
    make
    ctest --output-on-failure

| test | what it covers |
| --- | --- |
| `test_contexts` | the context table and levels through the public API: lookup, creation, names, level round trips, and what each call does with arguments it should refuse |
| `test_sharing` | that the table really is shared - children re-exec, so the library constructor runs again and has to attach to the block this process created |
| `test_stress` | several threads, optionally several processes, racing through `PmLogGetContext()` and `PmLogSetContextLevel()`, followed by an integrity check of the whole table |

## /dev/shm

The context table lives in `/dev/shm/pmloglib.shm` and is shared by every
process on the machine, so a test run would otherwise inherit whatever
contexts are already there and leave its own behind. `run-tests.sh`, which
is what `ctest` invokes, puts each test in its own mount namespace with a
fresh tmpfs on `/dev/shm`. Where the kernel does not allow that, it says
so and runs the tests against the real one.

## Stress runs

`test_stress` takes arguments, so it can be run directly for longer or
wider runs than ctest does:

    tests/run-tests.sh ./tests/test_stress            # the ctest defaults
    ./tests/test_stress --threads 32 --iterations 20000
    ./tests/test_stress --processes 8 --threads 4     # racing the file lock

`threads x contexts x processes` has to stay below `PMLOG_MAX_NUM_CONTEXTS`,
since that is the size of the table they all share.

Building the library with `-fsanitize=thread` and running the stress test
is what checks the locking itself:

    cmake -D ENABLE_TESTS=ON -D CMAKE_C_FLAGS=-fsanitize=thread <source dir>

## Static analysis

`run-static-checks.sh` runs whichever of gcc's analyzer, clang's analyzer,
clang-tidy, cppcheck and sparse are installed, with the include paths of a
configured build tree:

    tests/run-static-checks.sh build

Sparse does not parse C23, so it is given a copy of the file with the two
C23 spellings rewritten.
