# Upstream readiness review

Review date: 2026-09-29. Scope: standalone path geometry.
The initial review accompanies the numeric-hardening implementation. Its host evidence does not
constitute an upstream-accepted design or a versioned release. See [benchmark/validation details](benchmarks.md).

## Assessment

**Suitable for an exploratory LVGL design discussion; not ready for a merge-ready
proposal or stable 1.0 API.** The dependency boundary is clean and the library
builds independently. The largest remaining issue is the distance-query contract:
current parameter interpolation can have large error even on straight curves.

| Area | Finding |
| --- | --- |
| LVGL independence | All seven core C files build without LVGL; no widget/OS/allocator dependencies in headers or source |
| Application scope | No application-specific exported types/state; removed stale application sizing language from public comments |
| Naming | Generic pg (path geometry) names; legacy path2d include/target names retained compatibly |
| Memory | Caller-owned commands, descriptor, samples and sink; no direct heap calls; lifetime/failure rules documented |
| C portability | C99 strict GCC and Clang builds; public headers separately compile as C99 and C++11 |
| Algorithms | Existing tests plus fixed-seed independent double-reference and targeted regression coverage |
| API stability | Signatures/layouts unchanged in this review; no frozen 1.0 ABI or upstream naming agreement |
| Accuracy | Local flatness and approximate length; distance-query accuracy and recursion-limit reporting remain unresolved |
| Integration | No upstream LVGL adapter or target-board acceptance claimed |

## Corrections made

1. Replaced overflow-prone squared vector distances with scaled/hypot-based math;
   normalized large finite vectors without spuriously returning zero or NaN.
2. Reject detected non-finite leaf lengths, cumulative overflow, and cumulative
   float stagnation; failed initialization leaves no usable partial measure.
3. Prevent fallback tangents from crossing disconnected contour jumps, including
   the final endpoint of a degenerate-handle curve.
4. Corrected tiny-parameter quadratic/cubic slice restriction: a positive t must
   not be mistaken for zero solely because it is below the geometry epsilon.
5. Aligned uninitialized normalized-slice errors with the absolute API.
6. Fixed the test comparison helper that previously allowed NaN to pass.
7. Documented lifetimes, precision, partial sink output, CLOSE topology loss, and
   complexity; replaced unsafe example error handling with a compiled example.
8. Added header self-containment builds, regressions, independent numeric-oracle
   coverage, and repeatable benchmark timing without invented zero-time rates.

## Gaps before a formal proposal

- **Distance inversion:** M(0,0) Q(0,0) (100,0), length 100, currently gives x=25 at
  distance 50; the correct half-distance position is x=50. Flatness refinement
  alone does not solve this. Slicing endpoints inherit the issue. The explicit
  regression documents it; passing that regression is not acceptance of accuracy.
- **Accuracy contracts:** choose geometric/length/query error semantics, behavior
  at the recursion cap, cusp direction conventions and appropriate scale limits.
  A bounded, no-heap correction should preserve explicit workspace failures.
- **Representation and ABI:** discuss reuse of LVGL point/path conventions,
  closed-path topology, exposed structs, uint16_t limits, namespace and versioning.
  Avoid adding parallel representations without maintainer buy-in.
- **Embedded evidence:** collect actual MCU stack high-water marks, worst-case
  workspace/latency, code size and math-library cost. Host benchmarks and sanitizers
  do not establish those properties. No board was flashed for this review.
- **Broader robustness:** extreme-range Bezier intermediate behavior, randomized
  fuzzing, allocation-free target link checks and toolchains beyond the two tested
  Windows hosts remain work. CI defines Linux GCC/Clang sanitizer jobs; this review
  does not claim a new remote CI result for this follow-up.
- **Community/provenance:** confirm maintainer interest, contribution conventions,
  license/authorship review and a minimal upstream patch shape. Retained MIT
  headers/history are evidence, not a declaration of community approval.

## Recommended next milestone

Prepare a **distance-query accuracy and integration-contract milestone** before
expanding scope. Agree a measurable query-error criterion; correct the nonuniform
straight-curve counterexample and test curves with cusps, loops, overshoot and
several coordinate scales against an independent numerical reference. Define
failure/reporting when bounded resources cannot meet it. Preserve the no-heap
contract, rerun geometry regressions, and capture target stack/time/workspace.
Then bring a small design note with the comparison to LVGL's current vector path
API to maintainers before choosing final names or writing an adapter.

## Discussion-material follow-up (2026-09-29)

This documentation follow-up preserves the reviewed core code and public declarations.
It records [API review decisions](api.md#api-review-decisions-2026-09-29), separates
[geometry guarantees](geometry-contract.md) from [embedded resource budgets](embedded-engineering.md),
and adds a generic [motion example](../examples/README.md#point-moving-along-a-path).
The [discussion proposal](upstream-proposal.md) compares three integration options
and makes the accuracy/target-evidence gaps explicit. No adapter is implemented.

There is no technical blocker to an honest exploratory conversation if these
limits accompany the example. Before presenting a merge-ready contribution,
resolve distance inversion/error reporting, agree representation/ownership with
maintainers, and collect embedded resource evidence. No community outreach has
been performed by this task.

Follow-up validation on 2026-09-29:

- GCC 16.1.0 strict C99 Release: 11/11 CTest cases passed, including the new example.
- Clang 22.1.8 Debug with ASan/UBSan: 11/11 CTest cases passed.
- The 121 CSV rows were independently checked for finite values, time/requested
  distance progression, endpoints, unit tangents, orthogonal normals, and proximity
  to the analytic cubic locus. This checks output consistency, not arc-distance accuracy.
- Public header declaration/macro tokens match the preceding review; core source files are
  unchanged. Local documentation links and git diff whitespace checks passed.
- No new target-board, rendered-UI or remote-CI acceptance
  is claimed. This task adds a host example and documentation only.
