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

The default **Radial cavity (+Z opening)** mode derives a center and scale from the point cloud, voxel-filters a working copy, stereographically projects radial directions to a 2D disk, Delaunay-triangulates that disk, then lifts triangles back to the actual XYZ samples. This gives a coherent shell without substituting a known model or taking the convex hull in XYZ. Radial inward dents remain represented. It assumes one wall per direction around that center, full-around wall and floor coverage, and one opening toward +Z. It cannot reconstruct hidden undercuts or multiple radial layers reliably. These assumptions are displayed in the UI; use **Local surface preview** for geometry outside them.

Local preview retains the previous PCA/tangent-plane triangulation and can produce branching boundaries. It is useful for inspecting arbitrary samples but may not support volume. Raw captures are never merged or moved by either reconstruction mode. Cap projection affects only derived rim vertices within the selected tolerance.

Merge distance and maximum connection distance govern estimated connectivity, not accuracy. Maximum connection must be at least twice merge distance. New sessions default to radial reconstruction with a **6 mm** connection limit. Triangles longer than that are omitted, leaving gaps instead of closing unsupported areas. Both modes cap working points at 20,000 and faces at 40,000; radial mode automatically increases the working voxel distance when needed and re-filters all captures, reserving room for the cover when closure is enabled. It reports the effective preview resolution and preserves raw captures and saved settings. Coarsening never exceeds half the maximum connection distance; an unsupported over-limit result still withholds volume. Its cancellable CPU triangulation has quadratic worst-case work, so Pi timing still needs measurement. Local mode uses at most 32 neighbors. Raw session limit is 100,000 captures; display uses at most 20,000 representative points.

This local method can still create overlapping or incorrect patches near close sheets, sharp features or uneven coverage. It is an **open preview mesh**. Sparse measurements do not establish a solid or physical accuracy. The optional flat-cap operation below reports estimated volume only when the resulting mesh passes its geometric checks.

## Opening caps and estimated volume

In **Surface preview**, enable **Close opening + estimated volume**. Openings are ranked by perimeter, largest first; **Opening #** selects the rim. The cap is orange and follows the fitted rim plane. **Rim tolerance** limits how far derived rim vertices may move onto that plane; raw points remain untouched. The rim must be a simple loop with at most 2,048 vertices. Hide points using the existing Points checkbox to inspect the cap and surface.

The app checks closure, edge/vertex manifoldness, a single connected shell, consistent orientation and detected triangle intersections before calculating volume. Volume appears in mm³ and mL (1 mL = 1,000 mm³). It is an estimate defined by the added flat cap, not a measured closure surface or an accuracy certification. Other gaps and branching boundaries cause an explicit diagnostic and withhold volume. Enable **Curved lid (follow wall curvature)** for a nonflat closure. It fits a shared implicit quadratic curvature field to the nearby wall band and adds local first- and second-derivative corrections at the rim. The lid grows inward in a radial chart; its center follows the fitted surface instead of an arbitrary dome height. Local corrections fade with zero value, slope and curvature at the center. Raw points and derived rim vertices remain fixed in this mode; **Rim tolerance** applies only to flat caps. Shared vertex normals provide smooth lighting across the wall/lid seam. Tangents and curvature are estimates from sampled walls, not measured closure geometry.

Curved closure requires a simple opening visible from its center and locally convex wall curvature that supports a stable quadratic fit. Unsupported concave rims, insufficient samples, excessive fit residuals, folds and intersections produce diagnostics and withhold volume. The combined mesh stays within 20,000 vertices and 40,000 faces, with at least eight inward rings. The cap mode persists in JSON and curved triangles/vertices are included in PLY with a curved-cap label. Older sessions retain flat-cap behavior.

If an append-only reconstruction fails or reaches its work limit, the last successful compatible surface remains visible with an explicit diagnostic and its original capture count. It is not treated as current: numeric volume is withheld and PLY exports raw points until a current surface succeeds. Changing cap options removes the old cap and volume immediately but retains the previous wall while the new closure builds. Reset, undo and wall reconstruction settings still invalidate incompatible surfaces.

For the normal workflow, keep **Radial cavity (+Z opening)** selected, run **Start demo scan**, accumulate about **6,000 points**, stop the demo, then enable **Close opening + estimated volume**. At the default 6 mm connection limit the ordinary captured demo cloud closes and gives approximately **86.6 mL**. An independent numerical integration of the simulated dented cavity gives 86.8043 mL; the 6,000-point mesh differs by about 0.2%. That validates this simulated dataset, not physical device accuracy. Sparse or missing-sector scans still withhold volume. More points alone do not justify an invalid radial assumption. Older saved sessions retain local preview unless you select radial mode.

For a separate geometric check, **Load cap test bowl** loads the clearly labeled structured half-ellipsoid fixture (38 mm radius, 15 mm depth). Its faceted volume is approximately **45.2837 mL**, within 0.3% of the ideal 45.3646 mL. This known-topology fixture remains distinct from the regular demo, whose new closure is reconstructed from captured points. Capturing/undoing points or starting a scan exits fixture mode.

Cap settings and synthetic fixture provenance persist in JSON; older sessions default to no cap. PLY includes cap triangles and cap/volume comments when exporting a current mesh. Numeric volume disappears while a newer point/settings revision is processing.

## STL export

Enable **Close opening + estimated volume** and select the flat or curved cover. Stop acquisition and wait for a valid estimated volume. **Export closed cavity + cover as STL** writes one closed, outward-oriented triangular surface containing both the cavity wall and its cover. The button stays disabled for incomplete or stale results. STL coordinates are in millimeters; select millimeters when importing into CAD or a slicer because STL has no standard units field. The inferred cover is part of the exported geometry. CSV and PLY remain available for raw/open geometry.

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

## Raspberry Pi Debian installer

Build the installer on the target Pi OS release. CPack records the native Debian architecture and discovers linked runtime dependencies with dpkg-shlibdeps. For the current Debian 13 ARM64 Pi, run:

```bash
sudo apt install dpkg-dev
cd ~/MedWand
git pull --ff-only
cmake -S . -B build-pi -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-pi -j2
ctest --test-dir build-pi --output-on-failure
cpack --config build-pi/CPackConfig.cmake -G DEB -B build-pi/packages
```

The initial installer is `build-pi/packages/medicalwand_0.1.0-1_arm64.deb`. Copy it to another Pi running the same Debian release and architecture, then install with:

```bash
sudo apt install ./medicalwand_0.1.0-1_arm64.deb
```

APT installs the required Qt/OpenGL runtime dependencies (an internet connection or configured package mirror is needed when they are missing). The target Pi does not need a compiler or Qt development packages. Launch MedicalWand from the desktop application menu or run `MedicalWand` in a desktop terminal. Uninstall with `sudo apt remove medicalwand`. Sessions saved in your own folders are not part of the package. This package is not a Windows installer, does not install Raspberry Pi OS or configure SPI, and is not guaranteed compatible with older Debian releases. Native package creation and installation must still be verified on the Pi.

## Files for review

Code uses four-space indentation, expanded function/lambda bodies and one blank line between logical blocks. Run `python scripts/format-code.py --clang-format <path-to-clang-format>` to maintain these conventions, or add `--check` to verify without editing. The script applies `.clang-format` and single-line block spacing; use it instead of a bare format-on-save command.

- `src/core.*`: snapshots, capture checks, encoder wrapping, configurable rigid transforms and simulation.
- `src/reconstruction.*`: independent CPU surface preview.
- `src/session_io.*`: versioned JSON and CSV/PLY boundaries; atomic file writes.
- `src/viewer.*`: graphics context, shaders, layers and camera.
- `src/main_window.*`: controls, source selection and worker lifecycle.
- `tests/`: core/cap geometry fixtures, 100,000-capture file round-trip, ordinary scan/stop/cap GUI regression and no-graphics failure tests (five CTest cases).
- `docs/IMPLEMENTATION_LOG.md`: verified results and implementation decisions.
- `docs/superpowers/specs/2026-11-04-point-cloud-viewer-design.md`: approved design.

Source and build files are new; existing context and CAD files are preserved. No Git repository, commit or remote has been created.

## Session format

Version 1 JSON stores millimeter/right-handed conventions, session identity, geometry/calibration profile, reconstruction settings and ordered raw snapshots. Times and sequences are decimal strings to preserve integer precision. XYZ simulation has no encoder counts; five-angle simulation stores all five counts. On replay capture, capture time is the new session acquisition clock and the source explicitly records replay. On load, new acquisition times are offset beyond prior records.

Loads are bounded to 64 MB/100,000 captures, validate finite coordinates and geometry, and replace the active session only after success. Session writes are atomic. Meshes are regenerated rather than persisted, preventing stale derived geometry from being confused with raw measurements.

References: [Qt QOpenGLWidget](https://doc.qt.io/qt-6.8/qopenglwidget.html), [Khronos ES conformance](https://www.khronos.org/conformance/adopters/conformant-products/opengles), [Qt Windows toolchains](https://doc.qt.io/qt-6/windows.html).
