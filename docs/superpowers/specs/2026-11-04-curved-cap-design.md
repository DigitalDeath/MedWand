# Curved inward closure

Approved in chat: the cap grows inward from the rim, following local wall tangents and smoothly adapting curvature to close. Preserve the rim and raw captures. Keep flat closure as an option.

Use cubic radial curves assembled into rings, with boundary derivatives estimated from wall triangles and a common planar center tangent. This supports vertical wall tangents without infinite height-field slopes. A fixed dome cannot match varying rim tangents; a general variational patch is more complex than this first bounded implementation.

Require a simple projected rim whose center is in its visibility kernel; reject unsupported shapes. Limit ring mesh work, support cancellation, and reuse closure/manifold/orientation/intersection validation before volume. Tangency is estimated from faceted data and approximated by tessellation. This is a fitted closure, not measured anatomy. Persist the choice and include the resulting triangles in existing export.

## Curvature and preview corrections

User rejected the original independently aimed cubic curves: tangent direction alone and an arbitrary apex did not continue wall curvature. Replace them with a shared local implicit quadratic fitted to five topological wall rings, plus local quadratic residual fits constraining the first and second radial derivatives at each rim vertex. A quintic Hermite fade removes residual value/slope/curvature at the center. Fit only supported convex patches; reject unstable or incompatible fits. Use a rim-plane depth axis so rigid rotations do not change the result. Share angle-weighted vertex normals across wall and cap.

Preserve the previous compatible preview on an empty worker result after append-only acquisition. Keep its revision and capture count, report the failure, and continue withholding stale volume/export. Settings/reset/undo invalidation remains intact.
