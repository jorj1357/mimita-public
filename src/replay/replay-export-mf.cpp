// 08 16 2026, 01 35
/* purpose
* Writes replay frames to H.264 MP4 with Windows Media Foundation on a worker thread.
* Selects the encoder by config encoderMode: auto (integrated Intel/AMD hardware
* first, software fallback, never the discrete GPU), discrete (NVIDIA hardware),
* or software (CPU only). The game thread never blocks on the encoder.
* Appends the pre-converted outro MP4 through the same encoder pipeline.
* Does NOT use FFmpeg at runtime, capture OpenGL pixels, or register commands.
* Does NOT advance replay playback state or own export notifications.
*/
#include "replay/replay-export.h"

#ifdef _WIN32

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include "video/outro.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"

namespace {

constexpr LONGLONG kFrameDuration = 10000000LL / 60LL; // 100 ns units
constexpr size_t kMaxQueuedFrames = 8;

template <typename T> void releaseCom(T*& value)
{
    if (value) {
        value->Release();
        value = nullptr;
    }
}

std::wstring widen(const std::string& text)
{
    if (text.empty()) return {};
    int count = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring result((size_t)std::max(0, count), L'\0');
    if (count > 1) {
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), count);
        result.pop_back();
    }
    return result;
}

std::string narrow(const wchar_t* text)
{
    if (!text || !*text) return {};
    int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string result((size_t)std::max(0, count), '\0');
    if (count > 1) {
        WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), count, nullptr, nullptr);
        result.pop_back();
    }
    return result;
}

std::string hrText(const char* action, HRESULT hr)
{
    char text[160];
    std::snprintf(text, sizeof(text), "%s failed (HRESULT 0x%08lx)", action, (unsigned long)hr);
    return text;
}

bool setVideoAttributes(IMFMediaType* type, const GUID& subtype, int width, int height)
{
    return SUCCEEDED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) &&
           SUCCEEDED(type->SetGUID(MF_MT_SUBTYPE, subtype)) &&
           SUCCEEDED(type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive)) &&
           SUCCEEDED(MFSetAttributeSize(type, MF_MT_FRAME_SIZE, (UINT32)width, (UINT32)height)) &&
           SUCCEEDED(MFSetAttributeRatio(type, MF_MT_FRAME_RATE, 60, 1)) &&
           SUCCEEDED(MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1));
}

// Convert a bottom-up RGB framebuffer into a top-down NV12 frame at writer size.
// dstStride is the byte pitch of each row in the NV12 planes (>= dstW).
static void rgbToNv12(const uint8_t* rgbBottomUp, int srcW, int srcH,
                      BYTE* nv12, int dstW, int dstH, int dstStride)
{
    BYTE* yPlane = nv12;
    BYTE* uvPlane = nv12 + dstStride * dstH;
    auto sourcePixel = [&](int x, int y) {
        int sx = std::min(srcW - 1, x * srcW / dstW);
        int syTop = std::min(srcH - 1, y * srcH / dstH);
        return rgbBottomUp + ((srcH - 1 - syTop) * srcW + sx) * 3;
    };
    for (int y = 0; y < dstH; ++y)
        for (int x = 0; x < dstW; ++x) {
            const uint8_t* p = sourcePixel(x, y);
            int value = ((66 * p[0] + 129 * p[1] + 25 * p[2] + 128) >> 8) + 16;
            yPlane[y * dstStride + x] = (BYTE)std::clamp(value, 0, 255);
        }
    for (int y = 0; y < dstH; y += 2)
        for (int x = 0; x < dstW; x += 2) {
            int u = 0, v = 0;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const uint8_t* p = sourcePixel(x + dx, y + dy);
                    u += ((-38 * p[0] - 74 * p[1] + 112 * p[2] + 128) >> 8) + 128;
                    v += ((112 * p[0] - 94 * p[1] - 18 * p[2] + 128) >> 8) + 128;
                }
            size_t uv = (size_t)(y / 2) * dstStride + x;
            uvPlane[uv] = (BYTE)std::clamp(u / 4, 0, 255);
            uvPlane[uv + 1] = (BYTE)std::clamp(v / 4, 0, 255);
        }
}

// Nearest-neighbor NV12 scale (used for the outro when its size differs).
// src pitch is srcW; dst pitch is dstStride.
static void scaleNv12(const BYTE* src, int srcW, int srcH,
                      BYTE* dst, int dstW, int dstH, int dstStride)
{
    const BYTE* srcY = src;
    const BYTE* srcUV = src + srcW * srcH;
    BYTE* dstY = dst;
    BYTE* dstUV = dst + dstStride * dstH;
    for (int y = 0; y < dstH; ++y) {
        int sy = std::min(srcH - 1, y * srcH / dstH);
        for (int x = 0; x < dstW; ++x) {
            int sx = std::min(srcW - 1, x * srcW / dstW);
            dstY[y * dstStride + x] = srcY[sy * srcW + sx];
        }
    }
    const int srcH2 = srcH / 2, dstH2 = dstH / 2;
    for (int y = 0; y < dstH2; ++y) {
        int sy = std::min(srcH2 - 1, y * srcH2 / dstH2);
        for (int x = 0; x < dstW; ++x) {
            int sx = std::min(srcW - 1, x * srcW / dstW);
            dstUV[y * dstStride + x] = srcUV[sy * srcW + sx];
        }
    }
}

// Feed PCM bytes to the sink writer's audio stream (AAC encoder lives inside the sink).
static bool writePcmWav(IMFSinkWriter* sink, DWORD stream, const std::string& path,
                        std::string& error)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long fileLen = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fileLen <= 44) { fclose(f); return false; }
    std::vector<uint8_t> bytes((size_t)fileLen);
    if (fread(bytes.data(), 1, (size_t)fileLen, f) != (size_t)fileLen) {
        fclose(f);
        return false;
    }
    fclose(f);

    size_t dataOffset = 12;
    uint32_t dataSize = 0;
    while (dataOffset + 8 <= bytes.size()) {
        uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, bytes.data() + dataOffset + 4, sizeof(chunkSize));
        if (std::memcmp(bytes.data() + dataOffset, "data", 4) == 0) {
            dataOffset += 8;
            dataSize = std::min<uint32_t>(chunkSize, (uint32_t)(bytes.size() - dataOffset));
            break;
        }
        dataOffset += 8 + chunkSize + (chunkSize & 1u);
    }
    if (dataSize == 0) return false;

    constexpr uint32_t bytesPerSecond = 48000u * 2u * 2u;
    constexpr uint32_t blockBytes = 48000u * 2u * 2u / 10u;
    LONGLONG sampleTime = 0;
    for (uint32_t offset = 0; offset < dataSize; offset += blockBytes) {
        DWORD size = std::min(blockBytes, dataSize - offset);
        IMFMediaBuffer* buffer = nullptr;
        IMFSample* sample = nullptr;
        HRESULT hr = MFCreateMemoryBuffer(size, &buffer);
        BYTE* target = nullptr;
        if (SUCCEEDED(hr)) hr = buffer->Lock(&target, nullptr, nullptr);
        if (SUCCEEDED(hr)) {
            std::memcpy(target, bytes.data() + dataOffset + offset, size);
            buffer->Unlock();
            hr = buffer->SetCurrentLength(size);
        }
        if (SUCCEEDED(hr)) hr = MFCreateSample(&sample);
        if (SUCCEEDED(hr)) hr = sample->AddBuffer(buffer);
        LONGLONG duration = (LONGLONG)size * 10000000LL / bytesPerSecond;
        if (SUCCEEDED(hr)) hr = sample->SetSampleTime(sampleTime);
        if (SUCCEEDED(hr)) hr = sample->SetSampleDuration(duration);
        if (SUCCEEDED(hr)) hr = sink->WriteSample(stream, sample);
        releaseCom(sample);
        releaseCom(buffer);
        if (FAILED(hr)) {
            error = hrText("AAC WriteSample", hr);
            return false;
        }
        sampleTime += duration;
    }
    return true;
}

enum class EncoderVendor { Discrete, Integrated, Unknown };

struct EncodeFrame {
    std::vector<uint8_t> rgb;
    int srcW = 0;
    int srcH = 0;
    uint32_t frameIndex = 0;
};

} // namespace

struct MfMp4Writer {
    // Init result (set on the worker, read by the game thread).
    std::atomic<bool> initReady{false};
    std::atomic<bool> initOk{false};
    std::string initError;

    // Encoder path.
    IMFSinkWriter* sink = nullptr;
    IMFTransform* encoder = nullptr; // null => software sink-writer path
    IMFDXGIDeviceManager* deviceManager = nullptr;
    DWORD videoStream = 0;
    DWORD audioStream = 0;
    int width = 0;
    int height = 0;
    int bitrate = 0;
    int nv12Stride = 0;
    std::string outputPath;
    std::string encoderModeConfig = "auto";
    bool audioEnabled = false;
    bool mfStarted = false;
    std::string encoderModeUsed = "software";

    // Worker queue.
    std::thread worker;
    std::mutex mtx;
    std::condition_variable cv;
    std::deque<EncodeFrame> queue;
    bool finalizeRequested = false;
    bool shutdown = false;
    std::string wavPath;
    std::string outroPath;
    bool outroEnabledConfig = true;
    std::string outroPathConfig;

    // Result (set before done).
    std::atomic<bool> done{false};
    std::atomic<bool> ok{false};
    std::string error;
    bool outroMissing = false;

    uint32_t framesWritten = 0;
};

namespace {

EncoderVendor classifyVendor(const std::string& name)
{
    const std::string lower = name;
    if (lower.find("nvidia") != std::string::npos) return EncoderVendor::Discrete;
    if (lower.find("intel") != std::string::npos) return EncoderVendor::Integrated;
    if (lower.find("amd") != std::string::npos) return EncoderVendor::Integrated;
    if (lower.find("radeon") != std::string::npos) return EncoderVendor::Integrated;
    return EncoderVendor::Unknown;
}

// Enumerate hardware H.264 encoders and pick one per the configured mode.
// Returns a new IMFTransform or nullptr (caller then falls back to software).
static IMFTransform* createH264Encoder(int width, int height, int bitrate,
                                       const std::string& mode,
                                       int& outStride,
                                       IMFDXGIDeviceManager*& outDeviceManager,
                                       std::string& modeUsed, EncoderVendor& chosen)
{
    MFT_REGISTER_TYPE_INFO inputType = { MFMediaType_Video, MFVideoFormat_NV12 };
    MFT_REGISTER_TYPE_INFO outputType = { MFMediaType_Video, MFVideoFormat_H264 };
    IMFActivate** activates = nullptr;
    UINT32 count = 0;
    HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER,
                           MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT,
                           &inputType, &outputType, &activates, &count);
    if (FAILED(hr) || count == 0)
        return nullptr;

    struct Candidate {
        IMFActivate* act = nullptr;
        EncoderVendor vendor = EncoderVendor::Unknown;
        std::string name;
    };
    std::vector<Candidate> candidates;
    for (UINT32 i = 0; i < count; ++i) {
        LPWSTR nameW = nullptr;
        UINT32 nameLen = 0;
        std::string name;
        if (SUCCEEDED(activates[i]->GetAllocatedString(MFT_FRIENDLY_NAME_Attribute, &nameW, &nameLen)))
            name = narrow(nameW);
        CoTaskMemFree(nameW);
        candidates.push_back({activates[i], classifyVendor(name), name});
    }

    IMFActivate* pick = nullptr;
    if (mode == "discrete") {
        for (auto& c : candidates) if (c.vendor == EncoderVendor::Discrete) { pick = c.act; modeUsed = c.name; chosen = c.vendor; break; }
        if (!pick) for (auto& c : candidates) { pick = c.act; modeUsed = c.name; chosen = c.vendor; break; }
    } else {
        // auto (default): integrated (or unknown) hardware only; never the discrete GPU.
        for (auto& c : candidates)
            if (c.vendor != EncoderVendor::Discrete) { pick = c.act; modeUsed = c.name; chosen = c.vendor; break; }
    }

    IMFTransform* enc = nullptr;
    HRESULT encHr = pick ? pick->ActivateObject(IID_PPV_ARGS(&enc)) : E_POINTER;
    for (UINT32 i = 0; i < count; ++i) activates[i]->Release();
    CoTaskMemFree(activates);
    if (FAILED(encHr)) {
        Debug::log(Debug::Category::Replay, "[EXPORT MF] encoder %s: ActivateObject failed 0x%08lx",
                   modeUsed.c_str(), (unsigned long)encHr);
        return nullptr;
    }

    // Hardware encoders generally need a D3D11 device manager (Intel QSV in
    // particular rejects media types without one). Best effort: if it fails we
    // continue and rely on the software fallback.
    {
        ID3D11Device* d3dDevice = nullptr;
        UINT resetToken = 0;
        if (SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                nullptr, 0, D3D11_SDK_VERSION, &d3dDevice, nullptr, nullptr))) {
            if (SUCCEEDED(MFCreateDXGIDeviceManager(&resetToken, &outDeviceManager))) {
                if (SUCCEEDED(outDeviceManager->ResetDevice(d3dDevice, resetToken)))
                    enc->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)outDeviceManager);
                else
                    releaseCom(outDeviceManager);
            }
            releaseCom(d3dDevice);
        }
        if (!outDeviceManager)
            Debug::log(Debug::Category::Replay, "[EXPORT MF] no DXGI device manager; hardware encoder may fall back");
    }

    IMFMediaType* inType = nullptr;
    IMFMediaType* outType = nullptr;
    hr = MFCreateMediaType(&inType);
    if (SUCCEEDED(hr) && !setVideoAttributes(inType, MFVideoFormat_NV12, width, height))
        hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = enc->SetInputType(0, inType, 0);
    if (FAILED(hr))
        Debug::log(Debug::Category::Replay, "[EXPORT MF] encoder %s: SetInputType failed 0x%08lx",
                   modeUsed.c_str(), (unsigned long)hr);
    if (SUCCEEDED(hr)) {
        // Honor the encoder's alignment so hardware encoders never read past the
        // buffer. The NV12 input must use this aligned stride end to end.
        MFT_INPUT_STREAM_INFO inInfo = {};
        if (SUCCEEDED(enc->GetInputStreamInfo(0, &inInfo)) && inInfo.cbAlignment > 1)
            outStride = (int)(((UINT32)width + inInfo.cbAlignment - 1) & ~(inInfo.cbAlignment - 1));
        else
            outStride = (width + 15) & ~15;
        if (SUCCEEDED(inType->SetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32)outStride)))
            enc->SetInputType(0, inType, 0);
    }
    releaseCom(inType);

    if (SUCCEEDED(hr)) hr = MFCreateMediaType(&outType);
    if (SUCCEEDED(hr) && !setVideoAttributes(outType, MFVideoFormat_H264, width, height))
        hr = E_FAIL;
    if (SUCCEEDED(hr))
        hr = outType->SetUINT32(MF_MT_AVG_BITRATE,
            bitrate > 0 ? (UINT32)bitrate * 1000u : 10000000u);
    if (SUCCEEDED(hr)) hr = enc->SetOutputType(0, outType, 0);
    if (FAILED(hr))
        Debug::log(Debug::Category::Replay, "[EXPORT MF] encoder %s: SetOutputType failed 0x%08lx",
                   modeUsed.c_str(), (unsigned long)hr);
    releaseCom(outType);

    if (FAILED(hr)) {
        releaseCom(enc);
        return nullptr;
    }
    return enc;
}

static HRESULT buildAacOutputType(IMFMediaType* outputAudio)
{
    HRESULT hr = outputAudio->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (SUCCEEDED(hr)) hr = outputAudio->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    if (SUCCEEDED(hr)) hr = outputAudio->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    if (SUCCEEDED(hr)) hr = outputAudio->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
    if (SUCCEEDED(hr)) hr = outputAudio->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000);
    if (SUCCEEDED(hr)) hr = outputAudio->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    return hr;
}

static void setAudioInputType(MfMp4Writer* w)
{
    IMFMediaType* inputAudio = nullptr;
    HRESULT hr = MFCreateMediaType(&inputAudio);
    if (SUCCEEDED(hr)) hr = inputAudio->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (SUCCEEDED(hr)) hr = inputAudio->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (SUCCEEDED(hr)) hr = inputAudio->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    if (SUCCEEDED(hr)) hr = inputAudio->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
    if (SUCCEEDED(hr)) hr = inputAudio->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
    if (SUCCEEDED(hr)) hr = inputAudio->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 192000);
    if (SUCCEEDED(hr)) hr = inputAudio->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    if (SUCCEEDED(hr)) hr = w->sink->SetInputMediaType(w->audioStream, inputAudio, nullptr);
    releaseCom(inputAudio);
    w->audioEnabled = SUCCEEDED(hr);
    if (!w->audioEnabled)
        Debug::log(Debug::Category::Replay, "[EXPORT MF] audio stream unavailable, exporting video-only");
}

static void addAudioStream(MfMp4Writer* w)
{
    IMFMediaType* outputAudio = nullptr;
    HRESULT hr = MFCreateMediaType(&outputAudio);
    if (SUCCEEDED(hr)) hr = buildAacOutputType(outputAudio);
    if (SUCCEEDED(hr)) hr = w->sink->AddStream(outputAudio, &w->audioStream);
    releaseCom(outputAudio);
    if (FAILED(hr)) { w->audioEnabled = false; return; }
    setAudioInputType(w);
}

// Run the encoder MFT until it has no more output, writing each sample to the sink.
static bool drainEncoder(MfMp4Writer* w)
{
    for (;;) {
        MFT_OUTPUT_DATA_BUFFER outBuf = {};
        outBuf.dwStreamID = 0;
        DWORD status = 0;
        HRESULT hr = w->encoder->ProcessOutput(0, 1, &outBuf, &status);
        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
            if (outBuf.pSample) outBuf.pSample->Release();
            return true;
        }
        if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
            if (outBuf.pSample) outBuf.pSample->Release();
            IMFMediaType* newType = nullptr;
            if (SUCCEEDED(w->encoder->GetOutputAvailableType(0, 0, &newType))) {
                w->encoder->SetOutputType(0, newType, 0);
                releaseCom(newType);
            }
            continue;
        }
        if (FAILED(hr)) {
            if (outBuf.pSample) outBuf.pSample->Release();
            w->error = hrText("H.264 ProcessOutput", hr);
            return false;
        }
        if (outBuf.pSample) {
            HRESULT whr = w->sink->WriteSample(w->videoStream, outBuf.pSample);
            outBuf.pSample->Release();
            if (FAILED(whr)) {
                w->error = hrText("encoder WriteSample", whr);
                return false;
            }
        }
    }
}

// Submit one NV12 frame through whichever video path is active.
static bool submitVideoSample(MfMp4Writer* w, const BYTE* nv12, size_t bytes,
                              LONGLONG time, LONGLONG duration)
{
    IMFMediaBuffer* buffer = nullptr;
    IMFSample* sample = nullptr;
    HRESULT hr = MFCreateMemoryBuffer((DWORD)bytes, &buffer);
    BYTE* dst = nullptr;
    if (SUCCEEDED(hr)) hr = buffer->Lock(&dst, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
        std::memcpy(dst, nv12, bytes);
        buffer->Unlock();
        hr = buffer->SetCurrentLength((DWORD)bytes);
    }
    if (SUCCEEDED(hr)) hr = MFCreateSample(&sample);
    if (SUCCEEDED(hr)) hr = sample->AddBuffer(buffer);
    if (SUCCEEDED(hr)) hr = sample->SetSampleTime(time);
    if (SUCCEEDED(hr)) hr = sample->SetSampleDuration(duration);

    if (SUCCEEDED(hr)) {
        if (w->encoder) {
            hr = w->encoder->ProcessInput(0, sample, 0);
            if (hr == MF_E_NOTACCEPTING) {
                if (!drainEncoder(w)) {
                    releaseCom(sample);
                    releaseCom(buffer);
                    return false;
                }
                hr = w->encoder->ProcessInput(0, sample, 0);
            }
            if (SUCCEEDED(hr) && !drainEncoder(w)) {
                releaseCom(sample);
                releaseCom(buffer);
                return false;
            }
        } else {
            hr = w->sink->WriteSample(w->videoStream, sample);
        }
    }
    releaseCom(sample);
    releaseCom(buffer);
    if (FAILED(hr)) {
        w->error = hrText("video submit", hr);
        return false;
    }
    w->framesWritten++;
    return true;
}

static bool encodeQueuedFrame(MfMp4Writer* w, EncodeFrame& frame)
{
    const int ww = w->width;
    const int wh = w->height;
    const int stride = w->nv12Stride > 0 ? w->nv12Stride : ww;
    thread_local std::vector<uint8_t> nv12;
    nv12.resize((size_t)stride * wh + (size_t)stride * wh / 2);
    rgbToNv12(frame.rgb.data(), frame.srcW, frame.srcH, nv12.data(), ww, wh, stride);
    return submitVideoSample(w, nv12.data(), nv12.size(),
                             (LONGLONG)frame.frameIndex * kFrameDuration, kFrameDuration);
}

// ---- Outro ----

static bool feedOutroVideo(MfMp4Writer* w, IMFSourceReader* reader)
{
    IMFMediaType* vmt = nullptr;
    if (FAILED(reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &vmt)))
        return false;
    UINT32 vw = 0, vh = 0;
    MFGetAttributeSize(vmt, MF_MT_FRAME_SIZE, &vw, &vh);
    releaseCom(vmt);
    if (vw == 0 || vh == 0) return false;

    const int ww = w->width, wh = w->height;
    const int stride = w->nv12Stride > 0 ? w->nv12Stride : ww;
    thread_local std::vector<uint8_t> scratch;
    scratch.resize((size_t)stride * wh + (size_t)stride * wh / 2);
    const LONGLONG base = (LONGLONG)w->framesWritten * kFrameDuration;

    for (;;) {
        IMFSample* sample = nullptr;
        DWORD streamIndex = 0, flags = 0;
        LONGLONG time = 0;
        HRESULT hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                                        &streamIndex, &flags, &time, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            releaseCom(sample);
            break;
        }
        if (!sample) continue;

        // Copy + scale the decoded NV12 frame into our scratch buffer (stride layout).
        bool okCopy = false;
        IMFMediaBuffer* buf = nullptr;
        if (SUCCEEDED(sample->GetBufferByIndex(0, &buf))) {
            IMF2DBuffer* two = nullptr;
            if (SUCCEEDED(buf->QueryInterface(IID_PPV_ARGS(&two)))) {
                BYTE* data = nullptr;
                LONG pitch = 0;
                if (SUCCEEDED(two->Lock2D(&data, &pitch))) {
                    if (pitch < 0) { data += (LONG)(vh - 1) * pitch; pitch = -pitch; }
                    if (vw == (UINT32)ww && vh == (UINT32)wh) {
                        for (int y = 0; y < wh; ++y)
                            std::memcpy(scratch.data() + (size_t)y * stride,
                                        data + (size_t)y * pitch, (size_t)ww);
                        const BYTE* srcUV = data + (size_t)pitch * vh;
                        BYTE* dstUV = scratch.data() + (size_t)stride * wh;
                        for (int y = 0; y < wh / 2; ++y)
                            std::memcpy(dstUV + (size_t)y * stride,
                                        srcUV + (size_t)y * pitch, (size_t)ww);
                    } else {
                        scaleNv12(data, (int)vw, (int)vh, scratch.data(), ww, wh, stride);
                    }
                    two->Unlock2D();
                    okCopy = true;
                }
                releaseCom(two);
            }
            releaseCom(buf);
        }
        LONGLONG duration = 0;
        sample->GetSampleDuration(&duration);
        if (duration <= 0) duration = kFrameDuration;
        releaseCom(sample);
        if (!okCopy) return false;
        if (!submitVideoSample(w, scratch.data(), scratch.size(), base + time, duration))
            return false;
    }
    return true;
}

static bool feedOutroAudio(MfMp4Writer* w, IMFSourceReader* reader)
{
    const LONGLONG base = (LONGLONG)w->framesWritten * kFrameDuration;
    for (;;) {
        IMFSample* sample = nullptr;
        DWORD streamIndex = 0, flags = 0;
        LONGLONG time = 0;
        HRESULT hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0,
                                        &streamIndex, &flags, &time, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            releaseCom(sample);
            break;
        }
        if (sample) {
            sample->SetSampleTime(base + time);
            HRESULT whr = w->sink->WriteSample(w->audioStream, sample);
            releaseCom(sample);
            if (FAILED(whr)) {
                w->error = hrText("outro audio WriteSample", whr);
                return false;
            }
        }
    }
    return true;
}

static void feedOutro(MfMp4Writer* w)
{
    namespace fs = std::filesystem;
    const bool outroEnabled = w->outroEnabledConfig;
    std::string outroPath = w->outroPathConfig;
    if (outroPath.find(".webm") != std::string::npos)
        outroPath.replace(outroPath.rfind(".webm"), 5, ".mp4");
    else if (outroPath.find(".WebM") != std::string::npos)
        outroPath.replace(outroPath.rfind(".WebM"), 5, ".mp4");

    Debug::log(Debug::Category::Replay, "[OUTRO] enabled=%d path=%s",
               (int)outroEnabled, outroPath.c_str());

    std::error_code ec;
    const bool mp4Exists = outroEnabled && fs::exists(outroPath, ec);
    Debug::log(Debug::Category::Replay, "[OUTRO] file exists=%d", (int)mp4Exists);
    if (!mp4Exists) {
        if (!outroEnabled) {
            Debug::log(Debug::Category::Replay, "[OUTRO] disabled; exported clip without outro");
        } else {
            const std::string webm = w->outroPathConfig;
            if (fs::exists(webm, ec))
                Debug::log(Debug::Category::Replay,
                           "[OUTRO] outro is webm and not converted to mp4; exported clip without outro");
            else
                Debug::log(Debug::Category::Replay, "[OUTRO] missing; exported clip without outro");
        }
        w->outroMissing = true;
        return;
    }

    Debug::log(Debug::Category::Replay, "[OUTRO] append start path=%s", outroPath.c_str());
    std::string absOutro = fs::absolute(outroPath).make_preferred().string();
    IMFSourceReader* reader = nullptr;
    HRESULT hr = MFCreateSourceReaderFromURL(widen(absOutro).c_str(), nullptr, &reader);
    if (FAILED(hr)) {
        w->error = hrText("outro source reader", hr);
        Debug::log(Debug::Category::Replay, "[OUTRO] append failed reason=source reader %s",
                   hrText("MFCreateSourceReaderFromURL", hr).c_str());
        return;
    }

    if (w->audioEnabled) {
        IMFMediaType* aOut = nullptr;
        if (SUCCEEDED(MFCreateMediaType(&aOut))) {
            aOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            aOut->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
            aOut->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
            aOut->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
            aOut->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
            reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, aOut);
            releaseCom(aOut);
        }
    }
    IMFMediaType* vOut = nullptr;
    if (SUCCEEDED(MFCreateMediaType(&vOut))) {
        vOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        vOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, vOut);
        releaseCom(vOut);
    }

    bool videoOk = feedOutroVideo(w, reader);
    bool audioOk = w->audioEnabled ? feedOutroAudio(w, reader) : true;
    releaseCom(reader);

    if (!videoOk || !audioOk) {
        if (w->error.empty()) w->error = "outro feed failed";
        Debug::log(Debug::Category::Replay, "[OUTRO] append failed reason=%s", w->error.c_str());
        return;
    }
    Debug::log(Debug::Category::Replay, "[OUTRO] append success finalPath=%s",
               fs::absolute(w->outputPath).make_preferred().string().c_str());
}

// ---- Sink creation ----

static HRESULT createSoftwareSink(MfMp4Writer* w)
{
    std::wstring wide = widen(std::filesystem::absolute(w->outputPath).make_preferred().string());
    HRESULT hr = MFCreateSinkWriterFromURL(wide.c_str(), nullptr, nullptr, &w->sink);
    if (FAILED(hr)) return hr;

    IMFMediaType* outputVideo = nullptr;
    IMFMediaType* inputVideo = nullptr;
    if (SUCCEEDED(hr)) hr = MFCreateMediaType(&outputVideo);
    if (SUCCEEDED(hr) && !setVideoAttributes(outputVideo, MFVideoFormat_H264, w->width, w->height))
        hr = E_FAIL;
    if (SUCCEEDED(hr))
        hr = outputVideo->SetUINT32(MF_MT_AVG_BITRATE,
            w->bitrate > 0 ? (UINT32)w->bitrate * 1000u : 10000000u);
    if (SUCCEEDED(hr)) hr = w->sink->AddStream(outputVideo, &w->videoStream);
    if (SUCCEEDED(hr)) hr = MFCreateMediaType(&inputVideo);
    if (SUCCEEDED(hr) && !setVideoAttributes(inputVideo, MFVideoFormat_NV12, w->width, w->height))
        hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = w->sink->SetInputMediaType(w->videoStream, inputVideo, nullptr);
    releaseCom(inputVideo);
    releaseCom(outputVideo);
    if (FAILED(hr)) return hr;

    addAudioStream(w);
    return w->sink->BeginWriting();
}

static HRESULT createHardwareSink(MfMp4Writer* w)
{
    IMFMediaType* encOut = nullptr;
    HRESULT hr = w->encoder->GetOutputCurrentType(0, &encOut);
    if (FAILED(hr)) {
        Debug::log(Debug::Category::Replay, "[EXPORT MF] hardware sink: GetOutputCurrentType failed 0x%08lx", (unsigned long)hr);
        return hr;
    }

    std::wstring wide = widen(std::filesystem::absolute(w->outputPath).make_preferred().string());
    hr = MFCreateSinkWriterFromURL(wide.c_str(), nullptr, nullptr, &w->sink);
    if (FAILED(hr)) {
        Debug::log(Debug::Category::Replay, "[EXPORT MF] hardware sink: MFCreateSinkWriterFromURL failed 0x%08lx", (unsigned long)hr);
        releaseCom(encOut);
        return hr;
    }
    hr = w->sink->AddStream(encOut, &w->videoStream);
    if (FAILED(hr)) {
        Debug::log(Debug::Category::Replay, "[EXPORT MF] hardware sink: AddStream(video) failed 0x%08lx", (unsigned long)hr);
        releaseCom(encOut);
        return hr;
    }
    // Feed the encoder's COMPRESSED H.264 samples directly (input type == output
    // type), so the sink writer inserts no video encoder.
    hr = w->sink->SetInputMediaType(w->videoStream, encOut, nullptr);
    releaseCom(encOut);
    if (FAILED(hr)) {
        Debug::log(Debug::Category::Replay, "[EXPORT MF] hardware sink: SetInputMediaType(video) failed 0x%08lx", (unsigned long)hr);
        return hr;
    }
    addAudioStream(w);
    return w->sink->BeginWriting();
}

// ---- Worker ----

static void workerLoop(MfMp4Writer* w)
{
    const HRESULT coInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HRESULT startup = MFStartup(MF_VERSION);
    if (FAILED(startup)) {
        w->initError = hrText("MFStartup", startup);
        w->initOk = false;
        w->initReady = true;
        if (SUCCEEDED(coInit)) CoUninitialize();
        return;
    }
    w->mfStarted = true;

    const std::string& mode = w->encoderModeConfig;
    EncoderVendor chosen = EncoderVendor::Unknown;
    int stride = 0;
    if (mode != "software") {
        w->encoder = createH264Encoder(w->width, w->height, w->bitrate,
                                       mode, stride, w->deviceManager,
                                       w->encoderModeUsed, chosen);
    }
    w->nv12Stride = w->encoder ? stride : w->width;
    HRESULT hr = w->encoder ? createHardwareSink(w) : createSoftwareSink(w);
    if (FAILED(hr) && w->encoder) {
        // Hardware encoder present but the MP4 sink path failed: fall back to the
        // software sink writer (auto-resolved H.264 encoder) so export still works.
        Debug::log(Debug::Category::Replay, "[EXPORT MF] hardware sink failed, falling back to software (%s)",
                   hrText("sink", hr).c_str());
        releaseCom(w->encoder);
        releaseCom(w->deviceManager);
        releaseCom(w->sink);
        w->encoder = nullptr;
        w->encoderModeUsed = "software";
        hr = createSoftwareSink(w);
    }
    if (FAILED(hr)) {
        releaseCom(w->encoder);
        releaseCom(w->deviceManager);
        releaseCom(w->sink);
        w->initError = hrText(w->encoderModeUsed.empty() ? "software sink" : w->encoderModeUsed.c_str(), hr);
        w->initOk = false;
        w->initReady = true;
        MFShutdown();
        w->mfStarted = false;
        if (SUCCEEDED(coInit)) CoUninitialize();
        return;
    }
    Debug::warn(Debug::Category::Replay,
                "[EXPORT MF] encoder path=%s mode=%s size=%dx%d",
                w->encoder ? "hardware" : "software",
                w->encoderModeUsed.c_str(), w->width, w->height);
    w->initOk = true;
    w->initReady = true;

    // Frame pump.
    for (;;) {
        EncodeFrame frame;
        bool have = false;
        bool doFinalize = false;
        {
            std::unique_lock<std::mutex> lock(w->mtx);
            w->cv.wait(lock, [&] {
                return !w->queue.empty() || w->finalizeRequested || w->shutdown;
            });
            if (w->shutdown) break;
            if (!w->queue.empty()) {
                frame = std::move(w->queue.front());
                w->queue.pop_front();
                have = true;
            } else if (w->finalizeRequested) {
                doFinalize = true;
            }
        }
        if (have) {
            if (!encodeQueuedFrame(w, frame)) {
                w->ok = false;
                w->error = w->error.empty() ? "encode frame failed" : w->error;
                w->done = true;
                MFShutdown();
                w->mfStarted = false;
                if (SUCCEEDED(coInit)) CoUninitialize();
                return;
            }
            continue;
        }
        if (doFinalize) break;
    }

    // Cancel path: nothing more to do.
    bool wantFinalize = false;
    {
        std::lock_guard<std::mutex> lock(w->mtx);
        wantFinalize = w->finalizeRequested;
    }
    if (!wantFinalize) {
        releaseCom(w->sink);
        releaseCom(w->encoder);
        releaseCom(w->deviceManager);
        w->ok = false;
        w->done = true;
        MFShutdown();
        w->mfStarted = false;
        if (SUCCEEDED(coInit)) CoUninitialize();
        return;
    }

    // Finalize phase: drain encoder, feed clip audio, feed outro, finalize sink.
    if (w->encoder) {
        w->encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        w->encoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        drainEncoder(w);
    }

    std::string wavPath;
    std::string outroPath;
    {
        std::lock_guard<std::mutex> lock(w->mtx);
        wavPath = w->wavPath;
        outroPath = w->outroPath;
    }
    if (w->audioEnabled && !wavPath.empty())
        writePcmWav(w->sink, w->audioStream, wavPath, w->error);
    if (!outroPath.empty() && w->error.empty())
        feedOutro(w);

    HRESULT finalHr = w->sink->Finalize();
    if (FAILED(finalHr) && w->error.empty())
        w->error = hrText("Finalize", finalHr);

    releaseCom(w->encoder);
        releaseCom(w->deviceManager);
    releaseCom(w->sink);
    w->ok = w->error.empty() && SUCCEEDED(finalHr);
    w->done = true;
    MFShutdown();
    w->mfStarted = false;
    if (SUCCEEDED(coInit)) CoUninitialize();
}

} // namespace

// ---- Public API ----

bool startMfReplayExport(MfMp4Writer*& writer, const std::string& outputPath,
                         int width, int height, int bitrate, std::string& error)
{
    writer = new MfMp4Writer();
    writer->width = width & ~1;
    writer->height = height & ~1;
    if (writer->width < 2 || writer->height < 2) {
        error = "invalid export size";
        delete writer;
        writer = nullptr;
        return false;
    }
    writer->bitrate = bitrate;
    writer->outputPath = outputPath;
    writer->encoderModeConfig = gExportConfig.encoderMode;
    writer->worker = std::thread(workerLoop, writer);
    return true;
}

bool mfReplayInitReady(MfMp4Writer* writer)
{
    return writer && writer->initReady.load();
}

bool mfReplayInitSucceeded(MfMp4Writer* writer)
{
    return writer && writer->initReady.load() && writer->initOk.load();
}

std::string mfReplayEncoderMode(MfMp4Writer* writer)
{
    return writer ? writer->encoderModeUsed : std::string();
}

bool writeMfReplayVideoFrame(MfMp4Writer* writer, const uint8_t* rgbBottomUp,
                             int sourceWidth, int sourceHeight, uint32_t frameIndex,
                             bool* accepted, std::string& error)
{
    if (accepted) *accepted = false;
    if (!writer) { error = "Media Foundation writer is not active"; return false; }
    if (!writer->initReady.load()) return true; // init in progress; skip this frame
    if (!writer->initOk.load()) { error = writer->initError; return false; }

    EncodeFrame frame;
    frame.rgb.assign(rgbBottomUp, rgbBottomUp + (size_t)sourceWidth * sourceHeight * 3);
    frame.srcW = sourceWidth;
    frame.srcH = sourceHeight;
    frame.frameIndex = frameIndex;
    {
        std::lock_guard<std::mutex> lock(writer->mtx);
        if (writer->shutdown || writer->finalizeRequested) return true;
        if (writer->queue.size() >= kMaxQueuedFrames) return true; // backpressure: skip this frame
        writer->queue.push_back(std::move(frame));
    }
    writer->cv.notify_one();
    if (accepted) *accepted = true;
    return true;
}

void finishMfReplayExport(MfMp4Writer*& writer, const std::string& wavPath,
                          const std::string& outroPath)
{
    if (!writer) return;
    {
        std::lock_guard<std::mutex> lock(writer->mtx);
        writer->wavPath = wavPath;
        writer->outroPath = outroPath;
        writer->outroEnabledConfig = gOutroConfig.enabled;
        writer->outroPathConfig = gOutroConfig.outroPath;
        writer->finalizeRequested = true;
    }
    writer->cv.notify_all();
}

bool pollMfReplayExport(MfMp4Writer*& writer, bool& ok, bool& outroMissing,
                        std::string& error)
{
    if (!writer) { ok = false; error = "Media Foundation writer is not active"; return true; }
    if (!writer->done.load()) return false;
    ok = writer->ok.load();
    outroMissing = writer->outroMissing;
    error = writer->error;
    if (writer->worker.joinable()) writer->worker.join();
    cancelMfReplayExport(writer);
    return true;
}

void cancelMfReplayExport(MfMp4Writer*& writer)
{
    if (!writer) return;
    {
        std::lock_guard<std::mutex> lock(writer->mtx);
        writer->shutdown = true;
    }
    writer->cv.notify_all();
    if (writer->worker.joinable()) writer->worker.join();
    releaseCom(writer->sink);
    releaseCom(writer->encoder);
    releaseCom(writer->deviceManager);
    delete writer;
    writer = nullptr;
}

void exportMfDiag()
{
    MFT_REGISTER_TYPE_INFO inputType = { MFMediaType_Video, MFVideoFormat_NV12 };
    MFT_REGISTER_TYPE_INFO outputType = { MFMediaType_Video, MFVideoFormat_H264 };
    IMFActivate** activates = nullptr;
    UINT32 count = 0;
    HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER,
                           MFT_ENUM_FLAG_ALL | MFT_ENUM_FLAG_HARDWARE,
                           &inputType, &outputType, &activates, &count);
    if (FAILED(hr) || count == 0) {
        Terminal::instance().addLog("[MF DIAG] no H.264 encoders found");
        return;
    }
    for (UINT32 i = 0; i < count; ++i) {
        LPWSTR nameW = nullptr;
        UINT32 nameLen = 0;
        std::string name = "(unnamed)";
        if (SUCCEEDED(activates[i]->GetAllocatedString(MFT_FRIENDLY_NAME_Attribute, &nameW, &nameLen)))
            name = narrow(nameW);
        CoTaskMemFree(nameW);
        const char* vendor = "unknown";
        switch (classifyVendor(name)) {
            case EncoderVendor::Discrete: vendor = "discrete"; break;
            case EncoderVendor::Integrated: vendor = "integrated"; break;
            default: vendor = "unknown"; break;
        }
        Terminal::instance().addLog(std::string("[MF DIAG] H.264 encoder: ") + name +
                                    "  (" + vendor + ")");
        activates[i]->Release();
    }
    CoTaskMemFree(activates);
    Terminal::instance().addLog(std::string("[MF DIAG] encoderMode=") + gExportConfig.encoderMode);
}

#else

struct MfMp4Writer {};

bool startMfReplayExport(MfMp4Writer*&, const std::string&, int, int, int, std::string& error)
{ error = "Windows exporter unavailable"; return false; }
bool mfReplayInitReady(MfMp4Writer*) { return true; }
bool mfReplayInitSucceeded(MfMp4Writer*) { return false; }
std::string mfReplayEncoderMode(MfMp4Writer*) { return "unavailable"; }
bool writeMfReplayVideoFrame(MfMp4Writer*, const uint8_t*, int, int, uint32_t, bool*, std::string& error)
{ error = "Windows exporter unavailable"; return false; }
void finishMfReplayExport(MfMp4Writer*&, const std::string&, const std::string&) {}
bool pollMfReplayExport(MfMp4Writer*&, bool& ok, bool&, std::string& error)
{ ok = false; error = "Windows exporter unavailable"; return true; }
void cancelMfReplayExport(MfMp4Writer*&) {}
void exportMfDiag() {}

#endif
