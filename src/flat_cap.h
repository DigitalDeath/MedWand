#pragma once
#include "reconstruction.h"

namespace mw {
    Mesh makeCapTestBowl();
    Mesh closeFlatOpening(
        const Mesh &input, int opening, double tolerance, const std::function<bool()> &cancel);
} // namespace mw
