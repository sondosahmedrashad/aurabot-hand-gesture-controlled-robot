#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

const char* SSID = "ROBOT_AP";
const char* PASS = "12345678";
const uint16_t PORT = 4210;

const int IN1 = D5;
const int IN2 = D6;
const int IN3 = D7;
const int IN4 = D8;
const int ENA = D3;
const int ENB = D4;

const int PWM_MAX = 1023, BASE_DUTY = 650, MAX_DUTY = 950;
const float DEAD_DEG = 6.0f, MAX_DEG = 25.0f;
const unsigned long STILL_TIMEOUT_MS = 800;
const float STILL_ANGLE_DELTA = 1.5f;

WiFiUDP udp;
float lastRoll = 0, lastPitch = 0;
unsigned long lastMoveMs = 0;

inline int iconstrain(int v, int lo, int hi){ return v < lo ? lo : (v > hi ? hi : v); }
inline float fconstrain(float v, float lo, float hi){ return v < lo ? lo : (v > hi ? hi : v); }

void fwdA(int d){ analogWrite(IN1, d); analogWrite(IN2, 0); }
void bwdA(int d){ analogWrite(IN1, 0); analogWrite(IN2, d); }
void stpA(){ analogWrite(IN1, 0); analogWrite(IN2, 0); }

void fwdB(int d){ analogWrite(IN3, d); analogWrite(IN4, 0); }
void bwdB(int d){ analogWrite(IN3, 0); analogWrite(IN4, d); }
void stpB(){ analogWrite(IN3, 0); analogWrite(IN4, 0); }

int tiltToDuty(float deg){
  float a = fabs(deg);
  if (a <= DEAD_DEG) return 0;
  float k = fconstrain((a - DEAD_DEG) / (MAX_DEG - DEAD_DEG), 0.0f, 1.0f);
  int duty = BASE_DUTY + (int)((MAX_DUTY - BASE_DUTY) * k);
  return iconstrain(duty, 0, MAX_DUTY);
}

void setup(){
  Serial.begin(115200);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID, PASS, 6, false, 2);
  IPAddress ip = WiFi.softAPIP();
  udp.begin(PORT);
  lastMoveMs = millis();
  Serial.printf("Robot AP %s IP %s\n", SSID, ip.toString().c_str());
}

void loop(){
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    char buf[64] = {0};
    int n = udp.read(buf, sizeof(buf) - 1);
    float roll = 0, pitch = 0;
    if (n > 0 && sscanf(buf, "%f,%f", &roll, &pitch) == 2) {
      bool moving = (fabs(roll - lastRoll) > STILL_ANGLE_DELTA || fabs(pitch - lastPitch) > STILL_ANGLE_DELTA);
      if (moving) lastMoveMs = millis();
      lastRoll = roll; lastPitch = pitch;

      int a_cmd = 0, b_cmd = 0;
      if (millis() - lastMoveMs > STILL_TIMEOUT_MS) {
        a_cmd = 0; b_cmd = 0;
      } else {
        int forward = tiltToDuty(pitch);
        int turn = tiltToDuty(roll) / 2;
        if (pitch > DEAD_DEG) {
          a_cmd = iconstrain(forward + (roll > 0 ? -turn : +turn), 0, MAX_DUTY);
          b_cmd = iconstrain(forward + (roll > 0 ? +turn : -turn), 0, MAX_DUTY);
        } else if (pitch < -DEAD_DEG) {
          int back = forward;
          a_cmd = -iconstrain(back + (roll > 0 ? -turn : +turn), 0, MAX_DUTY);
          b_cmd = -iconstrain(back + (roll > 0 ? +turn : -turn), 0, MAX_DUTY);
        }
      }

      if (a_cmd > 0) { analogWrite(ENA, a_cmd); fwdA(a_cmd); }
      else if (a_cmd < 0) { analogWrite(ENA, -a_cmd); bwdA(-a_cmd); }
      else { analogWrite(ENA, 0); stpA(); }

      if (b_cmd > 0) { analogWrite(ENB, b_cmd); fwdB(b_cmd); }
      else if (b_cmd < 0) { analogWrite(ENB, -b_cmd); bwdB(-a_cmd); }
      else { analogWrite(ENB, 0); stpB(); }

      static unsigned long t0 = 0; unsigned long now = millis();
      if (now - t0 >= 300) {
        Serial.printf("r=%.1f p=%.1f A=%d B=%d%s\n", roll, pitch, a_cmd, b_cmd, (millis() - lastMoveMs > STILL_TIMEOUT_MS) ? " idle" : "");
        t0 = now;
      }
    }
  }
}