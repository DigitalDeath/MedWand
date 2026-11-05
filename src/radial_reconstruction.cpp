#include "radial_reconstruction.h"
#include <algorithm>
#include <map>
#include <set>

namespace mw {
    namespace {
        struct Point {
            double x, y;
        };

        struct Triangle {
            int a, b, c;
            double x, y, radiusSquared;
        };

        bool triangle(int a, int b, int c, const std::vector<Point> &points, Triangle &out) {
            Point p = points[a], q = points[b], r = points[c];
            double ux = q.x - p.x, uy = q.y - p.y, vx = r.x - p.x, vy = r.y - p.y;
            double determinant = 2 * (ux * vy - uy * vx);

            if (std::abs(determinant) < 1e-12)
                return false;

            if (determinant < 0) {
                std::swap(b, c);
                std::swap(ux, vx);
                std::swap(uy, vy);
                determinant = -determinant;
            }

            double u = ux * ux + uy * uy, v = vx * vx + vy * vy;
            double x = (u * vy - v * uy) / determinant, y = (ux * v - vx * u) / determinant;
            out = {a, b, c, p.x + x, p.y + y, x * x + y * y};
            return std::isfinite(out.radiusSquared);
        }
    } // namespace

    Mesh reconstructRadial(
        const std::vector<Vec3> &input, MeshSettings settings, const std::function<bool()> &cancel) {
        Mesh mesh;
        auto fail = [&](const char *reason) {
            mesh.faces.clear();
            mesh.capStatus = reason;
            return mesh;
        };

        if (!std::isfinite(settings.mergeDistance) || settings.mergeDistance < 0.01 ||
            !std::isfinite(settings.maxEdge) || settings.maxEdge < 2 * settings.mergeDistance ||
            settings.maxEdge > 100)
            return fail("Invalid radial reconstruction settings.");
        std::set<std::array<int, 3>> cells;
        Vec3 lo{1e100, 1e100, 1e100}, hi{-1e100, -1e100, -1e100};

        const size_t vertexBudget = settings.flatCap ? 16000 : 20000;
        double workingMerge = settings.mergeDistance;

        for (;;) {
            mesh.vertices.clear();
            cells.clear();
            lo = {1e100, 1e100, 1e100};
            hi = {-1e100, -1e100, -1e100};
            mesh.effectiveMergeDistance = workingMerge;
            bool overflow = false;

            for (size_t i = 0; i < input.size(); ++i) {
                if ((i % 128) == 0 && cancel()) {
                    mesh.cancelled = true;
                    return mesh;
                }

                Vec3 p = input[i];

                if (!finite(p) || std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) > 100000)
                    continue;
                std::array<int, 3> key{int(std::floor(p.x / workingMerge)),
                    int(std::floor(p.y / workingMerge)), int(std::floor(p.z / workingMerge))};

                if (!cells.insert(key).second)
                    continue;

                if (mesh.vertices.size() >= vertexBudget) {
                    overflow = true;
                    break;
                }

                mesh.vertices.push_back(p);
                lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
                hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
            }

            if (!overflow)
                break;
            double next = std::min(workingMerge * 1.25, settings.maxEdge * 0.5);

            if (next <= workingMerge) {
                mesh.limited = true;
                return fail("Radial preview cannot fit within the work limit at this connection distance.");
            }

            workingMerge = next;
        }

        if (mesh.vertices.size() < 100)
            return fail("More cavity coverage is needed (at least 100 filtered points).");
        Vec3 center = (lo + hi) * 0.5, half = (hi - lo) * 0.5;

        if (std::min({half.x, half.y, half.z}) < workingMerge * 2)
            return fail("Radial cavity needs a three-dimensional wall, not a plane.");
        std::vector<Point> projected;
        std::array<bool, 24> sectors{};
        std::map<std::array<int, 2>, std::pair<double, double>> radialBins;
        bool floor = false;

        for (auto p : mesh.vertices) {
            Vec3 d = p - center;
            d = {d.x / half.x, d.y / half.y, d.z / half.z};
            double radius = norm(d);

            if (radius < 0.1)
                return fail("Points near the radial center are incompatible with this cavity model.");
            Vec3 direction = d * (1 / radius);
            double phi = std::atan2(direction.y, direction.x) + pi;
            double theta = std::acos(std::clamp(direction.z, -1.0, 1.0));
            auto key = std::array<int, 2>{int(phi / 0.04), int(theta / 0.04)};
            auto it = radialBins.find(key);

            if (it == radialBins.end())
                radialBins[key] = {radius, radius};
            else {
                it->second.first = std::min(it->second.first, radius);
                it->second.second = std::max(it->second.second, radius);

                if (it->second.second > it->second.first * 1.18)
                    return fail("Multiple radial wall layers detected; use local preview for this geometry.");
            }

            if (std::abs(direction.z) < 0.55)
                sectors[std::min(23, int(phi * 24 / (2 * pi)))] = true;

            if (direction.z < -0.85)
                floor = true;
            double denominator = 1 - direction.z;

            if (denominator < 0.003)
                return fail("Opening must face +Z and leave a clear unsampled pole.");
            projected.push_back({direction.x / denominator, direction.y / denominator});
        }

        if (!floor || std::count(sectors.begin(), sectors.end(), true) != 24)
            return fail("Scan lacks full-around wall/floor coverage; volume is unavailable.");
        double extent = 1;

        for (auto p : projected)
            extent = std::max({extent, std::abs(p.x), std::abs(p.y)});
        int count = int(projected.size());
        projected.push_back({-32 * extent, -16 * extent});
        projected.push_back({32 * extent, -16 * extent});
        projected.push_back({0, 32 * extent});
        Triangle super;
        triangle(count, count + 1, count + 2, projected, super);
        std::vector<Triangle> triangles{super};

        for (int i = 0; i < count; ++i) {
            if (cancel()) {
                mesh.cancelled = true;
                return mesh;
            }

            std::map<std::pair<int, int>, int> boundary;
            std::vector<Triangle> kept;
            kept.reserve(triangles.size() + 6);
            Point p = projected[i];

            for (const auto &t : triangles) {
                double dx = p.x - t.x, dy = p.y - t.y;

                if (dx * dx + dy * dy <= t.radiusSquared * (1 + 1e-12)) {
                    for (auto e : std::array<std::pair<int, int>, 3>{{{t.a, t.b}, {t.b, t.c}, {t.c, t.a}}})
                        ++boundary[std::minmax(e.first, e.second)];
                } else
                    kept.push_back(t);
            }

            for (auto entry : boundary)

                if (entry.second == 1) {
                    Triangle t;

                    if (triangle(entry.first.first, entry.first.second, i, projected, t))
                        kept.push_back(t);
                }

            triangles = std::move(kept);
        }

        size_t rejected = 0;

        for (auto t : triangles) {
            if (t.a >= count || t.b >= count || t.c >= count)
                continue;
            Vec3 a = mesh.vertices[t.a], b = mesh.vertices[t.b], c = mesh.vertices[t.c];

            if (std::max({norm(a - b), norm(b - c), norm(c - a)}) > settings.maxEdge) {
                ++rejected;
                continue;
            }

            if (norm(cross(b - a, c - a)) < 1e-9) {
                ++rejected;
                continue;
            }

            mesh.faces.push_back({t.a, t.b, t.c});
        }

        mesh.capStatus = rejected
            ? "Radial shell has gaps beyond connection distance; more measurements are needed."
            : "Radial cavity shell: one wall per direction; opening along +Z.";
        return mesh;
    }
} // namespace mw
