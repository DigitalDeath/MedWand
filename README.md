# MedicalWand viewer

C++17 / Qt6 application for collecting 3D contact points and previewing an open cavity surface. Develop and review here in Cursor; compile the same source on Raspberry Pi 4.

## Run on this Windows computer

Double-click **Run MedicalWand.cmd** after building. Qt is installed locally under `.deps/Qt/6.8.3/msvc2022_64`; the project-local setup utility lives in `.tools/python`. Neither changes the ESP-IDF installation.

Build and test from a PowerShell terminal in this folder:

```powershell
.\scripts\build-windows.cmd -DMEDWAND_GUI=ON -DMEDWAND_VIEWER=ON
```

The current script locates this computer's MSVC 14.44 installation and uses CMake/Ninja from PATH. For another computer, install Qt6 with a matching C++ compiler, then run CMake from that compiler's developer terminal:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt-install-prefix>
cmake --build build
ctest --test-dir build --output-on-failure
```

Qt's `bin` must be on PATH when running the tests or application, unless the executable has been deployed with `windeployqt`. The optional Vulkan-header detection message is harmless: this application uses OpenGL, not Vulkan.

## Try it

1. Choose **Simulated XYZ**, then **Start demo scan**. Gold points accumulate on an open, indented cavity; teal surface patches develop where samples are sufficiently dense.
2. Stop the scan to let the last surface update finish. **Fit view** frames all captured points.
3. Orbit by left dragging, pan by right/middle dragging, zoom with the wheel. Choose top/front/side views or toggle points and surface independently.
4. For manual capture, change X/Y/Z and click **Capture point**. Space captures once; typing in a numeric editor does not capture. **Undo last capture** removes the last point.
5. **Simulated joint angles** exercises the five-axis transform chain with clearly fictional test geometry. These dimensions are not extracted from the SolidWorks assembly.
6. Save/load JSON sessions. A loaded session supplies **Replay loaded session** and **Next replay sample**; capturing replay explicitly labels the new record as replay. Original loaded captures remain unchanged.
7. CSV exports every raw capture. PLY exports the estimated mesh only when it matches the current point/settings revision; while an update is pending, it exports raw points instead. PLY contains millimeter coordinates.

**Pi SPI — not configured** deliberately disables capture. Wiring, sensor framing and read timing, actual link geometry and calibration must be integrated before real acquisition. No ESP32 firmware is needed for this direct-Pi simulator milestone.

## Surface behavior and limits

Reconstruction runs on one CPU worker and starts at most once per second. Append-only acquisition keeps the last completed preview visible; the panel says exactly how many captures that surface uses. Undo, new/load and settings changes cancel and invalidate old previews. A result from an earlier generation cannot reappear.

The algorithm merges nearby points into a separate working cloud, estimates local PCA planes, and creates local tangent-plane Delaunay candidates. Normal, edge-length, degeneracy and edge-incidence checks reject unsupported connections. A tiny deterministic perturbation resolves connectivity ties without changing coordinates. Candidate work is bounded; sparse data can produce no triangles. Raw captures are never merged or moved.

Merge distance and maximum connection distance govern estimated connectivity, not accuracy. Maximum connection must be at least twice merge distance. Reconstruction is capped at 20,000 working points, 32 neighbors per point and 40,000 faces; crowded spatial bins use bounded sampling. The panel reports when a limit affects a result. Raw session limit is 100,000 captures; display uses at most 20,000 representative points.

This local method can still create overlapping or incorrect patches near close sheets, sharp features or uneven coverage. It is an **open preview mesh**. Sparse measurements do not establish a solid or physical accuracy. The optional flat-cap operation below reports estimated volume only when the resulting mesh passes its geometric checks.

## Flat cap and estimated volume

In **Surface preview**, enable **Flat cap + estimated volume**. Openings are ranked by perimeter, largest first; **Opening #** selects the rim. The cap is orange and follows the fitted rim plane. **Rim tolerance** limits how far derived rim vertices may move onto that plane; raw points remain untouched. The rim must be a simple loop with at most 2,048 vertices. Hide points using the existing Points checkbox to inspect the cap and surface.

The app checks closure, edge/vertex manifoldness, a single connected shell, consistent orientation and detected triangle intersections before calculating volume. Volume appears in mm³ and mL (1 mL = 1,000 mm³). It is an estimate defined by the added flat cap, not a measured closure surface or an accuracy certification. Other gaps and branching boundaries cause an explicit diagnostic and withhold volume. Tangent/curved caps are deferred.

For a working geometric test, click **Load flat-cap test bowl** (discard unsaved changes only if you intend to replace the current session). This loads a clearly labeled synthetic, structured half-ellipsoid with radius 38 mm and depth 15 mm, then enables the cap. Its faceted volume is approximately **45,283.7 mm³ / 45.2837 mL**, within 0.3% of the ideal 45.3646 mL. The clean fixture uses known topology; it does not demonstrate that live point-cloud reconstruction is watertight. The current live scan demo has branching boundaries and correctly refuses volume. Capturing/undoing points or starting a scan exits fixture topology mode and returns to ordinary reconstruction.

Cap settings and synthetic fixture provenance persist in JSON; older sessions default to no cap. PLY includes cap triangles and cap/volume comments when exporting a current mesh. Numeric volume disappears while a newer point/settings revision is processing.

## Build on Raspberry Pi 4

First confirm OS/architecture and graphics environment on the actual Pi. Typical Raspberry Pi OS/Debian prerequisites (package names must be checked against the installed release):

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build qt6-base-dev libqt6opengl6-dev libgles2-mesa-dev
cmake -S . -B build-pi -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-pi -j2
ctest --test-dir build-pi --output-on-failure
./build-pi/MedicalWand
```

Run the GUI and GUI test from the Pi's graphical desktop. SSH alone does not provide a display. Core-only tests need no Qt or desktop:

```bash
cmake -S . -B build-core -DMEDWAND_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core -j2
ctest --test-dir build-core --output-on-failure
```

Rendering requests ES 2.0 on Linux and desktop OpenGL 2.1 on Windows. Both paths use the ES 2.0 common feature subset: buffer objects, simple vertex/fragment shaders, points/lines/triangles, no required extensions or advanced shader stages. The Windows shaders have a desktop syntax variant. Actual API/renderer appear below the viewport. **Windows testing does not certify Pi graphics compatibility or performance.**

Pi acceptance still required: verify ES shader compilation and all controls; test 20,000 points/40,000 faces at 1280×720, aiming for 30 FPS and <256 MB application memory; record frame/mesh times, memory and graphics diagnostics. Targets are unverified. For the core timing fixture, run `./build-pi/core_tests` (reports a 6,000-point reconstruction time). Benchmark rendering while orbiting, with reconstruction active; lower budgets if needed.

## Files for review

Code uses four-space indentation, expanded function/lambda bodies and one blank line between logical blocks. Run `python scripts/format-code.py --clang-format <path-to-clang-format>` to maintain these conventions, or add `--check` to verify without editing. The script applies `.clang-format` and single-line block spacing; use it instead of a bare format-on-save command.

- `src/core.*`: snapshots, capture checks, encoder wrapping, configurable rigid transforms and simulation.
- `src/reconstruction.*`: independent CPU surface preview.
- `src/session_io.*`: versioned JSON and CSV/PLY boundaries; atomic file writes.
- `src/viewer.*`: graphics context, shaders, layers and camera.
- `src/main_window.*`: controls, source selection and worker lifecycle.
- `tests/`: core fixtures, 100,000-capture file round-trip, interactive GUI smoke and no-graphics failure tests (four CTest cases).
- `docs/IMPLEMENTATION_LOG.md`: verified results and implementation decisions.
- `docs/superpowers/specs/2026-11-04-point-cloud-viewer-design.md`: approved design.

Source and build files are new; existing context and CAD files are preserved. No Git repository, commit or remote has been created.

## Session format

Version 1 JSON stores millimeter/right-handed conventions, session identity, geometry/calibration profile, reconstruction settings and ordered raw snapshots. Times and sequences are decimal strings to preserve integer precision. XYZ simulation has no encoder counts; five-angle simulation stores all five counts. On replay capture, capture time is the new session acquisition clock and the source explicitly records replay. On load, new acquisition times are offset beyond prior records.

Loads are bounded to 64 MB/100,000 captures, validate finite coordinates and geometry, and replace the active session only after success. Session writes are atomic. Meshes are regenerated rather than persisted, preventing stale derived geometry from being confused with raw measurements.

References: [Qt QOpenGLWidget](https://doc.qt.io/qt-6.8/qopenglwidget.html), [Khronos ES conformance](https://www.khronos.org/conformance/adopters/conformant-products/opengles), [Qt Windows toolchains](https://doc.qt.io/qt-6/windows.html).
