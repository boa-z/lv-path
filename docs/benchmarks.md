# Tests and benchmark evidence

Geometry baseline validation: 2026-09-29.
These are recorded local host results, not remote CI or MCU acceptance.

## Validation

| Check | Result |
| --- | --- |
| GCC 16.1.0, MinGW x86-64 UCRT, Release (-O3 -DNDEBUG), strict C99 warnings | 12/12 CTest cases passed |
| Clang 22.1.8, MSYS2 clang64, Debug, ASan + UBSan, halt_on_error=1 | 12/12 CTest cases passed |
| Public header self-containment | Six independent C99 translation units built by both toolchains |
| C++ interoperability | Six separate C++11 header compile/link/run checks with G++ passed |
| Core archive dependency inspection | External non-pg symbols in GCC archive: fmaxf and hypotf; no allocator, LVGL or OS symbols |

The twelve CTest entries are nine geometry suites, the API example and the host
benchmark. The reference suite runs 16,524 checks over 48 deterministic curves
(24 quadratic, 24 cubic), using independent double Bernstein evaluation and
4096-segment reference lengths. It checks splits, lengths, sampled position
errors, strict LUT monotonicity, tangent magnitude and normal orthogonality.
The targeted regression suite covers large floats, unrepresentable cumulative
length, disconnected-contour tangents, tiny-parameter slices and invalid state.
Existing suites cover command validation, workspace exhaustion, loops,
backtracking, contour breaks, slice reconstruction and writer failures.

The reference-test envelopes describe only their bounded corpus. The accuracy
suite adds 110,085 checks over quadratics/cubics, cusps, loops, retracing,
repeated contours and scale ranges; its allowances include explicit float/oracle
terms and are not universal certificates. The straight half-distance regression
now returns x=50. See [the API accuracy discussion](api.md#distance-query-accuracy).

CTest sanitizer success is evidence for exercised cases, not a proof of memory
safety for arbitrary inputs. No rendered-UI, upstream LVGL integration, or board validation is claimed.

## Reproduce

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure
    ./build/tests/bench_measure

For a sanitizer build, configure a separate directory with
-DLV_PATH_SANITIZE=ON and a supported GCC/Clang toolchain. Set
UBSAN_OPTIONS=halt_on_error=1. Windows executables have .exe suffixes; a
multi-configuration generator may require a configuration subdirectory.
The existing CI workflow runs GCC/Clang with and without sanitizers on Linux;
new tests and header builds are included automatically.

For dependency inspection on GNU/LLVM toolchains:

    nm -u build/liblv_path.a

Cross-object pg_* references are resolved within the archive. Compiler-generated
runtime symbols may differ by target. Source inspection plus this host symbol
check confirms no direct core allocator dependency; it does not measure the
implementation details of every target's math library or application callbacks.

## Host microbenchmark

Host: Windows x86-64, Intel Core 5 220H, GCC 16.1.0 MinGW/UCRT, Release
-O3 -DNDEBUG, no sanitizers, no explicit LTO. Five consecutive process runs on
2026-09-29. Timings use clock(), which has implementation-dependent/coarse
resolution. Fixed iteration counts amortize overhead; zero-duration phases are
reported as unavailable instead of substituting a fabricated denominator.
No performance threshold is used as a correctness test.

Input: two connected cubic spans, tolerance 0.25, total measured length 462.114,
49 samples. Each process performs 20,000 builds, 2,000,000 position/tangent
queries cycling through 1000 distances, and 200,000 slices cycling through
100 ranges. Slices use a 64-command buffer. A printed checksum consumes results.

| Operation | Minimum / second | Median / second | Maximum / second |
| --- | ---: | ---: | ---: |
| Measure initialization | 112,994 | 162,602 | 165,289 |
| Position/tangent query | 19,417,476 | 20,000,000 | 20,408,163 |
| Curve-preserving slice | 10,526,316 | 11,111,111 | 11,764,706 |

On this ABI: sizeof(pg_measure_t)=24, sizeof(pg_measure_sample_t)=12,
sizeof(pg_cmd_t)=28 bytes. The benchmark reserves 256 samples (3072 bytes)
and 64 output commands (1792 bytes); sizeof values and padding are not portable
ABI guarantees. Command input storage and recursive stack are additional.

Host speed does not predict software-float MCU speed, worst-case path time or
stack high-water. The next measurement milestone should include code size,
libm cost, maximum recursion/workspace and realistic/adversarial path sets on
an explicitly identified embedded target. No target benchmark is claimed here.
