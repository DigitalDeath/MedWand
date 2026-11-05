#pragma once
#include "reconstruction.h"

namespace mw {
    Mesh makeCapTestBowl();
    Mesh closeCurvedOpening(const Mesh &input, int opening, const std::function<bool()> &cancel);
    Mesh closeFlatOpening(
        const Mesh &input, int opening, double tolerance, const std::function<bool()> &cancel);
} // namespace mw
