#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <mysql/mysql.h> // MySQL 연동 헤더
#include "Protocol.h"    // 클라이언트와 약속한 구조체 헤더

using namespace std;

#define PORT 8080 

int main() {
    // --- [1] MySQL 초기화 및 연결 ---
    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL) {
        cerr << "[DB Error] MySQL 초기화 실패!" << endl;
        return 1;
    }

    // DB 접속 정보 (사용자: root, 비밀번호 수정 필요, DB명: traffic_db)
    if (mysql_real_connect(conn, "localhost", "junho", "Kinh0531!", "traffic_db", 0, NULL, 0) == NULL) {
        cerr << "[DB Error] 연결 실패: " << mysql_error(conn) << endl;
        mysql_close(conn);
        return 1;
    }
    cout << ">>> MySQL Database Connected Successfully!" << endl;

    // --- [2] TCP 서버 소켓 설정 ---
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    
    // 포트 재사용 옵션 (서버 재시작 시 Bind Error 방지)
    int option = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(PORT);

    if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
        cerr << "[Socket Error] Bind 실패!" << endl;
        return 1;
    }

    if (listen(serv_sock, 5) == -1) {
        cerr << "[Socket Error] Listen 실패!" << endl;
        return 1;
    }

    cout << "=== [ID5] Monitoring Server Started (Port: " << PORT << ") ===" << endl;

    // --- [3] 클라이언트 접속 대기 및 데이터 수신 ---
    clnt_adr_sz = sizeof(clnt_adr);
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
    
    if (clnt_sock == -1) {
        cerr << "[Socket Error] Accept 실패!" << endl;
        return 1;
    }
    cout << ">>> Connected Client IP: " << inet_ntoa(clnt_adr.sin_addr) << endl;

    GpsPacket packet;
    while (true) {
        // 데이터 수신 (구조체 크기만큼)
        int str_len = recv(clnt_sock, (char*)&packet, sizeof(GpsPacket), 0);
        
        if (str_len <= 0) {
            cout << ">>> Client Disconnected." << endl;
            break;
        }

        // 1. 수신 로그 출력
        cout << "[RECV] CarID: " << packet.car_id 
             << " | Lat: " << packet.latitude 
             << " | Lng: " << packet.longitude << endl;

        // 2. MySQL 저장 쿼리 생성
        char query[256];
        sprintf(query, "INSERT INTO gps_log (car_id, latitude, longitude) VALUES (%d, %f, %f)", 
                packet.car_id, packet.latitude, packet.longitude);

        // 3. 쿼리 실행
        if (mysql_query(conn, query)) {
            cerr << "[DB Error] INSERT 실패: " << mysql_error(conn) << endl;
        } else {
            cout << "   └─ [DB] 저장 완료!" << endl;
        }
    }

    // 리소스 해제
    mysql_close(conn);
    close(clnt_sock);
    close(serv_sock);
    
    cout << "=== Server Terminated ===" << endl;
    return 0;
}
