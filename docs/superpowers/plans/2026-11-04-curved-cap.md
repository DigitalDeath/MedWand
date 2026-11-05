# Curved Cap Implementation Plan

Goal: add tangent-informed inward closure to the C++17/Qt6 viewer.
Spec: docs/superpowers/specs/2026-11-04-curved-cap-design.md
Architecture: extend the existing independent cap core, reusing its boundary extraction and final validation. Add a curved selection alongside existing flat enablement.
Constraints: four spaces, expanded bodies, single blank lines; CPU worker cancellation; raw points and rim stay fixed.
Review focus: vertical slopes; concave/non-star rims; reversed wall faces; nonplanar rims; settings and fixture consistency.

- [x] Add failing curved bowl, preserved rim, cancellation and orientation tests to tests/cap_tests.cpp; build to confirm missing API.
- [x] Extend closeFlatOpening internals with curved mode and expose closeCurvedOpening; form tangent Bezier rings and run existing volume validation. Run cap tests.
- [x] Add MeshSettings.curvedCap, UI toggle, fixture routing, JSON persistence and session regression. Build and run all tests.
- [x] Verify curved demo, unsupported geometry, formatting and diff; document limitations and results.
