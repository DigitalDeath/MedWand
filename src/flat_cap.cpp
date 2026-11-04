#include "flat_cap.h"
#include <algorithm>
#include <map>
#include <numeric>
#include <queue>
#include <set>

namespace mw {
    Mesh makeCapTestBowl() {
        Mesh mesh;
        constexpr int rings = 24, segments = 96;
        mesh.vertices.push_back({0, 0, -15});

        for (int ring = 1; ring <= rings; ++ring) {
            double theta = ring * pi / (2 * rings);

            for (int j = 0; j < segments; ++j) {
                double angle = 2 * pi * j / segments;
                mesh.vertices.push_back({38 * std::sin(theta) * std::cos(angle),
                    38 * std::sin(theta) * std::sin(angle), -15 * std::cos(theta)});
            }
        }

        for (int j = 0; j < segments; ++j)
            mesh.faces.push_back({0, 1 + (j + 1) % segments, 1 + j});

        for (int ring = 1; ring < rings; ++ring)

            for (int j = 0; j < segments; ++j) {
                int a = 1 + (ring - 1) * segments + j, b = 1 + (ring - 1) * segments + (j + 1) % segments,
                    c = a + segments, d = b + segments;
                mesh.faces.push_back({a, b, c});
                mesh.faces.push_back({b, d, c});
            }

        return mesh;
    }

    namespace {
        using Face = std::array<int, 3>;
        using Edge = std::pair<int, int>;
        using Edges = std::map<Edge, std::vector<std::pair<int, int>>>;

        Edge edge(int a, int b) {
            return std::minmax(a, b);
        }

        Edges edgesOf(const std::vector<Face> &faces) {
            Edges edges;

            for (int i = 0; i < int(faces.size()); ++i) {
                for (int j = 0; j < 3; ++j) {
                    int a = faces[i][j], b = faces[i][(j + 1) % 3];
                    edges[edge(a, b)].push_back({i, a < b ? 1 : -1});
                }
            }

            return edges;
        }

        struct Point2 {
            double x, y;
        };

        double turn(Point2 a, Point2 b, Point2 c) {
            return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        }

        bool inside(Point2 p, Point2 a, Point2 b, Point2 c, double eps) {
            double x = turn(a, b, p), y = turn(b, c, p), z = turn(c, a, p);
            return (x >= -eps && y >= -eps && z >= -eps) || (x <= eps && y <= eps && z <= eps);
        }

        struct Bounds {
            Vec3 low{1e100, 1e100, 1e100}, high{-1e100, -1e100, -1e100};
            void add(Vec3 p) {
                low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
                high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
            }

            bool overlaps(const Bounds &b, double eps) const {
                return low.x <= b.high.x + eps && high.x + eps >= b.low.x && low.y <= b.high.y + eps &&
                    high.y + eps >= b.low.y && low.z <= b.high.z + eps && high.z + eps >= b.low.z;
            }
        };

        // Test contact beyond the shared topological vertices/edge. Coplanar overlap
        // is checked in 2D; noncoplanar contact is found by segment/plane crossings.
        bool intersects(const Mesh &mesh, Face f, Face g, double eps) {
            std::vector<Vec3> common;

            for (int a : f)

                for (int b : g)

                    if (a == b)
                        common.push_back(mesh.vertices[a]);
            auto allowed = [&](Vec3 p) {
                for (auto q : common)

                    if (norm(p - q) <= eps)
                        return true;

                if (common.size() == 2) {
                    Vec3 d = common[1] - common[0];
                    double t = dot(p - common[0], d) / dot(d, d);

                    if (t >= 0 && t <= 1 && norm(p - (common[0] + d * t)) <= eps)
                        return true;
                }

                return false;
            };

            Vec3 a = mesh.vertices[f[0]], b = mesh.vertices[f[1]], c = mesh.vertices[f[2]];
            Vec3 normal = unit(cross(b - a, c - a));
            bool coplanar = true;

            for (int id : g)

                if (std::abs(dot(mesh.vertices[id] - a, normal)) > eps)
                    coplanar = false;

            if (coplanar) {
                Vec3 u = unit(b - a), v = cross(normal, u);
                auto project = [&](Vec3 p) {
                    return Point2{dot(p - a, u), dot(p - a, v)};
                };

                std::array<Point2, 3> p, q;

                for (int i = 0; i < 3; ++i) {
                    p[i] = project(mesh.vertices[f[i]]);
                    q[i] = project(mesh.vertices[g[i]]);
                }

                const double areaEps = eps * std::max({norm(b - a), norm(c - a), 1.0});

                for (int i = 0; i < 3; ++i) {
                    if (inside(p[i], q[0], q[1], q[2], areaEps) && !allowed(mesh.vertices[f[i]]))
                        return true;

                    if (inside(q[i], p[0], p[1], p[2], areaEps) && !allowed(mesh.vertices[g[i]]))
                        return true;

                    for (int j = 0; j < 3; ++j) {
                        auto x = p[i], y = p[(i + 1) % 3], z = q[j], w = q[(j + 1) % 3];
                        double det = (y.x - x.x) * (w.y - z.y) - (y.y - x.y) * (w.x - z.x);

                        if (std::abs(det) < areaEps)
                            continue;
                        double t = ((z.x - x.x) * (w.y - z.y) - (z.y - x.y) * (w.x - z.x)) / det;
                        double s = ((z.x - x.x) * (y.y - x.y) - (z.y - x.y) * (y.x - x.x)) / det;

                        if (t >= 0 && t <= 1 && s >= 0 && s <= 1) {
                            Vec3 hit = mesh.vertices[f[i]] +
                                (mesh.vertices[f[(i + 1) % 3]] - mesh.vertices[f[i]]) * t;

                            if (!allowed(hit))
                                return true;
                        }
                    }
                }

                return false;
            }

            auto crosses = [&](Face first, Face second) {
                Vec3 x = mesh.vertices[second[0]], y = mesh.vertices[second[1]], z = mesh.vertices[second[2]];
                Vec3 n = unit(cross(y - x, z - x)), u = unit(y - x), v = cross(n, u);
                auto project = [&](Vec3 p) {
                    return Point2{dot(p - x, u), dot(p - x, v)};
                };

                for (int i = 0; i < 3; ++i) {
                    Vec3 p = mesh.vertices[first[i]], q = mesh.vertices[first[(i + 1) % 3]];
                    double d = dot(p - x, n), e = dot(q - x, n);

                    if ((d > eps && e > eps) || (d < -eps && e < -eps))
                        continue;

                    if (std::abs(d - e) < eps)
                        continue;
                    double t = d / (d - e);

                    if (t < 0 || t > 1)
                        continue;
                    Vec3 hit = p + (q - p) * t;

                    if (inside(project(hit), project(x), project(y), project(z),
                            eps * std::max(norm(y - x), norm(z - x))) &&
                        !allowed(hit))
                        return true;
                }

                return false;
            };

            return crosses(f, g) || crosses(g, f);
        }

        struct Node {
            Bounds bounds;
            int begin, end, left = -1, right = -1;
        };

        bool selfIntersects(const Mesh &mesh, const std::vector<Face> &faces, double eps,
            const std::function<bool()> &cancel) {
            std::vector<Bounds> bounds(faces.size());
            std::vector<int> ids(faces.size());
            std::iota(ids.begin(), ids.end(), 0);

            for (size_t i = 0; i < faces.size(); ++i)

                for (int v : faces[i])
                    bounds[i].add(mesh.vertices[v]);
            std::vector<Node> nodes;
            std::function<int(int, int)> build = [&](int begin, int end) {
                Node node;
                node.begin = begin;
                node.end = end;

                for (int i = begin; i < end; ++i) {
                    node.bounds.add(bounds[ids[i]].low);
                    node.bounds.add(bounds[ids[i]].high);
                }

                int index = int(nodes.size());
                nodes.push_back(node);

                if (end - begin > 8) {
                    Vec3 size = node.bounds.high - node.bounds.low;
                    int axis = size.x > size.y ? (size.x > size.z ? 0 : 2) : (size.y > size.z ? 1 : 2);
                    auto key = [&](int id) {
                        Vec3 p = bounds[id].low + bounds[id].high;
                        return axis == 0 ? p.x : axis == 1 ? p.y : p.z;
                    };

                    int mid = (begin + end) / 2;
                    std::nth_element(
                        ids.begin() + begin, ids.begin() + mid, ids.begin() + end, [&](int a, int b) {
                            return key(a) < key(b);
                        });

                    int left = build(begin, mid), right = build(mid, end);
                    nodes[index].left = left;
                    nodes[index].right = right;
                }

                return index;
            };

            build(0, int(ids.size()));

            for (int i = 0; i < int(faces.size()); ++i) {
                if (cancel())
                    return true;
                std::vector<int> stack{0};

                while (!stack.empty()) {
                    const auto &node = nodes[stack.back()];
                    stack.pop_back();

                    if (!node.bounds.overlaps(bounds[i], eps))
                        continue;

                    if (node.left >= 0) {
                        stack.push_back(node.left);
                        stack.push_back(node.right);
                        continue;
                    }

                    for (int k = node.begin; k < node.end; ++k) {
                        int j = ids[k];

                        if (j > i && bounds[i].overlaps(bounds[j], eps) &&
                            intersects(mesh, faces[i], faces[j], eps))
                            return true;
                    }
                }
            }

            return false;
        }
    } // namespace

    Mesh closeFlatOpening(
        const Mesh &input, int opening, double tolerance, const std::function<bool()> &cancel) {
        Mesh result = input;
        result.capFaces.clear();
        result.volumeValid = false;
        result.volumeMm3 = 0;
        result.openings = 0;
        auto fail = [&](const std::string &message) {
            if (result.capFaces.empty())
                result.vertices = input.vertices;
            result.capStatus = message;
            return result;
        };

        if (cancel()) {
            result.cancelled = true;
            return result;
        }

        if (input.cancelled || input.faces.empty())
            return fail("No surface available to cap.");

        if (input.faces.size() > 40000 || input.vertices.size() > 20000)
            return fail("Mesh exceeds closure work limits.");

        if (!std::isfinite(tolerance) || tolerance < 0 || tolerance > 10)
            return fail("Invalid rim tolerance.");
        Bounds total;

        for (auto p : input.vertices) {
            if (!finite(p))
                return fail("Nonfinite mesh coordinates.");
            total.add(p);
        }

        double scale = std::max(1.0, norm(total.high - total.low)), eps = scale * 1e-9;
        std::set<Face> distinct;

        for (auto f : input.faces) {
            for (int id : f)

                if (id < 0 || size_t(id) >= input.vertices.size())
                    return fail("Invalid mesh index.");

            if (norm(cross(input.vertices[f[1]] - input.vertices[f[0]],
                    input.vertices[f[2]] - input.vertices[f[0]])) <= eps * scale)
                return fail("Degenerate surface triangles.");
            auto key = f;
            std::sort(key.begin(), key.end());

            if (!distinct.insert(key).second)
                return fail("Duplicate surface triangles.");
        }

        auto edges = edgesOf(input.faces);
        std::map<int, std::vector<int>> boundary;

        for (const auto &entry : edges) {
            if (entry.second.size() > 2)
                return fail("Nonmanifold surface edges; repair the surface first.");

            if (entry.second.size() == 1) {
                boundary[entry.first.first].push_back(entry.first.second);
                boundary[entry.first.second].push_back(entry.first.first);
            }
        }

        for (const auto &entry : boundary)

            if (entry.second.size() != 2)
                return fail("Boundary branches or touches itself; repair surface connectivity first.");
        std::set<int> visited;
        std::vector<std::vector<int>> loops;

        for (const auto &entry : boundary) {
            if (visited.count(entry.first))
                continue;
            std::vector<int> loop;
            int start = entry.first, previous = -1, current = start;
            do {
                if (!visited.insert(current).second)
                    return fail("Invalid boundary cycle.");
                loop.push_back(current);
                auto adjacent = boundary.at(current);
                int next = adjacent[0] == previous ? adjacent[1] : adjacent[0];
                previous = current;
                current = next;
            } while (current != start);

            if (loop.size() < 3)
                return fail("Opening has fewer than three vertices.");
            loops.push_back(std::move(loop));
        }

        auto perimeter = [&](const auto &loop) {
            double length = 0;

            for (size_t i = 0; i < loop.size(); ++i)
                length += norm(input.vertices[loop[i]] - input.vertices[loop[(i + 1) % loop.size()]]);
            return length;
        };

        std::stable_sort(loops.begin(), loops.end(), [&](const auto &a, const auto &b) {
            return perimeter(a) > perimeter(b);
        });

        result.openings = loops.size();

        if (opening < 0 || size_t(opening) >= loops.size())
            return fail(loops.empty() ? "No open boundary found." : "Selected opening is unavailable.");
        const auto &rim = loops[opening];

        if (rim.size() > 2048)
            return fail("Opening exceeds the 2,048-vertex cap work limit.");
        Vec3 center{}, normal{};

        for (int id : rim)
            center = center + input.vertices[id];
        center = center * (1.0 / rim.size());

        for (size_t i = 0; i < rim.size(); ++i)
            normal = normal +
                cross(input.vertices[rim[i]] - center, input.vertices[rim[(i + 1) % rim.size()]] - center);

        if (norm(normal) <= eps * scale)
            return fail("Opening has no stable flat plane.");
        normal = unit(normal);
        Vec3 u = unit(input.vertices[rim.front()] - center), v = unit(cross(normal, u));
        u = cross(v, normal);
        std::vector<Point2> polygon;

        for (int id : rim) {
            Vec3 d = input.vertices[id] - center;
            double distance = dot(d, normal);

            if (std::abs(distance) > tolerance + eps)
                return fail(
                    "Rim is not flat within tolerance; select another opening or increase tolerance.");
            result.vertices[id] = input.vertices[id] - normal * distance;
            polygon.push_back({dot(d, u), dot(d, v)});
        }

        // Reject a crossing rim before triangulating it.

        for (size_t i = 0; i < rim.size(); ++i)

            for (size_t j = i + 1; j < rim.size(); ++j) {
                if (j == i + 1 || (i == 0 && j == rim.size() - 1))
                    continue;
                auto a = polygon[i], b = polygon[(i + 1) % rim.size()], c = polygon[j],
                        d = polygon[(j + 1) % rim.size()];

                if (turn(a, b, c) * turn(a, b, d) <= 0 && turn(c, d, a) * turn(c, d, b) <= 0 &&
                    std::max(std::min(a.x, b.x), std::min(c.x, d.x)) <=
                        std::min(std::max(a.x, b.x), std::max(c.x, d.x)) + eps &&
                    std::max(std::min(a.y, b.y), std::min(c.y, d.y)) <=
                        std::min(std::max(a.y, b.y), std::max(c.y, d.y)) + eps)
                    return fail("Opening boundary intersects itself.");
            }

        std::vector<int> remaining(rim.size());
        std::iota(remaining.begin(), remaining.end(), 0);

        while (remaining.size() > 3) {
            if (cancel()) {
                result.cancelled = true;
                return result;
            }

            bool found = false;

            for (size_t i = 0; i < remaining.size(); ++i) {
                int a = remaining[(i + remaining.size() - 1) % remaining.size()], b = remaining[i],
                    c = remaining[(i + 1) % remaining.size()];

                if (turn(polygon[a], polygon[b], polygon[c]) <= eps * scale)
                    continue;
                bool blocked = false;

                for (int p : remaining)

                    if (p != a && p != b && p != c &&
                        inside(polygon[p], polygon[a], polygon[b], polygon[c], eps * scale))
                        blocked = true;

                if (blocked)
                    continue;
                result.capFaces.push_back({rim[a], rim[b], rim[c]});
                remaining.erase(remaining.begin() + i);
                found = true;
                break;
            }

            if (!found) {
                result.capFaces.clear();
                return fail("Opening could not be triangulated without degeneracy.");
            }
        }

        if (turn(polygon[remaining[0]], polygon[remaining[1]], polygon[remaining[2]]) <= eps * scale) {
            result.capFaces.clear();
            return fail("Cap has a degenerate final triangle.");
        }

        result.capFaces.push_back({rim[remaining[0]], rim[remaining[1]], rim[remaining[2]]});

        if (loops.size() != 1)
            return fail(
                "Cap preview only: other openings remain (" + std::to_string(loops.size() - 1) + ").");
        std::vector<Face> faces = result.faces;
        faces.insert(faces.end(), result.capFaces.begin(), result.capFaces.end());

        for (auto f : faces)

            if (norm(cross(result.vertices[f[1]] - result.vertices[f[0]],
                    result.vertices[f[2]] - result.vertices[f[0]])) <= eps * scale)
                return fail("Plane projection creates degenerate wall triangles; lower rim tolerance.");
        edges = edgesOf(faces);

        for (const auto &entry : edges)

            if (entry.second.size() != 2)
                return fail("Capped mesh is not closed.");
        std::vector<std::vector<std::pair<int, int>>> adjacent(faces.size());

        for (const auto &entry : edges) {
            auto a = entry.second[0], b = entry.second[1];
            int flip = a.second == b.second;
            adjacent[a.first].push_back({b.first, flip});
            adjacent[b.first].push_back({a.first, flip});
        }

        std::vector<int> orientation(faces.size(), -1);
        orientation[0] = 0;
        std::queue<int> queue;
        queue.push(0);
        size_t reached = 0;

        while (!queue.empty()) {
            int i = queue.front();
            queue.pop();
            ++reached;

            for (auto next : adjacent[i]) {
                int expected = orientation[i] ^ next.second;

                if (orientation[next.first] < 0) {
                    orientation[next.first] = expected;
                    queue.push(next.first);
                } else if (orientation[next.first] != expected)
                    return fail("Surface is not consistently orientable.");
            }
        }

        if (reached != faces.size())
            return fail("Multiple disconnected shells; volume withheld.");
        std::vector<std::vector<int>> incident(result.vertices.size());

        for (int i = 0; i < int(faces.size()); ++i)

            for (int id : faces[i])
                incident[id].push_back(i);

        for (size_t vertex = 0; vertex < incident.size(); ++vertex) {
            if (incident[vertex].empty())
                continue;
            std::set<int> seen;
            std::vector<int> stack{incident[vertex].front()};

            while (!stack.empty()) {
                int i = stack.back();
                stack.pop_back();

                if (!seen.insert(i).second)
                    continue;

                for (auto neighbor : adjacent[i]) {
                    auto f = faces[neighbor.first];

                    if (std::find(f.begin(), f.end(), int(vertex)) != f.end() && !seen.count(neighbor.first))
                        stack.push_back(neighbor.first);
                }
            }

            if (seen.size() != incident[vertex].size())
                return fail("Nonmanifold vertex; volume withheld.");
        }

        if (selfIntersects(result, faces, eps, cancel)) {
            if (cancel())
                result.cancelled = true;
            return fail("Intersecting or touching surface triangles; volume withheld.");
        }

        double volume = 0;
        Vec3 origin = (total.low + total.high) * 0.5;

        for (size_t i = 0; i < faces.size(); ++i) {
            if (orientation[i])
                std::swap(faces[i][1], faces[i][2]);
            auto f = faces[i];
            volume += dot(result.vertices[f[0]] - origin,
                            cross(result.vertices[f[1]] - origin, result.vertices[f[2]] - origin)) /
                6;
        }

        if (std::abs(volume) <= eps * scale * scale)
            return fail("Enclosed volume is numerically degenerate.");

        if (volume < 0)

            for (auto &f : faces)
                std::swap(f[1], f[2]);
        result.faces.assign(faces.begin(), faces.begin() + input.faces.size());
        result.capFaces.assign(faces.begin() + input.faces.size(), faces.end());
        result.volumeMm3 = std::abs(volume);
        result.volumeValid = true;
        result.capStatus = "Closed single shell; estimated volume includes the flat cap.";
        return result;
    }
} // namespace mw
