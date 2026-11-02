#pragma once

#include "FairAgent.h"

#include <cstdint>

namespace realgame {

// Returns a process exit code: 0 when every converted state matched.
int runSelfTest(const fair::Params &params, std::uint64_t seedStart, int count, int ascension);

}  // namespace realgame
