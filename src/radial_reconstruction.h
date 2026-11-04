#pragma once
#include "reconstruction.h"

namespace mw {
    Mesh reconstructRadial(
        const std::vector<Vec3> &points, MeshSettings settings, const std::function<bool()> &cancel);
}
