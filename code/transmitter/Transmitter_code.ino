#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>

const char* SSID = "ROBOT_AP";
const char* PASS = "12345678";
const char* HOST_IP = "192.168.4.1";
const uint16_t PORT = 4210;

#define MPU_ADDR_LOW 0x68
#define MPU_ADDR_HIGH 0x69
uint8_t MPU_ADDR = MPU_ADDR_LOW;

#define REG_PWR_MGMT_1 0x6B
#define REG_SMPLRT_DIV 0x19
#define REG_CONFIG 0x1A
#define REG_ACCEL_CONFIG 0x1C
#define REG_ACCEL_XOUT_H 0x3B
#define REG_WHO_AM_I 0x75

const float ACC_SENS = 16384.0f;
float ax_off=0, ay_off=0, az_off=0;

WiFiUDP udp;

bool i2cW(uint8_t d,uint8_t r,uint8_t v){ Wire.beginTransmission(d); Wire.write(r); Wire.write(v); return Wire.endTransmission()==0; }
bool i2cR(uint8_t d,uint8_t r,uint8_t* b,uint8_t n){ Wire.beginTransmission(d); Wire.write(r); if(Wire.endTransmission(false)!=0) return false; uint8_t m=Wire.requestFrom((int)d,(int)n); if(m!=n) return false; for(uint8_t i=0;i<n;i++) b[i]=Wire.read(); return true; }
bool det(){ for(uint8_t a:{MPU_ADDR_LOW,MPU_ADDR_HIGH}){ uint8_t w=0; if(i2cR(a,REG_WHO_AM_I,&w,1)&&w==0x68){ MPU_ADDR=a; return true; } } return false; }
bool initIMU(){ if(!i2cW(MPU_ADDR,REG_PWR_MGMT_1,0x00)) return false; delay(30); return i2cW(MPU_ADDR,REG_SMPLRT_DIV,0x07)&&i2cW(MPU_ADDR,REG_CONFIG,0x03)&&i2cW(MPU_ADDR,REG_ACCEL_CONFIG,0x00); }
bool readAcc(int16_t& ax,int16_t& ay,int16_t& az){ uint8_t b[6]; if(!i2cR(MPU_ADDR,REG_ACCEL_XOUT_H,b,6)) return false; ax=(int16_t)((b[0]<<8)|b[1]); ay=(int16_t)((b[2]<<8)|b[3]); az=(int16_t)((b[4]<<8)|b[5]); return true; }

void setup(){
  Serial.begin(115200);
  Wire.begin(D2,D1);
  det(); initIMU();
  const int N=300; long sax=0,say=0,saz=0;
  for(int i=0;i<N;i++){ int16_t ax,ay,az; if(readAcc(ax,ay,az)){ sax+=ax; say+=ay; saz+=az; } delay(2); }
  ax_off=(float)sax/N; ay_off=(float)say/N; az_off=(float)saz/N-ACC_SENS;

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID,PASS);
  while (WiFi.status()!=WL_CONNECTED) { delay(200); }
  udp.begin(PORT);
  Serial.println("Glove sender ready");
}

void loop(){
  int16_t axr,ayr,azr;
  if(!readAcc(axr,ayr,azr)){ delay(5); return; }
  float ax=(axr-ax_off)/ACC_SENS;
  float ay=(ayr-ay_off)/ACC_SENS;
  float az=(azr-az_off)/ACC_SENS;
  float roll = atan2f(ay,az)*180.0f/PI;
  float pitch= atan2f(-ax, sqrtf(ay*ay+az*az))*180.0f/PI;

  char buf[64];
  int n = snprintf(buf,sizeof(buf),"%.2f,%.2f",roll,pitch);
  udp.beginPacket(HOST_IP,PORT);
  udp.write((uint8_t*)buf,n);
  udp.endPacket();

  static unsigned long t0=0; unsigned long now=millis();
  if(now-t0>500){ Serial.printf("TX: %s\n",buf); t0=now; }
  delay(50);
}