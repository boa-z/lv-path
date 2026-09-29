# Embedded engineering review

The core is C99 using ordinary pointers and C99 math. Embedded-friendly means
caller-controlled storage and bounded subdivision depth, not proven stack usage
or real-time timing on every MCU. No board was flashed or target stack measured
in this review.

## Ownership and storage placement

| Object | Owner / lifetime | Suggested placement |
| --- | --- | --- |
| Commands and pg_path_t descriptor | Caller; both outlive the measure and every query | Static const where ordinary reads can access Flash |
| pg_measure_t | Caller; managed fields written by init | Persistent state for repeated queries |
| Sample workspace | Caller; unchanged while measure is used | Static array or caller arena to avoid a large automatic array |
| Sink command array | Caller; survives all borrowed output views | Fixed output buffer sized separately from samples |
| pg_path_buffer_t / writer context | Caller; context outlives synchronous calls | Automatic if output lifetime does not depend on it |
| Query outputs | Caller; distinct position/tangent objects | Small automatic objects |

The measure borrows the descriptor, not just its command pointer: do not return
a measure referring to a function-local descriptor. Static storage is a choice,
not a requirement; caller arenas work equally well. Do not overlap source,
workspace and destination buffers. Immutable prepared measures can be shared by
readers; initialization and reuse need caller synchronization. The example's one
static sample array cannot serve independent paths concurrently.

Failed init clears the measure; workspace bytes are unspecified. Writers may
leave partial output. Buffer callback failures poison the buffer, but a
producer-side failure need not: check the producer before exporting/consuming
its output. There is no cleanup call or ownership transfer.

## No mandatory heap

The core C files make no allocator calls, install no allocator callbacks,
and allocate no hidden command list or cache. Exhaustion returns an error and
never triggers a heap fallback. A consumer may choose dynamic storage outside
the core contract.

This does not cover application callbacks, a target's math implementation, or
host stdio. The CSV example uses stdio, which may allocate internally. An embedded
consumer can replace CSV output with its own state update while calling the same
geometry API. The existing path_queries example has no stdio dependency.

## Static buffer budget

Use sizeof on the actual target; do not assume host ABI padding:

    read_only_bytes = C * sizeof(pg_cmd_t) + sizeof(pg_path_t)
    measure_bytes   = sizeof(pg_measure_t) + N * sizeof(pg_measure_sample_t)
    output_bytes    = K * sizeof(pg_cmd_t)
                      + sizeof(pg_path_buffer_t) + sizeof(pg_path_writer_t)

C is input command count, N sample capacity, K output command capacity. Query-only
clients need no output command array. These formulas exclude stack, linker
padding, program code, application state and runtime overhead. Moving arrays
from automatic to static storage shifts their cost from stack to static RAM;
it does not reduce total RAM.

For the motion example, N=256 and C=2; no output array or frame history is kept.
On the previously checked x86-64 ABI, a sample occupies 12 bytes, so the array is
1536 bytes. Re-evaluate sizeof for the target. That capacity fits its tested
curve/tolerance; it is not a recommendation for every path.

With L measurable leaves, init needs exactly L+1 samples. At configured depth D,
a quadratic/cubic command has at most 2^D leaves; a straight command has at most
one. This conservative bound may exceed the public uint16_t limit (65535 slots).
Calculate capacities in a wide type before narrowing; do not compute arbitrary
shifts in a 16-bit type. Handle exhaustion as a recoverable application error or
select an explicitly reviewed larger capacity. Do not silently loosen tolerance
or use partially initialized output.

## Stack and bounded work

Subdivision starts at depth zero. D=12 allows at most D+1=13 simultaneously active
subdivision invocations, plus traversal, callbacks and callees. Frames can retain
control polygons and child spans; inlining, spills, optimization, ABI and libm
change actual bytes. Bounded depth is not a byte budget. Increasing
PG_MAX_RECURSION increases potential work exponentially; set a nonnegative,
reviewed value in the core build, not just consumer headers. No explicit heap
stack or tail-call guarantee is used.

Queries and slicing do not use adaptive recursion. Query work includes an O(C)
scan plus O(log N) search; slicing can revisit prefixes (O(C^2)). Callbacks add
arbitrary time/stack, so the library cannot promise a deadline for caller code.

For embedded acceptance, compile the exact target/toolchain/flags with stack-usage
reporting (GCC: -fstack-usage), inspect the recursive call chain including math
and callbacks, and measure runtime stack high-water on adversarial curves.
Record configuration, command/sample counts, compiler, map/code size, raw timing
and overflow results. A per-function .su number or host benchmark alone is not a
whole-call-chain bound. Budget for surrounding application/interrupt context.

## Float and libm rationale

A single float model keeps storage and declarations predictable; no public
precision-selection API is added. hypotf avoids the intermediate squared-norm
overflow of sqrtf(x*x+y*y); scaled normalization handles large finite vectors.
fabsf, fmaxf, isfinite and isnan support predicates and error handling. Some may
be inlined; link the target math library when required (typically -lm with
GNU-like toolchains).

An FPU is not required for correctness, but software float/libm may be slow or
large. Float limits cumulative-length resolution and fixed absolute thresholds
make units significant. Do not claim full freestanding support or bounded latency
without runtime validation. Retain normal finite/NaN semantics; fast-math can
invalidate checks. Fixed-point or double variants need separate justification
and are outside this review.

## Remaining target evidence

Host tests and sanitizers establish only exercised host behavior. Target-specific
worst-case stack, workspace, code size, libm cost and latency remain unmeasured.
See [benchmarks](benchmarks.md) for dated host evidence and [geometry guarantees](geometry-contract.md)
before setting application budgets.
