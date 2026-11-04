#include "reconstruction.h"
#include "flat_cap.h"
#include "radial_reconstruction.h"
#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>

namespace mw {
    namespace {
        using Cell = std::array<int, 3>;

        struct Hash {
            size_t operator()(const Cell &c) const {
                return (std::hash<int>{}(c[0]) * 73856093u) ^ (std::hash<int>{}(c[1]) * 19349663u) ^
                    (std::hash<int>{}(c[2]) * 83492791u);
            }
        };

        Cell cell(Vec3 p, double size) {
            return {int(std::floor(p.x / size)), int(std::floor(p.y / size)), int(std::floor(p.z / size))};
        }

        using Grid = std::unordered_map<Cell, std::vector<int>, Hash>;
        template <class F> void adjacent(const Grid &grid, Cell key, F fn) {
            for (int x = -1; x <= 1; ++x)

                for (int y = -1; y <= 1; ++y)

                    for (int z = -1; z <= 1; ++z) {
                        auto it = grid.find({key[0] + x, key[1] + y, key[2] + z});

                        if (it != grid.end())
                            fn(it->second);
                    }
        }

        struct Plane {
            Vec3 normal;
            double quality;
        };

        Plane localPlane(const std::vector<Vec3> &vertices, const std::vector<int> &ids) {
            Vec3 center{};

            for (int id : ids)
                center = center + vertices[id];
            center = center * (1.0 / ids.size());
            double a[3][3]{}, v[3][3]{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

            for (int id : ids) {
                Vec3 d = vertices[id] - center;
                double c[]{d.x, d.y, d.z};

                for (int i = 0; i < 3; ++i)

                    for (int j = 0; j < 3; ++j)
                        a[i][j] += c[i] * c[j];
            }

            // Jacobi diagonalization of a real symmetric 3x3 covariance matrix.

            for (int step = 0; step < 20; ++step) {
                int p = 0, q = 1;

                for (int i = 0; i < 3; ++i)

                    for (int j = i + 1; j < 3; ++j)

                        if (std::abs(a[i][j]) > std::abs(a[p][q])) {
                            p = i;
                            q = j;
                        }

                if (std::abs(a[p][q]) < 1e-12)
                    break;
                double theta = 0.5 * std::atan2(2 * a[p][q], a[q][q] - a[p][p]);
                double c = std::cos(theta), s = std::sin(theta);

                for (int k = 0; k < 3; ++k) {
                    double x = a[k][p], y = a[k][q];
                    a[k][p] = c * x - s * y;
                    a[k][q] = s * x + c * y;
                }

                for (int k = 0; k < 3; ++k) {
                    double x = a[p][k], y = a[q][k];
                    a[p][k] = c * x - s * y;
                    a[q][k] = s * x + c * y;
                }

                for (int k = 0; k < 3; ++k) {
                    double x = v[k][p], y = v[k][q];
                    v[k][p] = c * x - s * y;
                    v[k][q] = s * x + c * y;
                }
            }

            std::array<int, 3> order{0, 1, 2};
            std::sort(order.begin(), order.end(), [&](int i, int j) {
                return a[i][i] < a[j][j];
            });

            int k = order[0];
            double mid = a[order[1]][order[1]], high = a[order[2]][order[2]];
            double quality = mid > 1e-10 && mid > high * 0.015 ? a[k][k] / mid : 1.0;
            return {unit({v[0][k], v[1][k], v[2][k]}), quality};
        }
    } // namespace

    static Mesh reconstructOpen(
        const std::vector<Vec3> &input, MeshSettings settings, const std::function<bool()> &cancel) {
        Mesh result;

        if (!std::isfinite(settings.mergeDistance) || !std::isfinite(settings.maxEdge) ||
            settings.mergeDistance < 0.01 || settings.maxEdge < settings.mergeDistance * 2 ||
            settings.maxEdge > 100)
            return result;
        Grid merged;

        for (size_t n = 0; n < input.size(); ++n) {
            if ((n % 128) == 0 && cancel()) {
                result.cancelled = true;
                return result;
            }

            Vec3 p = input[n];

            if (!finite(p) || std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) > 100000)
                continue;
            Cell key = cell(p, settings.mergeDistance);
            bool duplicate = false;
            adjacent(merged, key, [&](const auto &ids) {
                for (int id : ids)

                    if (norm(result.vertices[id] - p) < settings.mergeDistance)
                        duplicate = true;
            });

            if (duplicate)
                continue;

            if (result.vertices.size() >= 20000) {
                result.limited = true;
                break;
            }

            merged[key].push_back(int(result.vertices.size()));
            result.vertices.push_back(p);
        }

        Grid grid;

        for (int i = 0; i < int(result.vertices.size()); ++i)
            grid[cell(result.vertices[i], settings.maxEdge)].push_back(i);
        std::vector<std::vector<int>> neighbors(result.vertices.size());
        std::vector<Plane> planes(result.vertices.size());

        for (int i = 0; i < int(result.vertices.size()); ++i) {
            if (cancel()) {
                result.cancelled = true;
                return result;
            }

            std::vector<std::pair<double, int>> near;
            adjacent(grid, cell(result.vertices[i], settings.maxEdge), [&](const auto &ids) {
                // Bound work even when users choose a connection radius much larger than spacing.
                const size_t stride = std::max(size_t(1), (ids.size() + 127) / 128);

                if (stride > 1)
                    result.limited = true;

                for (size_t k = 0; k < ids.size(); k += stride) {
                    int j = ids[k];
                    double d = norm(result.vertices[j] - result.vertices[i]);

                    if (j != i && d <= settings.maxEdge)
                        near.push_back({d, j});
                }
            });

            std::sort(near.begin(), near.end());

            if (near.size() > 32)
                near.resize(32);
            auto &nn = neighbors[i];

            for (auto n : near)
                nn.push_back(n.second);

            if (nn.size() >= 5) {
                auto local = nn;
                local.push_back(i);
                planes[i] = localPlane(result.vertices, local);
            } else
                planes[i].quality = 1;
        }

        std::set<std::array<int, 3>> used;
        std::map<std::pair<int, int>, int> edgeUse;

        for (int i = 0; i < int(result.vertices.size()); ++i) {
            if (cancel()) {
                result.cancelled = true;
                return result;
            }

            if (planes[i].quality > 0.15)
                continue;
            Vec3 normal = planes[i].normal;
            Vec3 u = unit(cross(normal, std::abs(normal.z) < 0.8 ? Vec3{0, 0, 1} : Vec3{0, 1, 0})),
                    v = cross(normal, u);

            struct N {
                int id;
                double angle, x, y;
            };

            std::vector<N> ring;
            auto jitter = [&](int id) {
                auto component = [](unsigned value) {
                    value ^= value >> 16;
                    value *= 0x7feb352du;
                    value ^= value >> 15;
                    return (double(value % 100000) / 100000.0 - 0.5);
                };

                return Vec3{component(unsigned(id) * 3 + 1), component(unsigned(id) * 3 + 2),
                            component(unsigned(id) * 3 + 3)} *
                    (settings.mergeDistance * 0.00001);
            };

            for (int j : neighbors[i]) {
                Vec3 d = result.vertices[j] - result.vertices[i];

                if (planes[j].quality > 0.15 || std::abs(dot(normal, planes[j].normal)) < 0.85)
                    continue;

                if (std::abs(dot(d, normal)) > std::max(settings.mergeDistance * 0.5, norm(d) * 0.12))
                    continue;
                // A deterministic sub-resolution perturbation resolves cocircular samples.
                // It affects connectivity only; original coordinates remain untouched.
                Vec3 projected = d + jitter(j) - jitter(i);
                double x = dot(projected, u), y = dot(projected, v);
                ring.push_back({j, std::atan2(y, x), x, y});
            }

            std::sort(ring.begin(), ring.end(), [](const N &a, const N &b) {
                return a.angle < b.angle;
            });

            for (size_t k = 0; k < ring.size() && ring.size() >= 3; ++k)

                for (size_t l = k + 1; l < ring.size(); ++l) {
                    const auto &a = ring[k];
                    const auto &b = ring[l];
                    double gap = b.angle - a.angle;

                    if (gap > pi)
                        gap = 2 * pi - gap;

                    if (gap > 2.62 || gap < 0.08)
                        continue;
                    double det = 2 * (a.x * b.y - a.y * b.x);

                    if (std::abs(det) < 1e-8)
                        continue;
                    const double aa = a.x * a.x + a.y * a.y, bb = b.x * b.x + b.y * b.y;
                    double cx = (aa * b.y - bb * a.y) / det, cy = (a.x * bb - b.x * aa) / det,
                            r2 = cx * cx + cy * cy;
                    bool inside = false;

                    for (const auto &n : ring)

                        if (n.id != a.id && n.id != b.id) {
                            double d2 = (n.x - cx) * (n.x - cx) + (n.y - cy) * (n.y - cy);

                            if (d2 < r2 - 1e-10)
                                inside = true;
                        }

                    if (inside)
                        continue;
                    std::array<int, 3> face{i, a.id, b.id}, key = face;
                    std::sort(key.begin(), key.end());

                    if (used.count(key))
                        continue;
                    Vec3 ab = result.vertices[a.id] - result.vertices[i],
                            ac = result.vertices[b.id] - result.vertices[i];

                    if (norm(result.vertices[a.id] - result.vertices[b.id]) > settings.maxEdge ||
                        norm(cross(ab, ac)) < 1e-8)
                        continue;
                    bool allowed = true;
                    std::array<std::pair<int, int>, 3> edges;

                    for (int e = 0; e < 3; ++e) {
                        int x = face[e], y = face[(e + 1) % 3];

                        if (x > y)
                            std::swap(x, y);
                        edges[e] = {x, y};

                        if (edgeUse[edges[e]] >= 2)
                            allowed = false;
                    }

                    if (!allowed)
                        continue;

                    for (auto e : edges)
                        ++edgeUse[e];
                    used.insert(key);
                    result.faces.push_back(face);

                    if (result.faces.size() >= 40000) {
                        result.limited = true;
                        return result;
                    }
                }
        }

        return result;
    }

    Mesh reconstruct(
        const std::vector<Vec3> &points, MeshSettings settings, const std::function<bool()> &cancel) {
        Mesh mesh = settings.radialCavity ? reconstructRadial(points, settings, cancel)
                                            : reconstructOpen(points, settings, cancel);

        if (settings.flatCap && !mesh.cancelled && !mesh.faces.empty())
            return closeFlatOpening(mesh, settings.capOpening, settings.capTolerance, cancel);
        return mesh;
    }
} // namespace mw
