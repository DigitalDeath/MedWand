# Implementation log

## 2026-11-04 reconstruction v2

User authorized regular scan → stop → cap/volume workflow. Added radial-cavity reconstruction in `src/radial_reconstruction.*`: voxel-filter a derived copy, infer center/scales from cloud bounds, stereographically map radial directions to a disk, perform cancellable Bowyer-Watson Delaunay triangulation and lift faces to actual XYZ samples. No simulator dimensions, formulas or fixture topology are used in this reconstruction. Local preview is retained. New sessions default to radial mode and 6 mm maximum connection; saved mode persists and legacy JSON without mode remains local preview.

Ruling: explicit +Z opening / single radial layer assumption — permits a coherent shell for the present cavity while preserving radial dents — cannot represent arbitrary undercuts, unknown axis or multiple layers; assumptions are visible in UI/README. Center/scales are inferred, axis is constrained. Radial triangulation is bounded to 20,000 working points and quadratic worst-case CPU work; Pi timings remain unverified. Excess work limits withhold volume instead of silently closing a truncated cloud. Long unsupported connections are left out; sparse/missing-sector/multi-layer clouds do not report volume.

Red/green: the original 6,000-point regular demo cloud initially failed the new closure assertion. At 4 mm connection the radial mesh leaves two unsupported gaps; at 6 mm it forms one valid opening and passes cap/volume checks. Core result: 11,539 wall faces, 86,641 mm³ compared with independent cross-section integration 86,804.3 mm³ (~0.19% difference). Different 20×12×10 ellipsoid agrees with analytic volume within 1.5%; translated/reversed cloud and nonzero sequence offset997 pass. Missing angular wedge, nested layers, sparse40points and demo500/1000points withhold volume. Density sweep:2000/3000/4000/6000 close for this deterministic scan, but6000 is the documented comparison density. Mid-work cancellation passes.

Final verification: `scripts/build-windows.cmd -DMEDWAND_GUI=ON -DMEDWAND_VIEWER=ON` succeeded; CTest5/5 passed in31.98s (core, flat_cap, session, GUI, graphics_failure). GUI test now runs the actual ordinary demo to ≥6000captures, stops it, enables cap, and requires an estimated volume before loading any known fixture. Legacy JSON and current mode persistence assertions pass. Formatter `--check` passed all19C++files. No running user app was killed or unsaved user data discarded. Parent handles independent review and delivery.

## 2026-11-04 flat cap update

Final verification: release build succeeded; CTest 5/5 passed in 16.58 seconds (core, flat_cap, session, GUI, graphics_failure). Formatter check passed for all 17 C++ files. GUI test confirmed test-bowl estimated volume appears and disappears on cap toggle. Core fixture: 45,283.7 mm³ / 45.2837 mL. Synthetic fixture persistence and capped PLY face counts passed. Four-space indentation and single blank lines preserved.

User selected a flat cap first; curved/tangent closure is deferred. Added independent boundary-loop cap/validation/volume module, background integration, orange cap rendering, selectable opening and rim tolerance, JSON settings/provenance and PLY cap export. A labeled synthetic bowl provides an interactive successful case with known topology. The live demo has branching boundaries and volume is correctly withheld; this remains a limitation of the point-cloud reconstruction.

Tests observed failing before implementation: rectangular cap missing, cap settings roundtrip missing, GUI cap control missing. Core fixtures cover box volume (12 mm³), reversed winding/translation, concave prism (9 mm³), nonplanar rim rejection, extra gaps, folded-wall intersections, invalid opening, cancellation and known half-ellipsoid bowl (~45.2837 mL). Added fixture provenance persistence/rejection, PLY face-count and GUI volume on/off checks. Independent read-only review found no material defect; its minor request to assert volume rejection on the live-cloud fixture was implemented. Pi execution and physical accuracy remain unverified.

Plan: docs/superpowers/plans/2026-11-04-viewer.md

2026-11-04: Design approved; implementation authorized. No Git repository or product code exists. Work in the current project folder; no worktree/commits possible without creating a repository. CMake/Ninja and MSVC 14.44 are present. Qt/Eigen must be supplied locally.

Interfaces preflight: Tasks 1/2 share Vec3; Task 3 serializes Snapshot and MeshSettings; Task 4 consumes these plus revision tokens. Keep core independent of Qt; JSON belongs in Qt adapter.

Execution: inline, as requested (assistant programs; user reviews in Cursor).

Task 1: complete. Capture/kinematics tests observed failing at valid capture, then passing through capture/freshness/wrap/known-angle checks. Regression for coordinate limits failed then passed.
Task 2: complete. Reconstruction stub failed planar reconstruction; implementation passed bounded mesh checks. Stronger plane coverage test failed at 21 faces; local Delaunay candidates with deterministic connectivity perturbation now pass >=180 faces. Synthetic concave/open cavity and parallel-sheet checks pass.
Task 3: complete. Persistence test failed at saveSession stub, then JSON round-trip, invalid schema, atomic destination preservation, NaN save rejection and CSV/PLY tests passed.
Task 4: complete. GUI test failed missing controls before implementation; capture/undo/source/demo/graphics tests now pass. Live-scan surface regression failed, then passed after revision handling change.

Ruling: Use a small explicit 3x3 Jacobi eigensolver rather than Eigen — no extra dependency for this one operation — numerical behavior requires planar/curved fixtures (passing).
Ruling: Retain an earlier completed surface during append-only acquisition, labeled with the exact capture count used; only accept results newer than the displayed mesh and from the same destructive-change generation — enables progressive display without starvation — preview can lag the cloud but is clearly labeled. Undo/reset/load/settings invalidate and cancel all older-generation meshes. PLY exports mesh only at current revision; otherwise raw points.
Environment: Initial Qt download hit missing certificate issuer; project-local pip-system-certs restored Windows certificate trust, download succeeded without disabling verification. MSVC debug compiler probe hit PDB manager mismatch; embedded /Z7 debug info resolved it. Release build uses no PDB dependency.
Latest suite: 3 CTest executables passing (core, session, GUI), real Windows OpenGL context. Pi execution remains external/unverified.

## Final verification and review

2026-11-04: Fresh read-only independent review completed. Four material findings fixed:

- Save/load size mismatch: regression with 100,000 angle captures and 256-character source strings failed on an oversized indented save, then passed with compact serialization and a pre-replacement 64 MiB size guard. Large session reopens successfully.
- Space shortcut ownership: focused Undo regression failed, then passed after replacing the global shortcut with unhandled-key propagation. Viewport Space captures; numeric-editor Space does not.
- No-context reporting: ordinary Qt fallback label added outside the GL paint path. Minimal-platform test passes; disabling the fallback made it fail with the expected missing-explanation message, then restoring it made it pass.
- Save/load geometry mismatch: zero-axis save regression failed before write-side profile validation, then passed.

The two minor findings were also addressed: shaders now use ES2-compatible fragment/varying precision; the GUI test resets a running acquisition and confirms that no prior mesh or captures reappear. No deferred review findings. Reviewer declined to certify Pi performance, physical geometry accuracy and mesh correctness; these remain explicitly unverified, with no such claims in the delivered UI/docs.

Additional boundary regression: maximum signed timestamp was accepted without safe continuation headroom; test failed, then passed with a shared bounded timestamp domain on capture/write/load.

Task 5: complete for the Windows simulator deliverable. Final release build succeeded. CTest: 4/4 passed (core, session, GUI, graphics_failure), 13.01 seconds. clang-format dry-run with warnings as errors passed. Real Windows renderer: NVIDIA GeForce RTX 4080, desktop OpenGL 4.6; application uses the documented ES2-common subset with desktop shader syntax. 6,000-point synthetic reconstruction produced 10,738 faces in 56 ms on this PC; no Pi speed inference. Visual inspection of docs/viewer-preview.png confirmed axes/grid, gold cloud, teal surface and all primary controls visible. Demo screenshot: 6,120 captures, 10,955 faces.

Delivery: source in src/, test code in tests/, launcher Run MedicalWand.cmd, instructions README.md. No Git repository/branch exists, so merge/PR/worktree steps do not apply. Existing CAD and historical handoff content retained; CONTEXT.md now points to current implementation status.

## 2026-11-04 � Curved inward cap

Added optional wall-tangent curved closure using cubic radial curves and inward tessellated rings, fixed input/rim vertices, common center tangent plane, visibility-kernel restriction, bounded work and cancellation. Shares the flat cap manifold/orientation/intersection checks and volume calculation. UI toggles flat/curved, disables plane tolerance for curved mode, persists curved_cap in JSON and labels curved PLY faces. No measured closure or physical accuracy is implied.

Verification: structured bowl and ordinary 6,000-point demo closure; near-vertical first-ring tangent; nonplanar fixed rim; reversed winding; unsupported concavity; full vertex budget; cancellation during ring generation; JSON old/new sessions; PLY curved vertices/labels; live Qt demo scan/flat-to-curved cap and graphics failure. Independent read-only review found no actionable correctness issues. Pi execution remains unverified.

## 2026-11-04 � Curvature and disappearing preview fixes

Replaced arbitrary cubic dome with one wall-derived quadratic curvature field, constrained by local first/second radial derivatives and a quintic fade at the center. Added shared angle-weighted rendering normals. The initial dome produced 110,911 mm3 for a known 90,729 mm3 full ellipsoid; corrected continuation gives 90,595 mm3 (about 0.15% faceting error). Per-vertex analytic ellipsoid residual, arbitrary rigid rotation, perturbed-rim tangent, nonplanar-rim and cancellation/budget tests cover the correction. Removing the local derivatives makes the perturbed tangent regression fail (dot 0.834 versus 0.998 with correction).

An empty noncancelled worker result previously replaced a good preview, so the visibility checkbox had no surface to restore. Failed append-only updates now retain the prior compatible mesh and its original revision/count, show the failure diagnostic, and withhold stale numeric volume/surface export. A deterministic worker-result GUI regression failed before the change and passed after, including recovery and generation invalidation. Separate read-only review checked both fixes and prompted the local derivative correction. Actual Pi performance remains unverified.

## 2026-11-04 � Checkbox-triggered surface loss

User narrowed remaining disappearance to toggling points/cap during demo scanning. Cap controls shared the wall-settings invalidation path and erased the wall before rebuilding. Cap-only updates now advance generation/revision and cancel obsolete work while retaining the previous wall, clearing only cap faces and volume. Geometry settings/reset/undo retain full invalidation. Retained wall revisions still prevent stale volume/export.

A separate rendering regression seeds stale depth and a disabled depth write mask; the old paint path could not clear depth and lost every surface pixel. paintGL now restores depth writes before clearing. Added framebuffer pixel checks and a live 3,000+ point demo regression that hides points and repeatedly toggles the cap while scanning. Both targeted regressions were observed failing before their fixes. Read-only review found no actionable issues in these changes.
