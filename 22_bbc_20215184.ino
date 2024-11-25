#include <Servo.h>

// Arduino pin assignment
#define PIN_LED   9
#define PIN_SERVO 10
#define PIN_IR    A0

// Event interval parameters
#define _INTERVAL_DIST    100  // distance sensor interval (unit: ms)
#define _INTERVAL_SERVO   50   // servo interval (unit: ms)
#define _INTERVAL_SERIAL  500  // serial interval (unit: ms)

// EMA filter configuration for the IR distance sensor
#define _EMA_ALPHA 0.5    // EMA weight of new sample (range: 0 to 1)

// Servo adjustment - Set _DUTY_MAX, _NEU, _MIN with your own numbers
#define _DUTY_MAX 2300    // 서보 최대 값 (최소 각도, 역수 적용)
#define _DUTY_NEU 2000  // 서보 중립 값
#define _DUTY_MIN 300     // 서보 최소 값 (최대 각도, 역수 적용)

#define _SERVO_ANGLE_DIFF 180  // Replace with |D - E| degree
#define _SERVO_SPEED       200  // servo speed

#define _BANGBANG_RANGE    300  // Bang-bang 제어 범위

// Target Distance
#define _DIST_TARGET    140 // Center of the rail (unit:mm)

// global variables

Servo myservo;

float dist_ema;     // unit: mm

int duty_change_per_interval; // maximum duty difference per interval
int duty_target;    // Target duty
int duty_current;   // Current duty

unsigned long last_sampling_time_dist;   // unit: msec
unsigned long last_sampling_time_servo;  // unit: msec
unsigned long last_sampling_time_serial; // unit: msec

bool event_dist, event_servo, event_serial; // event triggered?

void setup()
{
  // GPIO 초기화
  pinMode(PIN_LED, OUTPUT);
  myservo.attach(PIN_SERVO);

  duty_target = duty_current = _DUTY_NEU;
  myservo.writeMicroseconds(duty_current);

  // 시리얼 포트 초기화
  Serial.begin(9600);  
    
  // 각속도를 duty change per interval로 변환
  duty_change_per_interval = 
    (float)(_DUTY_MAX - _DUTY_MIN) * ((float)_SERVO_SPEED / _SERVO_ANGLE_DIFF) * (_INTERVAL_SERVO / 1000.0); 
}

void loop()
{
  unsigned long time_curr = millis();
  
  // 이벤트 트리거
  if (time_curr >= (last_sampling_time_dist + _INTERVAL_DIST)) {
        last_sampling_time_dist += _INTERVAL_DIST;
        event_dist = true;
  }
  if (time_curr >= (last_sampling_time_servo + _INTERVAL_SERVO)) {
        last_sampling_time_servo += _INTERVAL_SERVO;
        event_servo = true;
  }
  if (time_curr >= (last_sampling_time_serial + _INTERVAL_SERIAL)) {
        last_sampling_time_serial += _INTERVAL_SERIAL;
        event_serial = true;
  }
    
  if (event_dist) {
    float dist_filtered; // unit: mm
    event_dist = false;

    // 센서 값 읽기
    dist_filtered = volt_to_distance(ir_sensor_filtered(10, 0.5, 0));
    dist_ema = _EMA_ALPHA * dist_filtered + (1.0 - _EMA_ALPHA) * dist_ema;

    // Bang-bang 제어
    if (dist_ema > _DIST_TARGET) {
      duty_target = _DUTY_NEU - _BANGBANG_RANGE;
      digitalWrite(PIN_LED, 0);
    } else if (dist_ema < _DIST_TARGET) {
      duty_target = _DUTY_NEU + _BANGBANG_RANGE;
      digitalWrite(PIN_LED, 1);
    }
  }
  
  if (event_servo) {
    event_servo = false;
     
    // 서보 이동 조정
    if (duty_target > duty_current) {
      duty_current += duty_change_per_interval;
      if (duty_current > duty_target)
          duty_current = duty_target;
    } else if (duty_target < duty_current) {
      duty_current -= duty_change_per_interval;
      if (duty_current < duty_target)
        duty_current = duty_target;
    }
    
    // 서보 범위 제한
    if (duty_current < _DUTY_MIN)
      duty_current = _DUTY_MIN;
    else if (duty_current > _DUTY_MAX) 
      duty_current = _DUTY_MAX;

    // 서보 위치 업데이트
    myservo.writeMicroseconds(duty_current);
  }
  
  if (event_serial) {
    event_serial = false;
    
    // 시리얼 출력
    Serial.print("TARGET:"); Serial.print(_DIST_TARGET);
    Serial.print(", DIST:"); Serial.print(dist_ema);
    Serial.print(", duty_target:"); Serial.print(duty_target);
    Serial.print(", duty_current:"); Serial.println(duty_current);
  }
}

float volt_to_distance(int a_value)
{
    float voltage = a_value * (5.0 / 1023.0); // 아날로그 값을 전압으로 변환
    float distance = 213.675 * pow(voltage, -1.15); // 수정된 거리 계산
    return distance;
}

int compare(const void *a, const void *b) {
  return (*(unsigned int *)a - *(unsigned int *)b);
}

unsigned int ir_sensor_filtered(unsigned int n, float position, int verbose)
{
  unsigned int *ir_val, ret_val;
 
  if ((n == 0) || (n > 100) || (position < 0.0) || (position > 1))
    return 0;
    
  ir_val = (unsigned int *)malloc(sizeof(unsigned int) * n);
  if (ir_val == NULL)
    return 0;

  for (int i = 0; i < n; i++) {
    ir_val[i] = analogRead(PIN_IR);
  }

  qsort(ir_val, n, sizeof(unsigned int), compare);
  ret_val = ir_val[(unsigned int)(n * position)];
  free(ir_val);

  return ret_val;
}
