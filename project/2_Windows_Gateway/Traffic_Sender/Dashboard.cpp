#include <iostream>
#include <windows.h>
#include <mysql.h> 

using namespace std;

int main() {
    // 1. MySQL 초기화
    MYSQL* conn = mysql_init(NULL);
    if (conn == NULL) {
        cerr << "[초기화 실패] MySQL 객체를 생성할 수 없습니다." << endl;
        return 1;
    }

    // 2. 우분투 DB 연결 (포트포워딩된 127.0.0.1:3307 또는 본인 설정에 맞게 사용)
    cout << ">>> 우분투 서버의 DB에 접속을 시도합니다..." << endl;
    if (mysql_real_connect(conn, "127.0.0.1", "junho", "Kinh0531!", "traffic_db", 3307, NULL, 0) == NULL) {
        cerr << "[DB Error] 연결 실패: " << mysql_error(conn) << endl;
        mysql_close(conn);
        return 1;
    }

    // 3. 실시간 대시보드 출력 루프
    while (true) {
        // 🚨 101, 102, 103 모든 차량의 최신 데이터를 가져오는 쿼리
        const char* query =
            "SELECT t1.car_id, t1.latitude, t1.longitude, t1.recorded_at "
            "FROM gps_log t1 "
            "INNER JOIN ( "
            "    SELECT car_id, MAX(recorded_at) as max_time "
            "    FROM gps_log "
            "    GROUP BY car_id "
            ") t2 ON t1.car_id = t2.car_id AND t1.recorded_at = t2.max_time "
            "ORDER BY t1.car_id ASC";

        if (mysql_query(conn, query) == 0) {
            MYSQL_RES* res = mysql_store_result(conn);
            if (res) {
                system("cls"); // 화면 지우기
                cout << "===========================================" << endl;
                cout << "     [ 실시간 통합 교통 관제 대시보드 ]" << endl;
                cout << "===========================================" << endl;
                cout << " 차량ID |   위도    |   경도    |    업데이트" << endl;
                cout << "-------------------------------------------" << endl;

                MYSQL_ROW row;
                int count = 0;

                // 데이터가 있는 만큼(3대) 반복 출력
                while ((row = mysql_fetch_row(res))) {
                    printf("  %-5s | %-9.4f | %-9.4f | %s\n",
                        row[0],
                        atof(row[1]),
                        atof(row[2]),
                        row[3]);
                    count++;
                }

                if (count == 0) {
                    cout << "         DB에 데이터가 없습니다." << endl;
                }

                cout << "===========================================" << endl;
                cout << " (0.5초마다 자동 갱신 중... 종료: Ctrl+C)" << endl;

                mysql_free_result(res);
            }
        }
        else {
            cerr << "쿼리 에러: " << mysql_error(conn) << endl;
        }

        Sleep(500); // 0.5초 대기
    }

    mysql_close(conn);
    return 0;
}