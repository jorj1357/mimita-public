#pragma once

#include <cstdint>

namespace Perf {

enum class ReportMode : int {
    Off = 0,
    Live = 1,
    Compact = 2,
    FullSampled = 3,
};

int reportMode();
void setReportMode(int mode);
uint32_t reportSampleMs();
void setReportSampleMs(uint32_t ms);

}  // namespace Perf
