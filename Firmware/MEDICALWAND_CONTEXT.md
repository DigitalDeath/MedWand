# MedicalWand — Project Context for Codex

**Scope:** MedicalWand device only. This document deliberately excludes Raspberry Pi installation, Debian configuration, Signal, mouse settings, and unrelated projects.

**Source and confidence:** Consolidated from prior project conversations available to ChatGPT, not a verbatim transcript or a verified snapshot of the current repository. Distinguish confirmed user requirements from proposals and items still needing verification. Inspect actual code, schematics, and mechanical drawings before making changes.

## 1. Purpose and geometry

MedicalWand is a **five-axis articulated contact-probe 3D digitizer** intended to map the internal surface of a cavity. The discussed working volume is approximately **80 × 80 × 30 mm**, accessible through an opening approximately **10 mm** across. The operator touches the probe tip to the surface and the system records its 3D position, ultimately reconstructing the sampled cavity surface.

These dimensions are project targets, not proof of completed mechanical design or achieved accuracy.

## 2. Articulated sensing hardware

Five **MT6701 14-bit absolute magnetic angular sensors** measure the articulated mechanism:

1. Base yaw
2. Shoulder pitch
3. Elbow pitch
4. Forearm roll
5. Wrist pitch

An **ESP32-S3** acquires the joint angles and the contact indication. The proposed arrangement uses a shared sensor bus with separate chip-select signals, but **verify MT6701 interface details and whether each sensor's SSI/SPI output is properly isolated when unselected** before committing to a shared bus. Earlier discussions used both I2C and SPI with MT6701 in other contexts; do not assume that an old pinout or driver belongs to this device.

Critical mechanical concerns: bearing play, sensor-to-shaft coupling, structural stiffness, deflection, precise link lengths and axes, joint offsets, and probe-tip geometry. These dominate achievable 3D accuracy alongside encoder quantization.

## 3. Contact detection — latest stated requirement

**Detect contact, not force magnitude.** The operator controls pressure. The detector must respond to **axial and lateral** tip contact, ideally regardless of lateral direction.

Selected piezo component from the latest discussion:

- **STEMINC SMD05T04R111WL**
- Approximately **5 mm diameter × 0.4 mm thickness**
- Approximately **450 kHz** resonance
- Approximately **450 pF** capacitance
- Wired piezoelectric disc

Proposed mechanical concept: a preloaded flexure / force-conversion arrangement so both axial and side contact produce strain or compression at the piezo. Proposed signal chain: high-input-impedance front end, filtering/conditioning, comparator or threshold detector, ESP32-S3 digital contact input. Record the event alongside a coherent set of five angles and timestamp.

**Important limitation:** A piezo element inherently responds to *changes* in stress; it is not a reliable static-contact sensor without an additional strategy. Evaluate contact-onset detection, slow contact, direction dependence, mechanical preload, false triggers, and vibration. Do not represent the piezo scheme as validated.

## 4. Proposed system architecture

### Device / ESP32-S3

- Read five joint encoders.
- Detect or timestamp probe contact.
- Sample at a proposed **250–500 Hz** (not yet a confirmed final rate).
- Transmit joint angles, contact state/event, timestamps, and sequence numbers using a **binary UDP protocol over Wi-Fi**.
- Use **ESP-IDF**, not Arduino, for new firmware.

### Host-side MedicalWand application

A proposed architecture places 3D processing in a **C++ / Qt6 / OpenGL** application. The host receives UDP packets, validates ordering and timestamps, applies joint calibration and forward kinematics, calculates probe-tip XYZ, records contact points, displays the developing geometry, reconstructs a surface, and exports data.

Previously discussed export formats: **STL, OBJ, PLY**, plus raw point-cloud data. Python was discussed as an alternative/prototyping tool, but no final implementation language should be assumed without examining the repository.

The choice of a Raspberry Pi as one possible host is an architectural possibility, **not** a request to work on Pi installation or operating-system setup. The current Codex workspace is on **Windows**; inspect the actual repository and target deployment requirements.

## 5. Forward kinematics and calibration

Model the wand as an ordered chain of calibrated rigid transforms, incorporating the five measured joint angles, joint-axis orientations, link lengths, mounting offsets, and the probe-tip offset. Conceptually:

`T_tip = T_base · T_1(q1) · T_2(q2) · T_3(q3) · T_4(q4) · T_5(q5) · T_probe`

The transform definitions, order, frames, and units must come from the actual mechanical design. Do not invent link lengths or axis signs.

Calibration topics:

- Joint zero offsets and angle directions.
- Magnetic sensor alignment and absolute-angle wrap handling.
- Link geometry and nonideal joint-axis alignment.
- Probe tip offset and possible interchangeable tip geometry.
- Repeatability, backlash, structural deflection, and overall error budget.
- Contact-trigger latency relative to joint-angle sampling.

Capture coherent joint-angle snapshots for contact events; avoid mixing angles sampled at materially different times.

## 6. Surface acquisition and reconstruction

- Transform each accepted contact sample into a 3D point.
- Keep the raw joint angles, timestamp, sequence number, contact indication, and calculated XYZ for traceability.
- Provide live 3D visualization of points and the emerging cavity geometry.
- Support filtering of duplicates/outliers and point-density inspection.
- Reconstruct a mesh from the sampled surface, with special attention to incomplete coverage, openings, and concave cavities.
- Export raw point clouds and mesh formats (STL / OBJ / PLY as appropriate).

Mesh method, sampling strategy, units, coordinate convention, and acceptance criteria have **not** been established in the retrieved history; verify rather than assume.

## 7. Clinical-use-related design constraints

Because the device is intended for contact with tissue, the design must consider an atraumatic probe, cleanability/sterility, contact force and deflection, electrical safety, biocompatibility of contacting materials, and validation of accuracy and detection reliability. No clinical approval or performance validation is implied by these notes.

## 8. Current state: what is known versus unverified

**Known project intent:** five-axis articulated cavity digitizer; five MT6701 encoders; ESP32-S3 acquisition; direction-independent contact-onset detection using the selected piezo as a candidate; Wi-Fi/UDP transport; host-side kinematics, 3D display, and mesh export.

**Proposed but not verified as implemented:** shared SPI/SSI wiring; sampling rate; exact packet structure; Qt6/OpenGL host stack; comparator circuit; mechanical flexure; calibration algorithm; mesh reconstruction pipeline.

**Unknown until the repository is inspected:** actual PCB revision, pin mapping, encoder driver, firmware build state, link dimensions, contact circuit, calibration parameters, host code, packet schema, tested performance, and next unfinished milestone.

## 9. Instructions to Codex

1. Read this context and inspect the entire current MedicalWand repository before changing anything.
2. Identify what is already implemented, what is only specified, and what conflicts with this historical context.
3. Treat the latest requirements above as design intent, but prefer actual current source files for implementation facts. Flag conflicts for resolution rather than silently overwriting code.
4. Focus **exclusively on the MedicalWand device and its associated firmware, electronics, kinematics, data transport, visualization, and reconstruction software**. Do not spend time on Debian, Raspberry Pi package installation, desktop configuration, or unrelated projects.
5. For ESP32 firmware, use **native ESP-IDF, never Arduino**. When asked to write a program, provide complete buildable files, not fragments, unless only a section is requested.
6. First produce a concise inventory of repository modules, verified current status, unresolved design decisions, and the most useful next development task.

## 10. Suggested opening prompt for Codex

> Read `MEDICALWAND_CONTEXT.md` in full, then inspect this project's actual source code, schematics, and other available files. This context summarizes earlier ChatGPT discussions about the MedicalWand device only. Separate confirmed requirements from unimplemented proposals, report the current state and any conflicts, and recommend the next concrete development step. Do not change files until you have presented your assessment. Ignore unrelated Raspberry Pi installation and OS-configuration topics.
