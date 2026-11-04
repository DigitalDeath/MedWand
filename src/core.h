#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace mw {
    constexpr double pi = 3.14159265358979323846;
    constexpr std::int64_t maxTimestampUs = std::numeric_limits<std::int64_t>::max() / 2;

    struct Vec3 {
        double x = 0, y = 0, z = 0;
    };

    inline Vec3 operator+(Vec3 a, Vec3 b) {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }

    inline Vec3 operator-(Vec3 a, Vec3 b) {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }

    inline Vec3 operator*(Vec3 a, double s) {
        return {a.x * s, a.y * s, a.z * s};
    }

    inline double dot(Vec3 a, Vec3 b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    inline double norm(Vec3 a) {
        return std::sqrt(dot(a, a));
    }

    inline Vec3 cross(Vec3 a, Vec3 b) {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }

    inline Vec3 unit(Vec3 a) {
        double n = norm(a);
        return n > 1e-12 ? a * (1 / n) : Vec3{};
    }

    inline bool finite(Vec3 a) {
        return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
    }

    double countRadians(int count);

    struct Joint {
        Vec3 axis;
        Vec3 offset;
        int zero = 0;
        int sign = 1;
    };

    struct Geometry {
        std::array<Joint, 5> joints;
        Vec3 base{}, probe{};
        bool validated = false;
        std::string name = "Fictional test geometry";
        Vec3 tip(const std::array<int, 5> &counts) const;
        static Geometry fictional();
    };

    struct Snapshot {
        Vec3 position;
        std::array<int, 5> counts{};
        bool hasAngles = false, valid = true;
        std::int64_t timestampUs = 0, intervalUs = 0;
        std::uint64_t sequence = 0;
        std::string source = "SIMULATED XYZ";
    };

    struct CaptureStore {
        std::vector<Snapshot> points;
        std::string error;
        bool capture(const Snapshot &, std::int64_t nowUs);
    };

    Snapshot simulate(std::uint64_t index, std::int64_t nowUs);

    struct Revision {
        std::uint64_t value = 1;
        std::uint64_t token() const {
            return value;
        }

        void change() {
            ++value;
        }

        bool accepts(std::uint64_t token) const {
            return token == value;
        }
    };
} // namespace mw
