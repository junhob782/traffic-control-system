#ifndef PROTOCOL_H
#define PROTOCOL_H

// 구조체 바이트 정렬을 1바이트 단위로 설정 (윈도우와 리눅스 간 데이터 어긋남 방지)
#pragma pack(push, 1)

/**
 * @brief GPS 데이터 전송용 패킷 구조체
 * 윈도우(클라이언트)에서 조립하여 우분투(서버)로 전송합니다.
 */
struct GpsPacket {
    int car_id;         // 차량 고유 번호 (4 bytes)
    float latitude;     // 위도 (4 bytes)
    float longitude;    // 경도 (4 bytes)

    // 필요 시 아래와 같이 상태 정보를 추가할 수 있습니다.
    // char status;     // 차량 상태 (1: 정상, 2: 정지, 3: 사고 등)
};

#pragma pack(pop)

#endif // PROTOCOL_H