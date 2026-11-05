#include "curved_cap.h"
#include <algorithm>
#include <array>
#include <queue>

namespace mw {
    namespace {
        using Row = std::array<double, 10>;

        bool solve(std::array<Row, 9> &matrix, std::array<double, 9> &solution) {
            for (int column = 0; column < 9; ++column) {
                int pivot = column;

                for (int row = column + 1; row < 9; ++row) {
                    if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column]))
                        pivot = row;
                }

                if (std::abs(matrix[pivot][column]) < 1e-12)
                    return false;
                std::swap(matrix[pivot], matrix[column]);
                double divisor = matrix[column][column];

                for (int j = column; j <= 9; ++j)
                    matrix[column][j] /= divisor;

                for (int row = 0; row < 9; ++row) {
                    if (row == column)
                        continue;
                    double factor = matrix[row][column];

                    for (int j = column; j <= 9; ++j)
                        matrix[row][j] -= factor * matrix[column][j];
                }
            }

            for (int i = 0; i < 9; ++i)
                solution[i] = matrix[i][9];
            return true;
        }
    } // namespace

    bool buildCurvedCap(Mesh &result, const Mesh &input, const std::vector<int> &rim,
        const std::function<bool()> &cancel, std::string &error) {
        auto fail = [&](const char *message) {
            error = message;
            result.vertices = input.vertices;
            result.capFaces.clear();
            return false;
        };

        Vec3 low = input.vertices.front(), high = low, rimCenter{}, wallCenter{}, pole{};

        for (auto p : input.vertices) {
            low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
            high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
            wallCenter = wallCenter + p * (1.0 / input.vertices.size());
        }

        for (int id : rim)
            rimCenter = rimCenter + input.vertices[id] * (1.0 / rim.size());

        for (size_t i = 0; i < rim.size(); ++i)
            pole = pole +
                cross(input.vertices[rim[i]] - rimCenter,
                    input.vertices[rim[(i + 1) % rim.size()]] - rimCenter);
        pole = unit(pole);

        if (dot(pole, rimCenter - wallCenter) < 0)
            pole = pole * -1;
        double depthExtent = 0;

        for (auto p : input.vertices)
            depthExtent = std::max(depthExtent, dot(rimCenter - p, pole));
        double scale = norm(high - low);

        if (norm(pole) < 0.5 || depthExtent < scale * 1e-6)
            return fail("Opening has no stable radial continuation axis.");
        Vec3 origin = rimCenter - pole * (0.5 * depthExtent);
        Vec3 u = input.vertices[rim.front()] - origin;
        u = unit(u - pole * dot(u, pole));
        Vec3 v = cross(pole, u);

        if (norm(u) < 0.5)
            return fail("Opening has no stable radial continuation frame.");

        // Fit one implicit quadratic to a wall band, so the lid shares a single
        // curvature field instead of unrelated curves aimed at an invented apex.
        std::vector<std::vector<int>> neighbors(input.vertices.size());

        for (auto face : input.faces) {
            for (int i = 0; i < 3; ++i) {
                neighbors[face[i]].push_back(face[(i + 1) % 3]);
                neighbors[face[i]].push_back(face[(i + 2) % 3]);
            }
        }

        std::vector<int> depth(input.vertices.size(), -1), support;
        std::queue<int> pending;

        for (int id : rim) {
            depth[id] = 0;
            pending.push(id);
        }

        while (!pending.empty()) {
            int id = pending.front();
            pending.pop();
            support.push_back(id);

            if (depth[id] == 5)
                continue;

            for (int other : neighbors[id]) {
                if (depth[other] < 0) {
                    depth[other] = depth[id] + 1;
                    pending.push(other);
                }
            }
        }

        if (support.size() < 18)
            return fail("Insufficient wall support for a curvature fit.");
        std::array<Row, 9> matrix{};
        size_t samples = std::min(size_t(768), support.size());

        for (size_t i = 0; i < samples; ++i) {
            if (cancel()) {
                result.cancelled = true;
                return fail("Curved fit cancelled.");
            }

            int id = support[i * support.size() / samples];
            Vec3 p = (input.vertices[id] - origin) * (1 / scale);
            std::array<double, 9> row{
                p.x * p.x, p.y * p.y, p.z * p.z, p.x * p.y, p.x * p.z, p.y * p.z, p.x, p.y, p.z};
            double weight = 1.0 / (1 + depth[id]);

            for (int a = 0; a < 9; ++a) {
                for (int b = 0; b < 9; ++b)
                    matrix[a][b] += weight * row[a] * row[b];
                matrix[a][9] += weight * row[a];
            }
        }

        std::array<double, 9> coefficients{};

        if (!solve(matrix, coefficients))
            return fail("Wall curvature fit is underdetermined; more wall coverage is needed.");
        const auto &quad = coefficients;
        Vec3 col0{quad[0], quad[3] * 0.5, quad[4] * 0.5}, col1{quad[3] * 0.5, quad[1], quad[5] * 0.5},
            col2{quad[4] * 0.5, quad[5] * 0.5, quad[2]};

        if (quad[0] <= 0 || quad[0] * quad[1] - quad[3] * quad[3] * 0.25 <= 0 ||
            dot(col0, cross(col1, col2)) <= 0)
            return fail("Local wall curvature does not support a smooth convex closure.");
        auto radius = [&](Vec3 direction) {
            double quadratic = quad[0] * direction.x * direction.x + quad[1] * direction.y * direction.y +
                quad[2] * direction.z * direction.z + quad[3] * direction.x * direction.y +
                quad[4] * direction.x * direction.z + quad[5] * direction.y * direction.z;
            double linear = quad[6] * direction.x + quad[7] * direction.y + quad[8] * direction.z;
            double discriminant = std::sqrt(linear * linear + 4 * quadratic);
            // Stable positive root, including strongly off-center fits.
            return scale *
                (linear >= 0 ? 2 / (discriminant + linear) : (discriminant - linear) / (2 * quadratic));
        };

        for (int id : support) {
            Vec3 d = input.vertices[id] - origin;
            double r = norm(d), fitted = radius(unit(d));

            if (!std::isfinite(fitted) || r <= scale * 1e-8 || std::abs(fitted - r) > 0.05 * r)
                return fail("Wall curvature varies beyond this closure fit; volume withheld.");
        }

        std::vector<std::array<double, 2>> chart;
        std::vector<double> residual;

        for (int id : rim) {
            Vec3 d = input.vertices[id] - origin;
            double r = norm(d), denominator = r + dot(d, pole);

            if (dot(d, pole) <= 0 || denominator <= scale * 1e-8)
                return fail("Opening is not a single radial patch around the fitted cavity.");
            chart.push_back({dot(d, u) / denominator, dot(d, v) / denominator});
            residual.push_back(r - radius(unit(d)));
        }

        // Match the local first and second radial derivatives as well as the rim.
        // A single global fit alone can miss a narrow curvature change at the lip.
        std::vector<double> first(rim.size()), second(rim.size());
        std::vector<int> localDepth(input.vertices.size(), -1), touched;

        for (size_t i = 0; i < rim.size(); ++i) {
            if (cancel()) {
                result.cancelled = true;
                return fail("Curved fit cancelled.");
            }

            touched.clear();
            touched.push_back(rim[i]);
            localDepth[rim[i]] = 0;

            for (size_t j = 0; j < touched.size(); ++j) {
                int id = touched[j];

                if (localDepth[id] == 2)
                    continue;

                for (int other : neighbors[id]) {
                    if (localDepth[other] < 0) {
                        localDepth[other] = localDepth[id] + 1;
                        touched.push_back(other);
                    }
                }
            }

            std::vector<std::array<double, 3>> local;
            double localScale = 0;

            for (int id : touched) {
                Vec3 d = input.vertices[id] - origin;
                double r = norm(d), denominator = r + dot(d, pole);

                if (denominator <= scale * 1e-8)
                    continue;
                double x = dot(d, u) / denominator - chart[i][0], y = dot(d, v) / denominator - chart[i][1];
                local.push_back({x, y, r - radius(unit(d)) - residual[i]});
                localScale = std::max(localScale, std::hypot(x, y));
            }

            for (int id : touched)
                localDepth[id] = -1;

            if (local.size() < 6 || localScale < 1e-8)
                return fail("Insufficient local samples to continue rim curvature.");
            std::array<Row, 9> localMatrix{};

            for (auto sample : local) {
                double x = sample[0] / localScale, y = sample[1] / localScale;
                std::array<double, 5> row{x, y, x * x, x * y, y * y};

                for (int a = 0; a < 5; ++a) {
                    for (int b = 0; b < 5; ++b)
                        localMatrix[a][b] += row[a] * row[b];
                    localMatrix[a][9] += row[a] * sample[2];
                }
            }

            for (int a = 5; a < 9; ++a)
                localMatrix[a][a] = 1;
            std::array<double, 9> localCoefficients{};

            if (!solve(localMatrix, localCoefficients))
                return fail("Local rim curvature is underdetermined; more wall samples are needed.");
            double x = -chart[i][0] / localScale, y = -chart[i][1] / localScale;
            first[i] = localCoefficients[0] * x + localCoefficients[1] * y;
            second[i] = 2 *
                (localCoefficients[2] * x * x + localCoefficients[3] * x * y + localCoefficients[4] * y * y);
        }

        int rings = std::min(40,
            std::min((19999 - int(input.vertices.size())) / int(rim.size()),
                (40000 - int(input.faces.size())) / (2 * int(rim.size()))));

        if (rings < 8) {
            result.capBudgetLimited = true;
            return fail("Insufficient mesh budget for curved closure.");
        }

        std::vector<int> previous = rim;

        for (int ring = 1; ring < rings; ++ring) {
            if (cancel()) {
                result.cancelled = true;
                return fail("Curved fit cancelled.");
            }

            double t = std::pow(double(ring) / rings, 1.4), q = 1 - t;
            double blend = q * q * q * (10 - 15 * q + 6 * q * q);
            double firstBlend = t - 6 * t * t * t + 8 * t * t * t * t - 3 * t * t * t * t * t;
            double secondBlend = 0.5 * t * t * q * q * q;
            std::vector<int> current;

            for (size_t i = 0; i < rim.size(); ++i) {
                double x = chart[i][0] * q, y = chart[i][1] * q, r2 = x * x + y * y;
                Vec3 direction = (u * (2 * x) + v * (2 * y) + pole * (1 - r2)) * (1 / (1 + r2));
                double distance =
                    radius(direction) + residual[i] * blend + first[i] * firstBlend + second[i] * secondBlend;

                if (!std::isfinite(distance) || distance <= scale * 1e-8)
                    return fail("Curvature continuation is not finite and positive.");
                current.push_back(int(result.vertices.size()));
                result.vertices.push_back(origin + direction * distance);
            }

            for (size_t i = 0; i < rim.size(); ++i) {
                size_t j = (i + 1) % rim.size();
                result.capFaces.push_back({previous[i], previous[j], current[i]});
                result.capFaces.push_back({previous[j], current[j], current[i]});
            }

            previous = std::move(current);
        }

        int tip = int(result.vertices.size());
        result.vertices.push_back(origin + pole * radius(pole));

        for (size_t i = 0; i < rim.size(); ++i)
            result.capFaces.push_back({previous[i], previous[(i + 1) % rim.size()], tip});
        return true;
    }
} // namespace mw
