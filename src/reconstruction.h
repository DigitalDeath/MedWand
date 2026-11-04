#pragma once
#include "core.h"
#include <functional>

namespace mw {
    struct MeshSettings {
        double mergeDistance = 0.5, maxEdge = 4.0;
        bool flatCap = false;
        double capTolerance = 0.5;
        int capOpening = 0;
    };

    struct Mesh {
        std::vector<Vec3> vertices;
        std::vector<std::array<int, 3>> faces;
        std::vector<std::array<int, 3>> capFaces;
        bool volumeValid = false;
        double volumeMm3 = 0;
        size_t openings = 0;
        std::string capStatus;
        bool cancelled = false, limited = false;
    };

    Mesh reconstruct(const std::vector<Vec3> &, MeshSettings, const std::function<bool()> &);
} // namespace mw
