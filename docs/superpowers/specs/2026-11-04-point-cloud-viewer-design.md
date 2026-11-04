# MedicalWand point-cloud viewer and cavity surface

Date: 2026-11-04. Status: approved by user; first Windows simulator implementation delivered. Pi/hardware acceptance remains pending.

## Intent and agreed decisions

Develop a desktop application in Cursor on Windows, then build the same portable source on Raspberry Pi 4 with Raspberry Pi OS 64-bit. Display measured cavity-wall points in a millimeter coordinate system and progressively reconstruct an estimated surface. The Pi defines the graphics and performance constraints.

User-approved choices: C++/Qt6 with a lightweight renderer; a manual Capture Point button; simulated input before hardware integration; incomplete coverage remains visibly incomplete. The user now asks us to assume five angle sensors can be read directly through Pi SPI. This supersedes ESP32/UDP as the initial acquisition architecture, without establishing actual wiring or a validated driver.

## Approaches

1. Selected: Qt6 Widgets with QOpenGLWidget, a small shared rendering feature set, and CPU reconstruction. Gives explicit control of dependencies, graphics and memory. Reconstruction is deliberately a bounded first implementation.
2. Considered: Qt6 plus VTK. More reconstruction facilities, but greater dependency and resource costs. User selected approach 1.

## First release

- One window: 3D viewport, input status, current XYZ, five angles when available, point count, capture controls and reconstruction settings.
- XYZ axes, millimeter grid, orbit/pan/zoom, fit view and standard views. Opaque two-sided surface rendering allows viewing the cavity wall from either side.
- Capture Point records one immutable measurement snapshot. Space is a shortcut when a text editor/control does not own the key. Holding a key does not repeatedly capture.
- Points appear immediately; surface updates asynchronously as local sample density becomes sufficient. Point and surface visibility are independent.
- Undo last capture, new session with unsaved-data confirmation, save/load session, export XYZ CSV and triangle PLY. Point-only PLY remains possible before reconstruction succeeds.
- Simulation mode is prominently labeled. Include deterministic cavity samples with concavity and an opening, manual movement of a simulated tip, and an explicitly started/stopped demo scan. Demo points are never labeled measured hardware data.

## Acquisition and geometry

Separate sources from processing: simulated XYZ for reconstruction demonstrations; simulated five-joint angles for kinematics verification; replay from saved sessions; later Linux SPI source. Windows builds must not include Linux-only SPI headers.

A snapshot includes source, sequence, monotonic acquisition time, validity, available five raw encoder counts, calibrated angles, XYZ and acquisition interval. A capture records the most recent valid complete snapshot only if within the source's allowed age. Missing/invalid/stale measurements disable capture with an explanation. Reading five sensors sequentially is not simultaneous: record first/last read time and enforce a configurable maximum skew when hardware is integrated.

Kinematics uses ordered rigid transforms, a base frame and tip offset, configurable angle signs, encoder zeros and joint-axis geometry. Units: millimeters, radians internally, degrees in the UI; right-handed coordinates. Encoder counts wrap modulo 16384. Do not infer physical dimensions from CAD filenames. A clearly fictional geometry supports testing; real hardware capture remains disabled until a user-supplied calibration/geometry profile is validated. No claim of actual tip accuracy follows from simulation.

The current SolidWorks assembly and parts exist but have not been interpreted. The first implementation does not depend on extracting their geometry. Real SPI mode is a later integration milestone requiring chip-select routing, sensor read format, speed/mode, wiring and calibration details; no dummy driver reports success.

## Progressive reconstruction: proposed implementation choice

The first surface is an estimated open triangle mesh, not a guaranteed solid. Raw captures remain unchanged. Reconstruction uses a separate spatially filtered point set, retaining traceability to captures.

Use a bounded spatial-neighbor index, local covariance/PCA normals and tangent-plane neighbor triangulation. Reject neighborhoods with insufficient support or ambiguous local planes, triangles exceeding configured edge length, degenerate triangles, and triangles incompatible with local normals. Deduplicate faces and reject edges with more than two incident faces. This is a preview algorithm: local tests can still bridge close sheets or produce intersecting patches. It makes no watertightness, topology or accuracy guarantee.

Expose point merge distance and maximum connection distance in millimeters; explain these govern estimated connections, not measurement accuracy. Show points even when no safe triangles are available. Never triangulate the entire cloud as a convex hull or assume the cavity is a height field. Do not infer normals from probe direction as though that direction were the surface normal.

Rebuild after captures, debounced to at most once per second. Use one CPU worker, bounded candidate neighbors, cancellation checks, latest-request coalescing and session/revision identifiers. A result from an older session or point revision cannot replace a newer mesh. Display mesh revision/count and pending status so an older preview cannot be mistaken for the current cloud.

Closed-solid generation is a separate future operation requiring explicit hole-closing assumptions and manifold/self-intersection checks. No volume measurement or closed-solid export claim in release 1. This limitation reflects the user's agreement to preserve missing coverage.

## Graphics and Pi budgets

Use Qt6 Widgets, QOpenGLWidget, buffer objects and simple vertex/fragment shaders, with basic points, lines and triangles. Target the OpenGL ES 2.0 common feature subset; no compute/geometry/tessellation shaders, desktop-only calls or required extensions. Use 16-bit index batches where needed. Disable expensive effects and antialiasing by default.

On Pi request an ES context; on Windows support the same rendering operations with a desktop-compatible shader variant if the Qt installation provides desktop OpenGL. A Windows desktop context does not prove ES compatibility. Log actual API/version/vendor/renderer and shader failures; fail visibly if the required subset is unavailable. Validate an ES context on the Pi before calling the graphics portable.

Provisional engineering targets, not measured performance: interactive 30 FPS at 1280x720 with 20,000 displayed points and 40,000 triangles; initial hard reconstruction cap of 20,000 filtered points, 32 neighbors per point and 40,000 triangles. Preserve raw captures, with a 100,000-capture session limit and explicit limit message rather than silent loss. Display decimation never modifies saved raw data. Target application memory below 256 MB on the Pi and revise budgets from measurements. Throttle redraw when idle. Reconstruction must not block capture or camera controls.

## Data and modules

C++17, CMake, Qt6 Core/Gui/Widgets/OpenGL/OpenGLWidgets; use a small header-only linear algebra dependency such as Eigen for PCA and transforms. Core measurement, kinematics and mesh code has no GUI dependency so it can be checked without a display.

Modules: measurement sources; geometry/calibration; session/capture storage; reconstruction worker; viewer; application controls. Preserve doubles for saved coordinates and kinematics; upload floats relative to a scene origin for rendering.

Versioned JSON session contains units, coordinate convention, source identity, geometry profile, all accepted captures, processing parameters and any saved preview revision. Validate finite values, counts and limits on load; use atomic session writes. CSV includes XYZ, available raw angles/counts and timestamp/sequence/source. Missing joint data in XYZ simulation is explicitly absent. Monotonic times carry a session identifier and are not interpreted as wall-clock dates. Save/load round trips preserve raw captures and their order.

## Delivery and acceptance

1. Establish buildable portable core and viewer, graphics diagnostics, simulated sources and manual capture.
2. Add background open-surface reconstruction, session persistence and exports.
3. Verify the Windows application with actual available tools; report build/runtime blockers accurately.
4. Build and exercise the same version on the actual Pi; record graphics support, frame timings, reconstruction timings and memory. Pi performance remains unverified until this occurs.
5. Integrate real SPI and measured geometry after receiving the hardware interface and calibration details.

Meaningful checks: known-angle kinematics fixtures, count wrapping, capture freshness/validity, rejection of malformed sessions, raw-data round trip, mesh degeneracy/edge constraints, sparse/open/concave/nearby-sheet fixtures, cancelled/stale worker results, and an interactive smoke test of capture, camera and visibility. Compare mesh error against known synthetic geometry without treating that as physical device validation. Sparse input must remain points rather than manufacture a closed cavity. Pi benchmark is a release acceptance item, not inferred from Windows results.

## Remaining external information

Pi OS/Qt versions, RAM, display environment and driver capabilities; Windows compiler/Qt availability; real geometry and calibration; SPI mapping and timing; desired physical accuracy and sampling density. None blocks a clearly labeled simulator build. They block claims of hardware readiness or validated accuracy.

## Documentation basis

- Qt QOpenGLWidget: https://doc.qt.io/qt-6.8/qopenglwidget.html (requested and actual context formats can differ).
- Khronos conformant products: https://www.khronos.org/conformance/adopters/conformant-products/opengles (Pi 4 ES conformance is hardware/driver context, not verification of this user's installation).
- Reconstruction method background: https://www.open3d.org/docs/release/tutorial/geometry/surface_reconstruction.html (reconstruction requires method/parameter assumptions; Open3D is not a runtime dependency).
