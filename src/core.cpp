#include "core.h"
#include <algorithm>

namespace mw {
    double countRadians(int count) {
        return ((count % 16384 + 16384) % 16384) * (2 * pi / 16384.0);
    }

    namespace {
        Vec3 rotate(Vec3 v, Vec3 axis, double angle) {
            axis = unit(axis);
            double c = std::cos(angle), s = std::sin(angle);
            return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1 - c));
        }
    } // namespace

    Vec3 Geometry::tip(const std::array<int, 5> &counts) const {
        Vec3 p = probe;

        for (int i = 4; i >= 0; --i) {
            const auto &j = joints[i];
            p = rotate(p + j.offset, j.axis, j.sign * countRadians(counts[i] - j.zero));
        }

        return base + p;
    }

    Geometry Geometry::fictional() {
        Geometry g;
        g.joints = {Joint{{0, 0, 1}, {0, 0, 0}}, Joint{{0, 1, 0}, {70, 0, 0}}, Joint{{0, 1, 0}, {60, 0, 0}},
            Joint{{1, 0, 0}, {30, 0, 0}}, Joint{{0, 1, 0}, {10, 0, 0}}};
        g.probe = {10, 0, 0};
        return g;
    }

    bool CaptureStore::capture(const Snapshot &s, std::int64_t now) {
        error.clear();

        if (points.size() >= 100000)
            error = "Session limit of 100,000 captures reached";
        else if (!s.valid || !finite(s.position) || s.source.empty() || s.source.size() > 256 ||
            std::max({std::abs(s.position.x), std::abs(s.position.y), std::abs(s.position.z)}) > 100000)
            error = "Invalid measurement";
        else if (s.timestampUs < 0 || s.timestampUs > maxTimestampUs || s.timestampUs > now ||
            now - s.timestampUs > 250000)
            error = "Measurement is stale or future dated";
        else if (s.intervalUs < 0 || s.intervalUs > 5000)
            error = "Joint acquisition interval exceeds 5 ms";
        else if (s.hasAngles && std::any_of(s.counts.begin(), s.counts.end(), [](int c) {
                        return c < 0 || c >= 16384;
                    }))
            error = "Invalid encoder counts";

        if (!error.empty())
            return false;
        points.push_back(s);
        return true;
    }

    Snapshot simulate(std::uint64_t index, std::int64_t now) {
        // Low-discrepancy wall samples: open at the top, with a smooth inward dent.
        const double t = 0.3 + 2.84 * (double((index * 197) % 4096) / 4096.0);
        const double a = 2 * pi * std::fmod(index * 0.6180339887498948, 1.0);
        const double dent = 1 - 0.30 * std::exp(-std::pow((a - pi) / 0.48, 2) - std::pow((t - 1.5) / 0.5, 2));
        Snapshot s;
        s.position = {
            38 * std::sin(t) * std::cos(a) * dent, 38 * std::sin(t) * std::sin(a) * dent, 15 * std::cos(t)};
        s.timestampUs = now;
        s.sequence = index;
        return s;
    }
} // namespace mw
