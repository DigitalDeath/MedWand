# Flat opening cap and estimated volume

User selected a flat cap first; tangent/curved caps are deferred. Existing authorization permits implementation without another approval prompt.

Add an optional flat-cap operation to background mesh processing. Find simple boundary loops, order by perimeter, let the user select an opening, and fit a plane through that rim. Project only derived rim vertices onto the plane if deviations are within an explicit tolerance; preserve raw points. Ear-clip the projected polygon so concave openings are supported. Display the cap separately in orange.

Compute estimated volume only for a single closed manifold shell after consistent orientation, vertex-link validation and triangle-intersection checks. Use centered signed tetrahedra for numerical stability; mm³ / 1000 = mL. A cap does not fill unrelated gaps or establish measurement accuracy. Show diagnostic messages for rejected caps/volume. Results remain attached to their source revision and are invalidated along with surface settings. Include cap triangles in PLY; save cap settings in JSON with defaults for older sessions.

Implementation sequence: (1) failing geometric fixtures; (2) independent cap/validation core; (3) worker/settings/persistence/render/export integration; (4) UI and file regressions; (5) full build/tests, formatter and read-only review. Known-volume fixtures cover a box and concave prism, reversed faces, translations, extra holes, nonplanar rims and invalid geometry.

Constraints: four-space indentation, one blank line between blocks, Pi graphics subset unchanged. Intersection work runs on the CPU worker, uses bounding-volume acceleration, and supports cancellation. Conservative rejection is preferable to an unsupported numeric volume.
