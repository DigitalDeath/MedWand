#include "flat_cap.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace mw;

void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

Mesh box() {
    Mesh mesh;
    mesh.vertices = {{0, 0, 0}, {2, 0, 0}, {2, 2, 0}, {0, 2, 0}, {0, 0, 3}, {2, 0, 3}, {2, 2, 3}, {0, 2, 3}};
    mesh.faces = {{0, 2, 1}, {0, 3, 2}, {0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6},
        {3, 0, 4}, {3, 4, 7}};
    return mesh;
}

int main() {
    try {
        auto mesh = box();
        auto result = closeFlatOpening(mesh, 0, 0.1, [] {
            return false;
        });

        require(result.capFaces.size() == 2, "flat rectangular cap has two triangles");
        require(result.volumeValid, "capped box bounds a valid volume");
        require(std::abs(result.volumeMm3 - 12) < 1e-8, "2 by 2 by 3 box has volume 12 mm3");
        require(mesh.faces.size() == 10, "input mesh remains unchanged");
        std::swap(mesh.faces[3][0], mesh.faces[3][1]);

        for (auto &p : mesh.vertices)
            p = p + Vec3{10000, 10000, 10000};
        result = closeFlatOpening(mesh, 0, 0.1, [] {
            return false;
        });

        require(result.volumeValid && std::abs(result.volumeMm3 - 12) < 1e-8,
            "orientation repair and translation preserve volume");
        mesh = box();
        mesh.faces.erase(mesh.faces.begin());
        result = closeFlatOpening(mesh, 0, 0.1, [] {
            return false;
        });

        require(!result.volumeValid, "another hole prevents volume reporting");
        mesh = box();
        mesh.vertices[6].z += 0.6;
        result = closeFlatOpening(mesh, 0, 0.01, [] {
            return false;
        });

        require(result.capFaces.empty() && !result.volumeValid, "nonplanar rim beyond tolerance rejected");
        result = closeFlatOpening(box(), 9, 0.1, [] {
            return false;
        });

        require(!result.volumeValid, "invalid opening rejected");
        result = closeFlatOpening(box(), 0, 0.1, [] {
            return true;
        });

        require(result.cancelled, "cap cancellation");
        result = closeFlatOpening(makeCapTestBowl(), 0, 0.1, [] {
            return false;
        });

        require(result.volumeValid &&
                std::abs(result.volumeMm3 - (2.0 / 3) * pi * 38 * 38 * 15) / ((2.0 / 3) * pi * 38 * 38 * 15) <
                    0.003,
            "faceted test bowl volume agrees with analytic half-ellipsoid within 0.3 percent");
        std::cout << "Test bowl: " << result.volumeMm3 << " mm3 / " << result.volumeMm3 / 1000 << " mL\n";
        mesh = box();
        mesh.vertices[0].z = 4;
        result = closeFlatOpening(mesh, 0, 0.1, [] {
            return false;
        });

        require(!result.volumeValid && result.capStatus.find("Intersecting") != std::string::npos,
            "cap crossing a folded wall is rejected");
        Mesh concave;
        concave.vertices = {{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {1, 1, 0}, {1, 2, 0}, {0, 2, 0}};

        for (int i = 0; i < 6; ++i)
            concave.vertices.push_back(concave.vertices[i] + Vec3{0, 0, 3});
        concave.faces = {{0, 3, 1}, {1, 3, 2}, {0, 5, 3}, {3, 5, 4}};

        for (int i = 0; i < 6; ++i) {
            int j = (i + 1) % 6;
            concave.faces.push_back({i, j, j + 6});
            concave.faces.push_back({i, j + 6, i + 6});
        }

        result = closeFlatOpening(concave, 0, 0.1, [] {
            return false;
        });

        require(result.volumeValid && std::abs(result.volumeMm3 - 9) < 1e-8,
            "concave L-shaped prism volume is 9 mm3");
        MeshSettings settings;
        settings.flatCap = true;
        std::vector<Vec3> cloud;

        for (int i = 0; i < 6000; ++i)
            cloud.push_back(simulate(i, 0).position);
        result = reconstruct(cloud, settings, [] {
            return false;
        });

        require(!result.volumeValid, "local preview still withholds volume for fragmented geometry");
        settings.radialCavity = true;
        settings.maxEdge = 6;
        result = reconstruct(cloud, settings, [] {
            return false;
        });

        std::cout << "Radial diagnostic: " << result.capStatus << " faces=" << result.faces.size()
                    << " openings=" << result.openings << '\n';
        require(result.volumeValid, "ordinary captured demo cloud closes in radial cavity mode");
        double reference = 0;
        constexpr int steps = 400;

        for (int i = 0; i < steps; ++i) {
            double theta = 0.3 + (pi - 0.3) * (i + 0.5) / steps;

            for (int j = 0; j < steps; ++j) {
                double phi = 2 * pi * (j + 0.5) / steps;
                double radiusFactor =
                    1 - 0.30 * std::exp(-std::pow((phi - pi) / 0.48, 2) - std::pow((theta - 1.5) / 0.5, 2));
                reference += std::pow(std::sin(theta), 3) * radiusFactor * radiusFactor;
            }
        }

        reference *= 38 * 38 * 15 / 2.0 * (pi - 0.3) / steps * 2 * pi / steps;
        require(std::abs(result.volumeMm3 - reference) / reference < 0.015,
            "dented cavity mesh agrees with independently integrated cross-section volume within 1.5 "
            "percent");
        std::cout << "Independent integrated cavity volume: " << reference << " mm3\n";
        std::vector<Vec3> translated = cloud;
        std::reverse(translated.begin(), translated.end());

        for (auto &p : translated)
            p = p + Vec3{50, -20, 30};
        auto shifted = reconstruct(translated, settings, [] {
            return false;
        });

        require(shifted.volumeValid && std::abs(shifted.volumeMm3 - reference) / reference < 0.015,
            "translated cavity remains data-derived and volume stable");
        std::vector<Vec3> offsetCloud;

        for (int i = 997; i < 6997; ++i)
            offsetCloud.push_back(simulate(i, 0).position);
        auto offsetResult = reconstruct(offsetCloud, settings, [] {
            return false;
        });

        require(offsetResult.volumeValid && std::abs(offsetResult.volumeMm3 - reference) / reference < 0.015,
            "nonzero demo sequence offset closes with stable volume");
        std::vector<Vec3> ellipsoid;

        for (int i = 0; i < 6000; ++i) {
            double theta = 0.4 + (pi - 0.401) * (i + 0.5) / 6000.0,
                    phi = 2 * pi * std::fmod(i * 0.6180339887498948, 1.0);
            ellipsoid.push_back({20 * std::sin(theta) * std::cos(phi), 12 * std::sin(theta) * std::sin(phi),
                10 * std::cos(theta)});
        }

        auto other = reconstruct(ellipsoid, settings, [] {
            return false;
        });

        double z = std::cos(0.4), otherReference = pi * 20 * 12 * 10 * (z - z * z * z / 3 + 2.0 / 3);
        require(other.volumeValid && std::abs(other.volumeMm3 - otherReference) / otherReference < 0.015,
            "different ellipsoid closes without simulator dimensions");
        std::vector<Vec3> partial;

        for (auto p : cloud)

            if (std::atan2(p.y, p.x) < 1.0 || std::atan2(p.y, p.x) > 2.0)
                partial.push_back(p);
        require(!reconstruct(partial, settings,
                    [] {
                        return false;
                    })
                    .volumeValid,
            "large unscanned wedge cannot report volume");
        std::vector<Vec3> sparse(cloud.begin(), cloud.begin() + 40);
        require(!reconstruct(sparse, settings,
                    [] {
                        return false;
                    })
                    .volumeValid,
            "sparse radial input cannot report volume");
        auto layers = ellipsoid;

        for (auto p : ellipsoid)
            layers.push_back(p * 0.6);
        require(!reconstruct(layers, settings,
                    [] {
                        return false;
                    })
                    .volumeValid,
            "nested radial layers cannot report volume");
        int cancellationChecks = 0;
        auto interrupted = reconstruct(cloud, settings, [&] {
            return ++cancellationChecks > 60;
        });

        require(interrupted.cancelled && !interrupted.volumeValid,
            "radial triangulation obeys mid-work cancellation");

        for (int density : {500, 1000, 2000, 3000, 4000, 6000}) {
            std::vector<Vec3> sample(cloud.begin(), cloud.begin() + density);
            auto probe = reconstruct(sample, settings, [] {
                return false;
            });

            if (density <= 1000)
                require(!probe.volumeValid, "insufficient demo density cannot report radial volume");

            std::cout << "Density " << density << ": " << (probe.volumeValid ? "closed" : "incomplete")
                        << " - " << probe.capStatus << '\n';
        }

        std::cout << "Captured cloud estimated volume: " << result.volumeMm3 << " mm3\n";

        std::cout << "Current point-cloud preview cap status: " << result.capStatus << '\n';
        std::cout << "Flat cap checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
