// 2026-11-13
// Game-DLL-side pure audio analysis and analysis logging.
//
// The analysis is stateless math. Logging goes through the game StructuredLogger
// bridge, which forwards one bounded event to the resident events.jsonl sink.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "debug/structured-log.h"

namespace {
template <typename T>
AudioBufferAnalysis analyzeImpl(const std::vector<T>& buffer, std::uint32_t frameCount,
                                std::uint16_t channels, std::uint32_t sampleRate,
                                double threshold) {
    AudioBufferAnalysis a;
    a.sampleRate = sampleRate;
    a.channels = channels;
    a.frameCount = frameCount;
    a.sampleCount = buffer.size();
    a.durationSec = frameCount > 0 ? static_cast<double>(frameCount) / sampleRate : 0.0;
    const std::uint64_t totalSamples = buffer.size();
    if (totalSamples == 0 || frameCount == 0 || channels == 0) return a;

    a.minSample = std::numeric_limits<double>::max();
    a.maxSample = -std::numeric_limits<double>::max();
    double sum = 0.0;
    double sumSq = 0.0;
    std::vector<double> prevSample(channels, 0.0);
    std::vector<bool> firstSamplePerCh(channels, true);

    for (std::uint64_t i = 0; i < totalSamples; ++i) {
        double s = static_cast<double>(buffer[i]);
        const int ch = static_cast<int>(i % channels);
        if (std::numeric_limits<T>::is_integer) {
            s /= static_cast<double>(std::numeric_limits<T>::max());
        }
        if (std::isnan(s)) { ++a.nanCount; continue; }
        if (std::isinf(s)) { ++a.infCount; continue; }
        if (s < a.minSample) a.minSample = s;
        if (s > a.maxSample) a.maxSample = s;
        sum += s;
        sumSq += s * s;
        if (std::fabs(s) <= 0.0001) ++a.zeroCount;
        if (std::fabs(s) >= 1.0) ++a.clipCount;
        if (!firstSamplePerCh[ch]) {
            const double delta = std::fabs(s - prevSample[ch]);
            if (delta > threshold) {
                ++a.discontinuityCount;
                if (delta > a.largestDiscontinuity) a.largestDiscontinuity = delta;
            }
        }
        prevSample[ch] = s;
        firstSamplePerCh[ch] = false;
        if (i < 16) a.firstSamples.push_back(s);
        if (i >= totalSamples - 16) a.lastSamples.push_back(s);
    }

    const std::uint64_t validSamples = totalSamples - a.nanCount - a.infCount;
    if (validSamples > 0) {
        a.mean = sum / static_cast<double>(validSamples);
        a.rms = std::sqrt(sumSq / static_cast<double>(validSamples));
    }
    a.peak = std::max(std::fabs(a.minSample), std::fabs(a.maxSample));
    return a;
}
}  // namespace

AudioBufferAnalysis analyzeAudioBuffer(
    const std::vector<float>& buffer, std::uint32_t frameCount,
    std::uint16_t channels, std::uint32_t sampleRate,
    std::uint32_t discontinuityThreshold) {
    return analyzeImpl<float>(buffer, frameCount, channels, sampleRate,
                              static_cast<double>(discontinuityThreshold));
}

AudioBufferAnalysis analyzeAudioBuffer(
    const std::vector<std::int16_t>& buffer, std::uint32_t frameCount,
    std::uint16_t channels, std::uint32_t sampleRate,
    std::uint32_t discontinuityThreshold) {
    return analyzeImpl<std::int16_t>(buffer, frameCount, channels, sampleRate,
                                     static_cast<double>(discontinuityThreshold));
}

void logAudioAnalysis(StructuredCategory cat, StructuredLevel level,
                      const std::string& eventId, const std::string& correlationId,
                      const std::string& stage, const AudioBufferAnalysis& a) {
    if (!StructuredLogger::instance().shouldLog(cat, level)) return;
    StructuredLogger::Entry e;
    e.category = cat;
    e.level = level;
    e.eventId = eventId;
    e.correlationId = correlationId;
    e.reason = "Audio buffer analysis: " + stage;
    e.sourceFile = __FILE__;
    e.sourceLine = __LINE__;
    e.functionName = __FUNCTION__;
    e.numericKeys = {
        "sampleRate", "channels", "frameCount", "durationSec",
        "minSample", "maxSample", "mean", "rms", "peak",
        "zeroCount", "nanCount", "infCount", "clipCount",
        "discontinuityCount", "largestDiscontinuity"
    };
    e.numericExpected = {
        static_cast<double>(a.sampleRate), static_cast<double>(a.channels),
        static_cast<double>(a.frameCount), a.durationSec, a.minSample,
        a.maxSample, a.mean, a.rms, a.peak, static_cast<double>(a.zeroCount),
        static_cast<double>(a.nanCount), static_cast<double>(a.infCount),
        static_cast<double>(a.clipCount), static_cast<double>(a.discontinuityCount),
        a.largestDiscontinuity
    };
    e.numericActual = e.numericExpected;
    e.tolerance = 0.0;
    StructuredLogger::instance().write(e);
}
