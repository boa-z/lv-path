# Upstream readiness review

Review date: 2026-09-29. Scope: standalone path geometry.
The review now includes bounded distance-query accuracy and adversarial host tests. Its host evidence does not constitute an upstream-accepted design or a versioned release. See [benchmark/validation details](benchmarks.md).

## Assessment

**Suitable for an exploratory LVGL design discussion; not ready for a merge-ready
proposal or stable 1.0 API.** The dependency boundary is clean and the library
builds independently. The main remaining work is target-scale evidence and maintainer review of the distance-query budget.

| Area | Finding |
| --- | --- |
| LVGL independence | All core C files build without LVGL; no widget/OS/allocator dependencies in headers or source |
| Application scope | No application-specific exported types/state; removed stale application sizing language from public comments |
| Naming | Generic pg (path geometry) names; legacy path2d include/target names retained compatibly |
| Memory | Caller-owned commands, descriptor, samples and sink; no direct heap calls; lifetime/failure rules documented |
| C portability | C99 strict GCC and Clang builds; public headers separately compile as C99 and C++11 |
| Algorithms | Existing tests plus fixed-seed independent double-reference and targeted regression coverage |
| API stability | Signatures/layouts unchanged in this review; no frozen 1.0 ABI or upstream naming agreement |
| Accuracy | Explicit T/4, 3T/4 and T exact-arithmetic budgets; float-scale qualification and target evidence remain |
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

- **Target evidence:** collect MCU stack high-water marks, code size, libm cost,
  worst-case latency and workspace for an identified target. Host sanitizer and
  benchmark results do not establish those properties.
- **Representation and ABI:** discuss reuse of LVGL point/path conventions,
  closed-path topology, exposed structs, uint16_t limits, namespace and versioning.
  Avoid adding parallel representations without maintainer buy-in.
- **Numerical scope:** validate the exact-arithmetic budget against target-scale
  float behavior and agree whether the appended convergence error is sufficient.
- **Broader robustness:** extreme-range intermediate behavior, randomized fuzzing,
  allocation-free target linking and toolchains beyond the two tested hosts remain.
- **Community/provenance:** confirm maintainer interest, contribution conventions,
  license/authorship review and a minimal upstream patch shape.

## Recommended next milestone

Prepare a **target evidence and integration-contract milestone**. The host accuracy
model and adversarial regression corpus are now in place. Collect MCU stack,
code-size, libm and latency evidence; review the T budget and error enum with LVGL
maintainers; then decide whether to adapt existing LVGL path storage or keep an
optional standalone component.

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
