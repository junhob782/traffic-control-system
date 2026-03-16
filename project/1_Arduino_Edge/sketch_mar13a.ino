// [역방향 통신] 1000-Step 초정밀 주행 (보간법) + Ping-Pong 유턴 로직
const int EN_PIN = 7; 
const int P_PIN = 6;  
const int N_PIN = 5;  

// 🚀 [핵심] 1000개의 웨이포인트를 가상으로 쪼개는 마법 (메모리 폭발 방지)
const int NUM_STEPS = 1000;
int currentStep = 0; 
int stepDir = 1; // 1: 전진, -1: 후진(유턴)

// --- 차량별 출발점(Start)과 도착점(End) 설정 ---
// 101번 VIP (남쪽 -> 대전대 정문 36.3400 통과 -> 북쪽 깊숙이)
float c101_startLat = 3620.100, c101_startLng = 12727.500;
float c101_endLat   = 3620.600, c101_endLng   = 12727.880;

// 102번 경찰차 (서쪽 도로를 따라 아주 길게 순찰)
float c102_startLat = 3620.100, c102_startLng = 12727.400;
float c102_endLat   = 3619.600, c102_endLng   = 12726.800;

// 103번 구급차 (동쪽 산기슭 도로를 길게 왕복)
float c103_startLat = 3620.500, c103_startLng = 12727.900;
float c103_endLat   = 3619.800, c103_endLng   = 12727.600;

void setup() {
  Serial.begin(9600);
  pinMode(EN_PIN, OUTPUT); pinMode(P_PIN, OUTPUT); pinMode(N_PIN, OUTPUT);
  digitalWrite(P_PIN, LOW); digitalWrite(N_PIN, LOW); analogWrite(EN_PIN, 0); 
}

void loop() {
  // --- 역방향 통신 (모터 제어) ---
  if (Serial.available() > 0) {
    char cmd = Serial.read(); 
    if (cmd == 'O') { 
      digitalWrite(P_PIN, HIGH); digitalWrite(N_PIN, LOW); analogWrite(EN_PIN, 200); 
      delay(1000); 
      digitalWrite(P_PIN, LOW); digitalWrite(N_PIN, LOW); analogWrite(EN_PIN, 0); 
    }
  }

  // 🚀 [보간법 공식] 현재 스텝(0~1000)이 전체에서 몇 % 지점인지 계산
  float ratio = (float)currentStep / NUM_STEPS;

  // 출발점과 도착점 사이의 현재 좌표를 수학적으로 쪼개서 생성
  float cur101_lat = c101_startLat + (c101_endLat - c101_startLat) * ratio;
  float cur101_lng = c101_startLng + (c101_endLng - c101_startLng) * ratio;

  float cur102_lat = c102_startLat + (c102_endLat - c102_startLat) * ratio;
  float cur102_lng = c102_startLng + (c102_endLng - c102_startLng) * ratio;

  float cur103_lat = c103_startLat + (c103_endLat - c103_startLat) * ratio;
  float cur103_lng = c103_startLng + (c103_endLng - c103_startLng) * ratio;

  // --- NMEA 데이터 발송 (노이즈 완전 제거) ---
  Serial.print("101,$GPGGA,120000.000,");
  Serial.print(cur101_lat, 4); Serial.print(",N,");
  Serial.print(cur101_lng, 4); Serial.println(",E,1,08,1.0,60.0,M,0.0,M,,*00");
  delay(50); // 통신 딜레이 단축

  Serial.print("102,$GPGGA,120000.000,");
  Serial.print(cur102_lat, 4); Serial.print(",N,");
  Serial.print(cur102_lng, 4); Serial.println(",E,1,08,1.0,60.0,M,0.0,M,,*00");
  delay(50);

  Serial.print("103,$GPGGA,120000.000,");
  Serial.print(cur103_lat, 4); Serial.print(",N,");
  Serial.print(cur103_lng, 4); Serial.println(",E,1,08,1.0,60.0,M,0.0,M,,*00");

  // 🚀 핑퐁 유턴 로직 (1000번째 스텝에 도달하면 방향 전환!)
  currentStep += stepDir*3;
  if (currentStep >= NUM_STEPS) {
    stepDir = -1; // 후진(유턴)
  } else if (currentStep <= 0) {
    stepDir = 1;  // 다시 전진
  }

  // 1000번을 쪼갰기 때문에 부드러운 이동을 위해 딜레이를 대폭 줄임 (0.1초마다 갱신)
  delay(100); 
}
