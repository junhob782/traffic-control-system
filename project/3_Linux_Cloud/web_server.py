from flask import Flask, jsonify, render_template_string
import pymysql
import threading
import socket
import math
from datetime import datetime

app = Flask(__name__)

# ---------------------------------------------------------
# 📡 1. [원격 제어] 9001번 커맨드 센터 (킬-스위치 유지)
# ---------------------------------------------------------
cmd_clients = []

def command_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind(('0.0.0.0', 9001))
    server.listen(5)
    print(">>> [Command Server] 9001번 포트(원격 제어) 대기 중...")
    
    while True:
        conn, addr = server.accept()
        print(f">>> [Command Server] 윈도우 C++ 연결됨: {addr}")
        cmd_clients.append(conn)

threading.Thread(target=command_server, daemon=True).start()

def get_db_connection():
    return pymysql.connect(host='localhost', user='junho', password='Kinh0531!', db='traffic_db', cursorclass=pymysql.cursors.DictCursor)

# ---------------------------------------------------------
# 🧮 2. [핵심] 하버사인(Haversine) 공식 엔진
# ---------------------------------------------------------
def calculate_haversine(lat1, lon1, lat2, lon2):
    R = 6371.0 # 지구의 반지름 (km)
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat / 2)**2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlon / 2)**2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return R * c # 두 좌표 사이의 거리 (km)

car_states = {} # 차량들의 1초 전 상태(위치, 시간, 속도)를 기억하는 주머니

# ---------------------------------------------------------
# 🎨 3. [프론트엔드] 속도계가 추가된 웹 UI
# ---------------------------------------------------------
HTML_PAGE = """
<!DOCTYPE html>
<html>
<head>
    <title>실시간 교통 관제 센터</title>
    <link rel="stylesheet" href="https://unpkg.com/leaflet/dist/leaflet.css" />
    <script src="https://unpkg.com/leaflet/dist/leaflet.js"></script>
    <style>
        body { margin: 0; padding: 0; }
        #map { height: 100vh; width: 100vw; }
        .title-box { position: absolute; top: 10px; left: 50px; z-index: 1000; background: white; padding: 10px 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.5); font-family: sans-serif; font-weight: bold;}
        .custom-icon { background: none; border: none; text-align: center; }
        .control-box { position: absolute; top: 10px; right: 50px; z-index: 1000; background: white; padding: 15px; border-radius: 8px; box-shadow: 0 0 15px rgba(255,0,0,0.6); font-family: sans-serif;}
        .btn-open { background: #e74c3c; color: white; border: none; padding: 12px 24px; font-size: 16px; font-weight: bold; border-radius: 5px; cursor: pointer; transition: 0.2s;}
        .btn-open:hover { background: #c0392b; transform: scale(1.05); }
    </style>
</head>
<body>
    <div class="title-box">🚦 실시간 스마트 캠퍼스 관제 대시보드</div>
    
    <div class="control-box">
        <h3 style="margin: 0 0 10px 0; text-align: center; color: #333;">🎮 원격 모터 제어</h3>
        <button class="btn-open" onclick="manualOpen()">🚨 차단기 강제 개방</button>
    </div>

    <div id="map"></div>

    <script>
        var map = L.map('map').setView([36.3380, 127.4630], 16); 
        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', { attribution: '© OpenStreetMap' }).addTo(map);

        var geofence = L.circle([36.3400, 127.4630], { color: 'red', fillColor: '#f03', fillOpacity: 0.2, radius: 50 }).addTo(map);

        function getCarIcon(carId) {
            let emoji = carId == 101 ? "🚘" : (carId == 102 ? "🚓" : "🚑");
            return L.divIcon({ className: 'custom-icon', html: `<div style='font-size: 35px;'>${emoji}</div>`, iconSize: [40, 40], iconAnchor: [20, 20] });
        }

        var markers = {}; var polylines = {}; var pathData = {};

        function updateMap() {
            fetch('/api/locations')
                .then(res => res.json())
                .then(data => {
                    data.forEach(car => {
                        let latlng = [car.latitude, car.longitude];
                        
                        // 궤적 그리기
                        if (!pathData[car.car_id]) pathData[car.car_id] = [];
                        pathData[car.car_id].push(latlng);
                        if (pathData[car.car_id].length > 20) pathData[car.car_id].shift();

                        if (polylines[car.car_id]) {
                            polylines[car.car_id].setLatLngs(pathData[car.car_id]);
                        } else {
                            let lineColor = car.car_id == 101 ? 'red' : (car.car_id == 102 ? 'blue' : 'green');
                            polylines[car.car_id] = L.polyline(pathData[car.car_id], {color: lineColor, weight: 5, opacity: 0.8, dashArray: '10, 15'}).addTo(map);
                        }

                        // 🚀 [핵심] 마커를 클릭하면 나타나는 말풍선(Popup)에 속도(km/h) 추가!
                        let popupText = `<b>🚗 차량 ID: ${car.car_id}</b><br>
                                         <span style='color:red; font-weight:bold;'>🚀 속도: ${car.speed} km/h</span>`;

                        if(markers[car.car_id]) {
                            markers[car.car_id].setLatLng(latlng);
                            markers[car.car_id].setPopupContent(popupText); // 이동할 때마다 속도 갱신
                        } else {
                            markers[car.car_id] = L.marker(latlng, {icon: getCarIcon(car.car_id)})
                                .addTo(map)
                                .bindPopup(popupText);
                        }
                    });
                });
        }
        setInterval(updateMap, 1000); // 1초마다 화면 갱신

        function manualOpen() {
            fetch('/api/command_open', { method: 'POST' })
                .then(res => res.json())
                .then(data => alert("✅ 관제센터: " + data.msg));
        }
    </script>
</body>
</html>
"""

# ---------------------------------------------------------
# 🛠️ 4. [백엔드 API] DB 데이터 추출 및 실시간 속도 계산
# ---------------------------------------------------------
@app.route('/')
def index():
    return render_template_string(HTML_PAGE)

@app.route('/api/locations')
def locations():
    conn = get_db_connection()
    with conn.cursor() as cursor:
        sql = "SELECT t1.car_id, t1.latitude, t1.longitude, t1.recorded_at FROM gps_log t1 INNER JOIN (SELECT car_id, MAX(recorded_at) as max_time FROM gps_log GROUP BY car_id) t2 ON t1.car_id = t2.car_id AND t1.recorded_at = t2.max_time;"
        cursor.execute(sql)
        result = cursor.fetchall()
    conn.close()

    # 🚀 방금 가져온 데이터에 속도(km/h)를 계산해서 덧붙임
    for row in result:
        car_id = row['car_id']
        curr_lat = float(row['latitude'])
        curr_lng = float(row['longitude'])
        curr_time = row['recorded_at']
        
        speed_kmh = 0.0

        # 만약 이전에 저장해둔 좌표가 있다면? (비교 시작)
        if car_id in car_states:
            prev = car_states[car_id]
            
            # 시간 차이 (초)
            time_diff = (curr_time - prev['time']).total_seconds()

            if time_diff > 0:
                # 하버사인 공식으로 물리적 거리(km) 계산
                distance_km = calculate_haversine(prev['lat'], prev['lng'], curr_lat, curr_lng)
                
                # 속도(km/h) = 거리(km) / 시간(hour)
                speed_kmh = distance_km / (time_diff / 3600.0)
            else:
                speed_kmh = prev['speed'] # 시간이 너무 짧으면 이전 속도 유지

        # 계산이 끝났으니 현재 상태를 주머니에 다시 덮어쓰기 (다음 1초 뒤 계산을 위해)
        car_states[car_id] = {
            'lat': curr_lat,
            'lng': curr_lng,
            'time': curr_time,
            'speed': speed_kmh
        }

        # 프론트엔드로 보낼 JSON에 속도를 소수점 1자리로 예쁘게 잘라서 추가
        row['speed'] = round(speed_kmh, 1)
        row['recorded_at'] = curr_time.strftime('%Y-%m-%d %H:%M:%S')

    return jsonify(result)

@app.route('/api/command_open', methods=['POST'])
def command_open():
    dead_clients = []
    for c in cmd_clients:
        try: c.sendall(b'O')
        except: dead_clients.append(c)
    for c in dead_clients: cmd_clients.remove(c)
    return jsonify({"status": "success", "msg": "차단기 강제 개방 명령이 하드웨어로 전송되었습니다!"})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=9000)
