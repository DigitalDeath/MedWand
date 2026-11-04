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

        require(!result.volumeValid, "current fragmented point cloud cannot report a volume");

        std::cout << "Current point-cloud preview cap status: " << result.capStatus << '\n';
        std::cout << "Flat cap checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
