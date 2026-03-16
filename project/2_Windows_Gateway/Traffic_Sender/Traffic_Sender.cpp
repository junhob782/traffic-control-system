#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <string>
#include <winsock2.h>
#include <windows.h>
#include "Protocol.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// ⚠️ 본인의 아두이노 포트 및 우분투 IP
const char* PORT_NAME = "\\\\.\\COM7";
const char* SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 8080; // 데이터 전송 포트
const int CMD_PORT = 9001;    // 🚀 원격 제어 수신 포트

int main() {
    cout << "=== [Traffic Control] Data Sender & Remote Command Center ===" << endl;

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 1. 기존 데이터 전송용 소켓 (8080) 연결
    SOCKET sock = socket(PF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    servAddr.sin_port = htons(SERVER_PORT);

    if (connect(sock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR) {
        cerr << "[Socket Error] 우분투 서버(8080)에 연결할 수 없습니다." << endl;
        return 1;
    }
    cout << ">>> 📡 우분투 데이터 서버(8080) 연결 성공!" << endl;

    // ---------------------------------------------------------
    // 🚀 2. [새로운 마법] 9001번 커맨드 센터(원격 제어) 연결
    // ---------------------------------------------------------
    SOCKET cmdSock = socket(PF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN cmdAddr;
    memset(&cmdAddr, 0, sizeof(cmdAddr));
    cmdAddr.sin_family = AF_INET;
    cmdAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    cmdAddr.sin_port = htons(CMD_PORT);

    if (connect(cmdSock, (SOCKADDR*)&cmdAddr, sizeof(cmdAddr)) != SOCKET_ERROR) {
        cout << ">>> 🎮 웹 원격 제어 커맨드 센터(9001) 연결 성공!" << endl;
    }
    else {
        cerr << "[Warning] 커맨드 센터(9001) 연결 실패. 웹 제어는 불가능합니다." << endl;
    }

    // 🚨 수신 소켓을 넌블로킹(Non-blocking)으로 설정! (데이터를 기다리다 프로그램이 멈추는 것 방지)
    u_long mode = 1;
    ioctlsocket(cmdSock, FIONBIO, &mode);
    // ---------------------------------------------------------

    // 3. 아두이노 시리얼 통신 연결
    HANDLE hComm = CreateFileA(PORT_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hComm, &dcbSerialParams);
    dcbSerialParams.BaudRate = CBR_9600;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    SetCommState(hComm, &dcbSerialParams);

    cout << ">>> 🔌 아두이노 시리얼 포트 연결 성공!" << endl;

    char buffer[256];
    DWORD bytesRead;
    GpsPacket packet;
    string rx_buffer = "";
    bool isGateOpened = false;

    while (true) {
        // ---------------------------------------------------------
        // 🚀 4. 웹에서 '강제 개방' 버튼을 눌렀는지 1초마다 확인!
        // ---------------------------------------------------------
        char cmd_buf[10];
        // recv()가 넌블로킹이므로 명령이 없으면 그냥 지나갑니다.
        int n = recv(cmdSock, cmd_buf, sizeof(cmd_buf) - 1, 0);
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                if (cmd_buf[i] == 'O') {
                    cout << "\n=======================================================" << endl;
                    cout << " [🎮 WEB CONTROL] 관제센터 원격 강제 개방 명령 수신! 징~!" << endl;
                    cout << "=======================================================\n" << endl;

                    // 아두이노로 'O' 쏴서 모터 구동!
                    char command = 'O';
                    DWORD bytesWritten;
                    WriteFile(hComm, &command, 1, &bytesWritten, NULL);
                }
            }
        }
        // ---------------------------------------------------------

        // 5. 아두이노 NMEA 수신 및 우분투 전송 (기존과 동일)
        if (ReadFile(hComm, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            rx_buffer += buffer;

            size_t pos;
            while ((pos = rx_buffer.find('\n')) != string::npos) {
                string line = rx_buffer.substr(0, pos);
                rx_buffer.erase(0, pos + 1);

                int car_id; char header[10]; float time_val, nmea_lat, nmea_lng; char ns, ew;
                if (sscanf(line.c_str(), "%d,%[^,],%f,%f,%c,%f,%c", &car_id, header, &time_val, &nmea_lat, &ns, &nmea_lng, &ew) >= 7) {
                    if (strcmp(header, "$GPGGA") == 0) {
                        int lat_deg = (int)(nmea_lat / 100); float lat_min = nmea_lat - (lat_deg * 100);
                        packet.latitude = lat_deg + (lat_min / 60.0f);
                        int lng_deg = (int)(nmea_lng / 100); float lng_min = nmea_lng - (lng_deg * 100);
                        packet.longitude = lng_deg + (lng_min / 60.0f);
                        packet.car_id = car_id;

                        send(sock, (char*)&packet, sizeof(GpsPacket), 0);

                        // 자동 지오펜싱(101번 진입 시 개방) 로직은 그대로 살려둡니다.
                        if (packet.car_id == 101 && packet.latitude > 36.3400 && !isGateOpened) {
                            cout << " [🚨 AUTO SYSTEM] 101번 진입 자동 개방!" << endl;
                            char command = 'O'; DWORD bytesWritten;
                            WriteFile(hComm, &command, 1, &bytesWritten, NULL);
                            isGateOpened = true;
                        }
                        if (packet.car_id == 101 && packet.latitude < 36.3380) isGateOpened = false;
                    }
                }
            }
        }
    }

    CloseHandle(hComm);
    closesocket(sock);
    closesocket(cmdSock);
    WSACleanup();
    return 0;
}