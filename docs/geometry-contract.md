# Geometry contract

This describes the reviewed implementation behavior, not proposed guarantees. The
current documentation/example follow-up does not change the algorithms. See
[API contracts](api.md) for ownership and errors and [embedded engineering](embedded-engineering.md)
for resource limits. All distances and tolerances use the caller's coordinate units.

## Guarantees at a glance

| Operation | Successful result means | It does not guarantee |
| --- | --- | --- |
| Validate | Nonempty sequence starts with MOVE; consumed coordinates/opcodes are valid | Measurable length, adequate workspace, or representable later arithmetic |
| Flatten | Synchronous MOVE/LINE stream from accepted subdivision leaves | Global error bound, closed topology, or meeting tolerance at the depth cap |
| Measure | Finite, strictly increasing stored distances; total sums retained leaf chords | Exact length, certified lower/upper bound, or a total error bound |
| Position at distance | LUT-interpolated parameter evaluated on the original curve, subject to float roundoff | Position error bounded by flatness tolerance or exactly constant-speed motion |
| Tangent/normal | Finite unit query direction with documented fallback; normal is a rotation | A unique mathematical direction at a cusp, corner or contour discontinuity |
| Slice | Degree-preserving restriction using located endpoint parameters | More accurate distance endpoints than a query or a preserved CLOSE flag |

PG_OK reports completion under these rules, not convergence or an accuracy
certificate. Outputs after an error must not be used.

## Flattening

The input is validated before traversal. MOVE calls move_to, including repeated
same-coordinate MOVEs; contour breaks survive. LINE and CLOSE contribute straight
spans. Quadratic/cubic spans are bisected at t=0.5 with De Casteljau. A curve leaf
is normally accepted when both conditions hold:

1. Each interior control point's perpendicular distance to the chord's infinite
   line is at most the effective tolerance. For a chord <= PG_EPSILON, distance
   from the start point is used instead.
2. Control-polygon length minus chord length is at most that tolerance.

The second test catches collinear overshoot/backtracking missed by the first.
A monotone collinear curve may be accepted without subdivision even when its
parameter speed varies greatly.

Finite positive tolerances below PG_MIN_TOLERANCE (1e-4) are clamped upward;
zero, negative, NaN and infinite tolerances are invalid. At PG_MAX_RECURSION
(default 12), a leaf is accepted even when those tests fail. No separate status
reports that cap. Retained leaves emit their endpoint via line_to;
quad_to/cubic_to are not used. Chords <= PG_EPSILON (1e-6) are skipped. At the
cap, even a nontrivial closed leaf with a tiny endpoint chord can be skipped.
Skipping tiny leaves may join successive retained endpoints: the emitted
polyline's remeasured length need not be bit-identical to the measure LUT.

CLOSE becomes an explicit closing line when measurable; its closed-topology
marker is lost. The stream is not a lossless serialization of stroke join/cap
semantics. Callbacks run synchronously; failure stops traversal and may leave a
prefix in the sink. Discard that prefix after any failure.

## Arc-length calculation

Measurement uses the same subdivision acceptance rules. For every retained leaf,
it adds the Euclidean endpoint chord length (hypotf) to a float accumulator. It
stores cumulative distance, owning command index, and original command parameter
at the leaf end. A distance-zero anchor is stored at the first retained leaf's
start. With L retained leaves, successful initialization needs L+1 samples.
MOVE jumps have no length; CLOSE contributes the closing straight span.

There is no numerical quadrature or analytic arc-length integration. In exact
arithmetic, endpoint chords underestimate each curve segment's arc length;
floating-point accumulation, skipped tiny leaves and capped subdivision mean
the returned value is not a certified lower bound. Per-leaf tolerances do not
establish an overall error bound. Smaller tolerances often improve the estimate
and increase work/storage, but cannot overcome the cap, scaling or float limits.
No successful convergence monotonicity guarantee is made.

Initialization rejects detected non-finite lengths, cumulative overflow or a
positive increment that does not increase the stored float distance. Workspace
exhaustion is a failure, not a successful truncated table. MOVE-only paths and
paths with no retained measurable length return PG_ERR_DEGENERATE.

## Point-at-distance and motion

A query clamps distance, binary-searches the LUT, linearly interpolates t within
the bracket, and evaluates the original curve at t. It interpolates a parameter,
not polyline positions, and does not invert the arc-length integral. The result
lies on the curve up to roundoff; its actual traveled distance can differ
substantially from the requested distance.

For M(0,0) Q(0,0) (100,0), B(t)=(100*t*t,0). The measured length is 100 with only
two samples. Distance 50 yields t=0.5 and x=25; the true half-length point is
x=50. Lowering flatness tolerance leaves this straight curve unchanged. This
is a known blocker for a bounded-accuracy distance API.

The [motion example](../examples/README.md#point-moving-along-a-path) advances
requested distance uniformly with simulated time. It separates timing from
geometry; it must not be presented as guaranteed uniform physical speed.
Sliced endpoints inherit the same limitation. More frequent queries do not
improve the underlying LUT or distance inversion.

## Boundaries, direction and limits

At interior command/contour joints, queries choose the later measurable command
at t=0. No interpolation bridges a MOVE jump. A multi-contour motion consumer
must decide how to handle that instantaneous position jump. Start/end queries
use the first/last measurable geometry, excluding zero-length prefixes/suffixes.
Direction may be discontinuous at corners.

The tangent is the normalized analytic derivative. At zero/near-zero derivative,
the fallback is a local sample span in the same command, then its endpoint chord,
then (1,0). Normal rotation is (-t.y,t.x); its visual side depends on axis
orientation. This is a direction convention, not smoothing or a curvature frame.

All geometry is float-only. Absolute epsilon and minimum tolerance are not
scale-invariant. Finite coordinates do not guarantee representable Bezier
intermediates. Low-level primitives have no error channel; respect their
finite-intermediate precondition. Do not use fast-math flags that invalidate
NaN/Inf checks. There is no guarantee for all finite float inputs, no query-error
bound, and no built-in rescaling, allocator, renderer or parser.
