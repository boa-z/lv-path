# Geometry contract

All distances use caller-defined coordinate units. This contract distinguishes
**approximation error in exact arithmetic** from **floating-point numerical
error**. The core uses float, not interval arithmetic; it does not certify total
error for arbitrary finite coordinates. Read [numerical limits](#floating-point-limits)
before applying these bounds. Public function signatures and storage layouts are unchanged.

## Guarantees at a glance

Let T = max(requested tolerance, PG_MIN_TOLERANCE), with PG_MIN_TOLERANCE = 1e-4.
Let L be the true path length and Lhat the measured length. These bounds assume
exact arithmetic on the supplied command coordinates and successful initialization.

| Quantity | Approximation bound | Numerical qualification |
| --- | --- | --- |
| Total and every LUT prefix length | 0 <= true length - chord sum <= T/4 | Float rounding can change the sign and add error |
| Absolute query at clamped distance d | abs(S(returned parameter) - d) <= 3T/4 | Add subdivision, accumulation, inversion and evaluation roundoff |
| Normalized query f in [0,1] | abs(S(returned parameter) - f*L) <= T | Includes the difference between f*Lhat and f*L; add roundoff |
| Position error on a continuous contour | At most the corresponding arc-distance bound | Does not apply across a disconnected MOVE jump |
| Slice endpoints | Same bounds as queries | Slice construction adds float rounding |
| Tangent/normal | Defined direction convention | No angular-error or speed/acceleration guarantee |
| Flatten | Existing per-leaf flatness policy | Does not acquire the measurement bounds |

S(t) is cumulative geometric arc length to the located command/parameter; MOVE
jumps contribute zero. An absolute query clamps d to [0,Lhat], not [0,L]. An
interior command boundary uses the later command. Near a disconnected contour
boundary, a small distance error can select a spatially remote contour: only the
arc-distance bound survives. On a continuous path, Euclidean separation is at
most the intervening arc length, including at corners, cusps and intersections.

## Measurement and distance inversion

Measurement uses endpoint-chord lengths and a caller-owned LUT, with no numerical
quadrature, root solver, hidden cache or query-time subdivision. The LUT stores
(cumulative distance, command index, parameter); binary search and linear
parameter interpolation locate each query, followed by original-curve evaluation.

**Flatness alone is insufficient.** For M(0,0) Q(0,0) (100,0), B(t)=100*t*t.
Endpoint-only interpolation previously returned x=25 for distance 50. Measurement
now subdivides nonuniform parameter speed even when the geometry is straight.
The regression requires x=50 within 1e-4 at tolerance 1e-4. Flattening this
straight curve may still use a single chord.

### Local interpolation bound

For a De Casteljau leaf with local parameter u in [0,1], controls P_i, degree
n (2 or 3), and chord vector c=P_n-P_0, define:

    D_i = n * (P_(i+1) - P_i)
    E = max_i length(D_i - c)
    G = sum_i length(P_(i+1) - P_i) - length(c)

The derivative is a convex combination of D_i. Therefore
abs(length(B'(u))-length(c)) <= E. Integrating from 0 to u gives
abs(S_leaf(u)-u*length(c)) <= E. This detects changing parameter speed on
straight curves as well as curved geometry. It does not divide by speed and
continues to apply at zero derivatives and cusps.

True leaf length lies between chord and control-polygon lengths, so G bounds
lost length. The implementation computes G as the sum of edge-length minus
projection onto the chord. For positive projections a rationalized cross-product
expression avoids subtraction of nearly equal lengths. A nonconstant leaf with
coincident endpoints is subdivided, never discarded.

### Whole-path budget and bounded preflight

1. Start an internal local tolerance at T; accept curve leaves only when
   E <= local_tolerance/2. LINE/CLOSE have E=G=0.
2. A read-only preflight sums G over **all** accepted leaves, using compensated
   float summation. It writes no LUT samples and invokes no user callback.
3. If the sum exceeds T/4, halve the internal local tolerance and repeat.
   Internal refinement can go below PG_MIN_TOLERANCE; the caller's T stays fixed.
   At most 2*PG_MAX_RECURSION+1 preflight attempts are allowed (25 by default).
4. On convergence, repeat the accepted traversal to fill the caller's table.
   Positive chord lengths use compensated float accumulation. Only exactly zero
   chords are skipped: many tiny positive segments cannot disappear because each
   is <= PG_EPSILON. Total measurable length must exceed PG_EPSILON (1e-6).

In exact arithmetic all G are nonnegative, so every prefix loses at most T/4.
Add local interpolation error E <= T/2 to obtain 3T/4 for absolute queries.
Normalization adds at most abs(f*(L-Lhat)) <= T/4, giving T. Preflight tests the
whole-path budget rather than assigning tiny parameter-proportional budgets to
individual leaves, which would unnecessarily reject narrow reversal regions.

## Failure behavior

- PG_ERR_TOLERANCE_NOT_MET: a measurement leaf reaches PG_MAX_RECURSION without
  acceptance, or bounded preflight attempts cannot meet the total budget. No
  best-effort table is returned. This enum member is appended; existing values
  and public layouts remain unchanged.
- PG_ERR_WORKSPACE_TOO_SMALL: the converged table does not fit. Supply a larger
  caller-owned buffer. More space does not remedy a depth/refinement failure.
- PG_ERR_INVALID_PATH: malformed input or detected nonfinite/unrepresentable
  geometry, overflowing length, or a positive increment that cannot advance the
  stored float distance. Some numerical difficulties can instead reach the
  convergence limit; this is not an exhaustive floating-point diagnostic.
- PG_ERR_DEGENERATE: no total length greater than PG_EPSILON.
- PG_ERR_INVALID_ARG: NULL required pointers or nonpositive/nonfinite tolerance;
  queries also reject NaN distance. Infinities clamp like other out-of-range inputs.

Every initialization failure clears the measure. Workspace contents are
unspecified; queries cannot consume a failed measure. A smaller tolerance may
need more samples or fail; convergence is not promised for every input or every
feasible hand-constructed table. Limits are not silently loosened. Review units,
tolerance, storage or compile-time depth deliberately. Do not consume a partial
result or automatically retry with arbitrary relaxed accuracy.

## Floating-point limits

These are **approximation budgets, not a certified float error bar**. Rounding
input changes the represented path. De Casteljau controls, norms, budget tests,
chord accumulation, parameter interpolation and curve evaluation introduce
further rounding. Compensated sums and rationalized excess reduce common losses;
they do not provide directed rounding or interval enclosures. PG_OK means these
float tests converged and produced usable finite data; it does not detect every
case where roundoff dominates T.

For a physical position allowance A, choose T plus an independently validated
numerical allowance <= A for the intended units, range and toolchain. Recenter
large translated coordinates and scale units before creating float commands.
Near 1e8 binary32 spacing is 8 units: a 1e-4 position promise there is meaningless,
even on a line. Such a line can return PG_OK because its geometric approximation
error is zero. Tightening T cannot recover rounded input. No universal numerical
allowance is specified for all finite float data.

Independent double-reference tests check both arc residual and position, so a
self-intersection cannot hide a query on the wrong branch. Cases include cusps,
loops, retracing, repeated commands, tiny geometry and large/translated coordinates.
Their explicit scale-dependent oracle/roundoff allowances are test envelopes,
not portable error certificates. See [validation](benchmarks.md).
Do not use fast-math options that remove finite/NaN semantics.

## Resource cost

For C commands, N stored samples and depth D, storage is N samples plus the
unchanged measure and borrowed input. With R preflight passes, initialization
cost is O((R+1)*C*2^D) worst case, R <= 2D+1. Recursion is O(D), with at most D+1
active subdivision frames. Preflights reuse constant-size accumulators and the
same stack, without allocation. Final traversal needs one anchor plus one sample
per positive leaf. The stricter contract can need many more samples than the
former flatten-derived LUT, even for a straight Bezier.

Queries remain O(log N + C), with no adaptive recursion or extra caller workspace.
Slices use the same located parameters. Stack bytes, libm time and MCU latency
need target measurement; see [embedded engineering](embedded-engineering.md).

## Flattening, direction and motion

Flattening retains its policy: perpendicular control-point deviation and
control-polygon excess must each be <= T, except that at the depth cap it accepts
a leaf even if the tests fail. It skips chords <= PG_EPSILON and emits MOVE/LINE
synchronously. Its output is not necessarily the measurement polyline;
measurement uses stricter acceptance and explicit convergence failure. Flatten
does not guarantee global error or convergence at the cap. Discard partial output
after callback failure. CLOSE becomes a line, losing topology metadata.

Tangents use the normalized analytic derivative. Near a zero derivative the
fallback is a local sampled span in the same command, then the command chord,
then (1,0). Normal is (-t.y,t.x). Cusps/corners have no guaranteed unique direction
or bounded angular error. MOVE jumps are not interpolated or smoothed.

The [motion example](../examples/README.md#point-moving-along-a-path) advances
requested distance uniformly. Its position has the stated approximation budget;
instantaneous speed need not be constant because LUT interpolation does not make
the arc-distance derivative exact. More animation frames improve temporal
sampling, not table accuracy. The GIF visualizes C example output on the host;
it is not an LVGL renderer or board demonstration.
