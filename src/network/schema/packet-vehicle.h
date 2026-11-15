// 2026-11-14
// MiMITA vehicle packet wire structs.
#pragma once

#include "network/schema/packet-common.h"
#include "network/schema/packet-header.h"

namespace MimitaNet {

#pragma pack(push, 1)

constexpr int MAX_VEHICLE_TEMPLATE_BYTES = 32;
constexpr int MAX_VEHICLE_SEATS = 4;
constexpr int MAX_VEHICLE_WHEELS = 8;
constexpr int MAX_VEHICLE_STATE_ENTRIES = 4;

enum VehicleInputFlags : uint8_t
{
    VEHICLE_INPUT_ENGINE_TOGGLE = 1 << 0,
    VEHICLE_INPUT_MODE_TOGGLE = 1 << 1,
    VEHICLE_INPUT_SHIFT_UP = 1 << 2,
    VEHICLE_INPUT_SHIFT_DOWN = 1 << 3
};

enum VehicleSeatAction : uint8_t
{
    VEHICLE_SEAT_ENTER = 1,
    VEHICLE_SEAT_EXIT = 2
};

struct VehicleInputPacket
{
    PacketHeader header;
    uint32_t sequence = 0;
    uint32_t vehicleId = 0;
    float throttle = 0.0f;
    float brake = 0.0f;
    float steer = 0.0f;
    float clutch = 0.0f;
    float handbrake = 0.0f;
    uint8_t flags = 0;
    uint8_t reserved[3] = {};
};

struct VehicleSeatRequestPacket
{
    PacketHeader header;
    uint32_t requestSerial = 0;
    uint32_t vehicleId = 0;
    int8_t seat = -1;
    uint8_t action = VEHICLE_SEAT_ENTER;
    uint8_t reserved[2] = {};
};

struct VehicleSpawnEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t vehicleId = 0;
    uint32_t chassisNetworkId = 0;
    char templateId[MAX_VEHICLE_TEMPLATE_BYTES] = {};
    float position[3] = {0.0f, 0.0f, 0.0f};
    float orientation[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    int8_t seatCount = 0;
    uint32_t seatOccupants[MAX_VEHICLE_SEATS] = {};
};

struct VehicleDespawnEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t vehicleId = 0;
    uint32_t chassisNetworkId = 0;
};

struct VehicleSeatEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t vehicleId = 0;
    uint32_t actorId = 0;
    int8_t seat = -1;
    uint8_t action = VEHICLE_SEAT_ENTER;
    uint8_t reserved[2] = {};
};

struct VehicleWheelNetState
{
    float spinAngle = 0.0f;
    float spinVelocity = 0.0f;
    float steerAngle = 0.0f;
    float compression = 0.0f;
    uint8_t grounded = 0;
};

struct VehicleNetStateEntry
{
    uint32_t vehicleId = 0;
    uint32_t chassisNetworkId = 0;
    uint8_t engineOn = 0;
    uint8_t mode = 0;
    int8_t gear = 1;
    uint8_t seatCount = 0;
    uint32_t seatOccupants[MAX_VEHICLE_SEATS] = {};
    float engineRpm = 0.0f;
    float speed = 0.0f;
    float throttle = 0.0f;
    float brake = 0.0f;
    float steer = 0.0f;
    float clutch = 0.0f;
    float handbrake = 0.0f;
    uint8_t wheelCount = 0;
    VehicleWheelNetState wheels[MAX_VEHICLE_WHEELS] = {};
};

struct VehicleStatePacket
{
    PacketHeader header;
    uint8_t count = 0;
    uint8_t reserved[3] = {};
    VehicleNetStateEntry entries[MAX_VEHICLE_STATE_ENTRIES] = {};
};


static_assert(sizeof(VehicleInputPacket) <= 64,
              "VehicleInputPacket is too large");
static_assert(sizeof(VehicleSeatRequestPacket) <= 32,
              "VehicleSeatRequestPacket is too large");
static_assert(sizeof(VehicleSpawnEventPacket) <= 128,
              "VehicleSpawnEventPacket is too large");
static_assert(sizeof(VehicleStatePacket) < MAX_GAME_DATAGRAM_BYTES,
              "VehicleStatePacket exceeds the safe datagram limit");

static_assert(sizeof(PhysicalEntitySpawnEventPacket) <= 384,
              "PhysicalEntitySpawnEventPacket is too large");
static_assert(sizeof(PhysicalEntityStatePacket) < MAX_GAME_DATAGRAM_BYTES,
              "PhysicalEntityStatePacket exceeds the safe datagram limit");
static_assert(sizeof(PhysicalEntityCutEventPacket) <= 96,
              "PhysicalEntityCutEventPacket is too large");

#pragma pack(pop)

}  // namespace MimitaNet
