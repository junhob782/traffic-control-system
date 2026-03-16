#pragma once

#pragma pack(push, 1)
struct GpsPacket {
    int car_id;
    float latitude;
    float longitude;
};
#pragma pack(pop)