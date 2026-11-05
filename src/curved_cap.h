#pragma once
#include "reconstruction.h"

namespace mw {
    bool buildCurvedCap(Mesh &result, const Mesh &input, const std::vector<int> &rim,
        const std::function<bool()> &cancel, std::string &error);
}
