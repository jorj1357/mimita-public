// 2026-11-12
// Game-DLL-side partial audio bridge for CAP_AUDIO.
//
// All device ownership stays resident. The game generation forwards typed
// audio requests; it never opens a device, decodes to a device, or owns a
// voice. No silent stubs: unsupported ops fail and log at the owner.

#include <cstring>

#include "audio/audio.h"
#include "audio/music-manager.h"
#include "host/abi/audio-capability.h"
#include "host/abi/game-host-abi.h"

namespace mimita::game {

void initializeHostAudio(const mimita::host::HostServicesV1* services);

namespace {
const mimita::host::AudioCapabilityV1* gAudio = nullptr;
void* gHostData = nullptr;

bool invoke(mimita::host::AudioOp op, const void* input, std::uint32_t inputSize,
            void* output, std::uint32_t outputSize) {
    if (!gAudio || !gAudio->invoke) {
        return false;
    }
    return gAudio->invoke(gHostData, op, input, inputSize, output, outputSize) != 0;
}

void copyName(char* destination, std::size_t capacity, const std::string& name) {
    if (!destination || capacity == 0) {
        return;
    }
    const std::size_t count = name.size() < capacity - 1 ? name.size() : capacity - 1;
    std::memcpy(destination, name.data(), count);
    destination[count] = 0;
}

template <typename T>
bool sendScalar(mimita::host::AudioOp op, T value) {
    return invoke(op, &value, sizeof(value), nullptr, 0);
}

template <typename T>
T receiveScalar(mimita::host::AudioOp op) {
    T value{};
    invoke(op, nullptr, 0, &value, sizeof(value));
    return value;
}
}  // namespace

void initializeHostAudio(const mimita::host::HostServicesV1* services) {
    gAudio = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    auto* capability = static_cast<const mimita::host::AudioCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Audio),
            mimita::host::kAudioCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kAudioCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::AudioCapabilityV1)) {
        gAudio = capability;
    }
}

}  // namespace mimita::game

AudioManager& AudioManager::instance() {
    static AudioManager manager;
    return manager;
}

void AudioManager::play(const AudioEvent& event) {
    mimita::host::AudioEventPayload payload{};
    mimita::game::copyName(payload.name, sizeof(payload.name), event.name);
    payload.category = static_cast<std::int32_t>(event.category);
    payload.world = event.world ? 1 : 0;
    payload.position[0] = event.position.x;
    payload.position[1] = event.position.y;
    payload.position[2] = event.position.z;
    payload.volume = event.volume;
    payload.pitch = event.pitch;
    payload.maxDistance = event.maxDistance;
    payload.ownerId = event.ownerId;
    payload.startSeconds = event.startSeconds;
    payload.endSeconds = event.endSeconds;
    payload.retrigger = event.retrigger ? 1 : 0;
    mimita::game::invoke(mimita::host::AudioOp::AudioPlay, &payload,
                         sizeof(payload), nullptr, 0);
}

void AudioManager::stopOwner(unsigned int ownerId) {
    mimita::game::sendScalar(mimita::host::AudioOp::AudioStopOwner,
                             static_cast<std::uint32_t>(ownerId));
}

void AudioManager::stopAll() {
    mimita::game::invoke(mimita::host::AudioOp::AudioStopAll, nullptr, 0, nullptr, 0);
}

void AudioManager::setDebug(bool enabled) {
    mimita::game::sendScalar(mimita::host::AudioOp::AudioSetDebug,
                             static_cast<std::int32_t>(enabled ? 1 : 0));
}

bool AudioManager::debug() const {
    return mimita::game::receiveScalar<std::int32_t>(
               mimita::host::AudioOp::AudioDebug) != 0;
}

MusicManager& MusicManager::instance() {
    static MusicManager manager;
    return manager;
}

MusicManager::~MusicManager() = default;

void MusicManager::applyVolume() {}

void MusicManager::update(float dt) {
    mimita::game::sendScalar(mimita::host::AudioOp::MusicUpdate, dt);
}

void MusicManager::enterMenuMode() {
    mimita::game::invoke(mimita::host::AudioOp::MusicEnterMenuMode, nullptr, 0,
                         nullptr, 0);
}

void MusicManager::enterGameMode() {
    mimita::game::invoke(mimita::host::AudioOp::MusicEnterGameMode, nullptr, 0,
                         nullptr, 0);
}

bool MusicManager::playModeTrack(const std::string& path) {
    std::int32_t played = 0;
    mimita::game::invoke(mimita::host::AudioOp::MusicPlayModeTrack, path.c_str(),
                         static_cast<std::uint32_t>(path.size() + 1), &played,
                         sizeof(played));
    return played != 0;
}

void MusicManager::stopModeTrack() {
    mimita::game::invoke(mimita::host::AudioOp::MusicStopModeTrack, nullptr, 0,
                         nullptr, 0);
}

void MusicManager::pause() {
    mimita::game::invoke(mimita::host::AudioOp::MusicPause, nullptr, 0, nullptr, 0);
}

void MusicManager::resume() {
    mimita::game::invoke(mimita::host::AudioOp::MusicResume, nullptr, 0, nullptr, 0);
}

void MusicManager::skip() {
    mimita::game::invoke(mimita::host::AudioOp::MusicSkip, nullptr, 0, nullptr, 0);
}

void MusicManager::previous() {
    mimita::game::invoke(mimita::host::AudioOp::MusicPrevious, nullptr, 0, nullptr, 0);
}

void MusicManager::setPlaylistMode(PlaylistMode mode) {
    mimita::game::sendScalar(mimita::host::AudioOp::MusicSetPlaylistMode,
                             static_cast<std::int32_t>(mode));
}

void MusicManager::setPersonalMusicOverride(bool enabled) {
    mimita::game::sendScalar(
        mimita::host::AudioOp::MusicSetPersonalMusicOverride,
        static_cast<std::int32_t>(enabled ? 1 : 0));
}

void MusicManager::stop() {
    mimita::game::invoke(mimita::host::AudioOp::MusicStop, nullptr, 0, nullptr, 0);
}

void MusicManager::reload() {
    mimita::game::invoke(mimita::host::AudioOp::MusicReload, nullptr, 0, nullptr, 0);
}

void MusicManager::setVolume(float vol) {
    mimita::game::sendScalar(mimita::host::AudioOp::MusicSetVolume, vol);
}

float MusicManager::volume() const {
    return mimita::game::receiveScalar<float>(mimita::host::AudioOp::MusicVolume);
}

void MusicManager::setPlaybackSpeed(float speed) {
    mimita::game::sendScalar(mimita::host::AudioOp::MusicSetPlaybackSpeed, speed);
}

float MusicManager::playbackSpeed() const {
    return mimita::game::receiveScalar<float>(
        mimita::host::AudioOp::MusicPlaybackSpeed);
}

void MusicManager::setMuted(bool m) {
    mimita::game::sendScalar(mimita::host::AudioOp::MusicSetMuted,
                             static_cast<std::int32_t>(m ? 1 : 0));
}

bool MusicManager::muted() const {
    return mimita::game::receiveScalar<std::int32_t>(
               mimita::host::AudioOp::MusicMuted) != 0;
}

bool MusicManager::isPlaying() const {
    return mimita::game::receiveScalar<std::int32_t>(
               mimita::host::AudioOp::MusicIsPlaying) != 0;
}

std::string MusicManager::currentTrackInfo() const {
    char info[512]{};
    mimita::game::invoke(mimita::host::AudioOp::MusicCurrentTrackInfo, nullptr, 0,
                         info, sizeof(info));
    return std::string(info);
}

void playWorldSound(const std::string& name, glm::vec3 pos, float volume,
                    float pitch, float maxDistance, AudioCategory category) {
    mimita::host::WorldSoundPayload payload{};
    mimita::game::copyName(payload.name, sizeof(payload.name), name);
    payload.position[0] = pos.x;
    payload.position[1] = pos.y;
    payload.position[2] = pos.z;
    payload.volume = volume;
    payload.pitch = pitch;
    payload.maxDistance = maxDistance;
    payload.category = static_cast<std::int32_t>(category);
    mimita::game::invoke(mimita::host::AudioOp::PlayWorldSound, &payload,
                         sizeof(payload), nullptr, 0);
}

void playWorldSoundOwned(const std::string& name, glm::vec3 pos, float volume,
                         float pitch, float maxDistance, unsigned int ownerId,
                         bool retrigger, AudioCategory category) {
    mimita::host::WorldSoundPayload payload{};
    mimita::game::copyName(payload.name, sizeof(payload.name), name);
    payload.position[0] = pos.x;
    payload.position[1] = pos.y;
    payload.position[2] = pos.z;
    payload.volume = volume;
    payload.pitch = pitch;
    payload.maxDistance = maxDistance;
    payload.ownerId = ownerId;
    payload.retrigger = retrigger ? 1 : 0;
    payload.category = static_cast<std::int32_t>(category);
    mimita::game::invoke(mimita::host::AudioOp::PlayWorldSoundOwned, &payload,
                         sizeof(payload), nullptr, 0);
}

glm::vec3 audioListenerPosition() {
    float position[3]{};
    mimita::game::invoke(mimita::host::AudioOp::AudioListenerPosition, nullptr, 0,
                         position, sizeof(position));
    return glm::vec3(position[0], position[1], position[2]);
}

void computeImpactAudio(float baseVolume, float distance, float severity,
                        float& outVolume, float& outPitch) {
    mimita::host::ImpactAudioPayload payload{};
    payload.baseVolume = baseVolume;
    payload.distance = distance;
    payload.severity = severity;
    float result[2]{};
    mimita::game::invoke(mimita::host::AudioOp::ComputeImpactAudio, &payload,
                         sizeof(payload), result, sizeof(result));
    outVolume = result[0];
    outPitch = result[1];
}
