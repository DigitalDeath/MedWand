#include "core.h"
#include "reconstruction.h"
#include <chrono>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
using namespace mw;
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}

int main() {
    try {
        CaptureStore store;
        auto s = simulate(0, 1000);
        check(store.capture(s, 1000), "valid capture");
        check(!store.capture(s, 2000000), "stale rejected");
        check(!store.capture(s, 999), "future rejected");
        s.position.x = std::numeric_limits<double>::quiet_NaN();
        check(!store.capture(s, 1000), "NaN rejected");
        s = simulate(0, 1000);
        s.valid = false;
        check(!store.capture(s, 1000), "invalid rejected");
        check(store.points.size() == 1, "failed captures do not mutate");
        check(std::abs(countRadians(16384) - countRadians(0)) < 1e-12, "wrap");
        Geometry geometry = Geometry::fictional();
        std::array<int, 5> counts{};
        auto tip = geometry.tip(counts);
        check(norm(tip - Vec3{180, 0, 0}) < 1e-8, "straight fixture");
        counts[0] = 4096;
        check(norm(geometry.tip(counts) - Vec3{0, 180, 0}) < 1e-8, "yaw fixture");
        auto first = simulate(0, 0).position;
        check(std::abs(first.x - 11.229767853) < 1e-6 && std::abs(first.z - 14.330047337) < 1e-6,
            "simulation known fixture");
        s = simulate(0, 1000);
        s.intervalUs = 5001;
        check(!store.capture(s, 1000), "incoherent joint interval rejected");
        s = simulate(0, 1000);
        s.position = {1e20, 0, 0};
        check(!store.capture(s, 1000), "unsupported coordinates rejected before rendering");
        s = simulate(0, std::numeric_limits<std::int64_t>::max());
        check(!store.capture(s, s.timestampUs), "timestamp headroom protects continuation clock");
        MeshSettings settings;
        auto empty = reconstruct({}, settings, [] {
            return false;
        });

        check(empty.faces.empty(), "sparse stays empty");
        std::vector<Vec3> plane;

        for (int y = 0; y < 12; ++y)

            for (int x = 0; x < 12; ++x)
                plane.push_back({double(x), double(y), 0});
        auto mesh = reconstruct(plane, settings, [] {
            return false;
        });

        check(mesh.faces.size() >= 180, "dense planar samples have useful surface coverage");
        std::map<std::pair<int, int>, int> edges;

        for (auto f : mesh.faces) {
            auto a = mesh.vertices[f[0]], b = mesh.vertices[f[1]], c = mesh.vertices[f[2]];
            check(norm(cross(b - a, c - a)) > 1e-8, "nondegenerate face");

            for (int i = 0; i < 3; ++i) {
                int u = f[i], v = f[(i + 1) % 3];

                if (u > v)
                    std::swap(u, v);
                check(++edges[{u, v}] <= 2, "manifold edge bound");
                check(norm(mesh.vertices[u] - mesh.vertices[v]) <= settings.maxEdge + 1e-8, "edge distance");
            }
        }

        plane.insert(plane.end(), 20000, plane.front());
        check(reconstruct(plane, settings,
                    [] {
                        return false;
                    }).vertices.size() == 144,
            "duplicate merge");
        check(reconstruct(plane, settings,
                    [] {
                        return true;
                    })
                    .cancelled,
            "cancellation");
        std::vector<Vec3> separated;

        for (int y = 0; y < 10; ++y)

            for (int x = 0; x < 10; ++x)

                for (int z = 0; z < 2; ++z)
                    separated.push_back({double(x), double(y), z * 0.8});
        auto sheets = reconstruct(separated, {0.1, 1.8}, [] {
            return false;
        });

        for (auto f : sheets.faces)
            check(std::abs(sheets.vertices[f[0]].z - sheets.vertices[f[1]].z) < 1e-8 &&
                    std::abs(sheets.vertices[f[1]].z - sheets.vertices[f[2]].z) < 1e-8,
                "separate parallel sheets not joined");
        std::vector<Vec3> cavity;

        for (int i = 0; i < 6000; ++i)
            cavity.push_back(simulate(i, 0).position);
        auto start = std::chrono::steady_clock::now();
        auto curved = reconstruct(cavity, settings, [] {
            return false;
        });

        check(curved.faces.size() > 100, "concave cavity produces patches");

        for (auto f : curved.faces) {
            Vec3 midpoint =
                (curved.vertices[f[0]] + curved.vertices[f[1]] + curved.vertices[f[2]]) * (1.0 / 3);
            check(midpoint.z < 14.4, "opening not capped across missing top");
        }

        std::cout << "Synthetic 6000-point reconstruction: " << curved.faces.size() << " faces in "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count()
                    << " ms\n";
        Revision r;
        auto old = r.token();
        r.change();
        check(!r.accepts(old), "stale revision rejected");
        check(r.accepts(r.token()), "current revision accepted");
        std::cout << "Core checks passed; planar faces=" << mesh.faces.size() << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
