#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

// 第二块 ESP32-S3 连接的 2.4 GHz 开放 Wi-Fi。
const char *WIFI_SSID = "TP-LINK_FA80";
const char *WIFI_PASSWORD = "";

// ==================== 引脚定义 ====================
constexpr int HALL_PINS[] = {4, 5, 6, 7};
constexpr size_t HALL_COUNT = sizeof(HALL_PINS) / sizeof(HALL_PINS[0]);

constexpr int MOTOR_R_EN = 16;
constexpr int MOTOR_L_EN = 15;
constexpr int MOTOR_RPWM = 17;
constexpr int MOTOR_LPWM = 18;

constexpr int MIN_MOTOR_SPEED = 30;
constexpr int MAX_MOTOR_SPEED = 255;
constexpr int DEFAULT_MOTOR_SPEED = 200;
constexpr uint32_t HALL_DEBOUNCE_MS = 30;
constexpr uint32_t DIAGNOSTIC_INTERVAL_MS = 500;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;
constexpr uint32_t CLEAN_STEP_PAUSE_MS = 500;

// 原点为 H3。清理顺序：反向 H2 -> 正向 H4 -> 反向 H3。
constexpr int HOME_HALL_INDEX = 2;
constexpr int CLEAN_TARGETS[] = {1, 3, 2};
constexpr int CLEAN_DIRECTIONS[] = {-1, 1, -1};
constexpr size_t CLEAN_STEP_COUNT =
    sizeof(CLEAN_TARGETS) / sizeof(CLEAN_TARGETS[0]);

WebServer server(80);

int targetHall = -1;
int moveDirection = 0;  // 1=正转，-1=反转，0=停止
int motorSpeed = DEFAULT_MOTOR_SPEED;
bool motorMoving = false;
bool cleaning = false;
size_t cleanStep = 0;
uint32_t nextCleanStepMs = 0;
uint32_t moveStartedMs = 0;
uint32_t hallActiveSinceMs = 0;
uint32_t lastDiagnosticMs = 0;
uint32_t lastWifiRetryMs = 0;
int lastTargetRawState = HIGH;
String motorState = "IDLE";

void runMotor(int speed);
void stopMotor(const char *state = "STOPPED");
void updateCleaning();

bool hallActive(size_t index) {
  return index < HALL_COUNT && digitalRead(HALL_PINS[index]) == LOW;
}

void printHallDiagnostics(const char *event) {
  const int h1 = digitalRead(HALL_PINS[0]);
  const int h2 = digitalRead(HALL_PINS[1]);
  const int h3 = digitalRead(HALL_PINS[2]);
  const int h4 = digitalRead(HALL_PINS[3]);
  const uint32_t elapsed = motorMoving ? millis() - moveStartedMs : 0;

  Serial.printf(
      "[Diag:%s] state=%s moving=%s target=%s elapsed=%lu ms "
      "direction=%s speed=%d | H1(GPIO4)=%s H2(GPIO5)=%s "
      "H3(GPIO6)=%s H4(GPIO7)=%s\n",
      event, motorState.c_str(), motorMoving ? "YES" : "NO",
      targetHall >= 0 ? ("H" + String(targetHall + 1)).c_str() : "NONE",
      static_cast<unsigned long>(elapsed),
      moveDirection > 0 ? "FORWARD" : moveDirection < 0 ? "REVERSE" : "STOPPED",
      motorMoving ? motorSpeed : 0,
      h1 == LOW ? "LOW/TRIGGERED" : "HIGH/OPEN",
      h2 == LOW ? "LOW/TRIGGERED" : "HIGH/OPEN",
      h3 == LOW ? "LOW/TRIGGERED" : "HIGH/OPEN",
      h4 == LOW ? "LOW/TRIGGERED" : "HIGH/OPEN");
}

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
}

void stopMotor(const char *state) {
  ledcWrite(MOTOR_RPWM, 0);
  ledcWrite(MOTOR_LPWM, 0);
  digitalWrite(MOTOR_R_EN, LOW);
  digitalWrite(MOTOR_L_EN, LOW);
  motorMoving = false;
  targetHall = -1;
  moveDirection = 0;
  hallActiveSinceMs = 0;
  motorState = state;
}

void runMotor(int speed) {
  digitalWrite(MOTOR_R_EN, HIGH);
  digitalWrite(MOTOR_L_EN, HIGH);

  if (speed > 0) {
    ledcWrite(MOTOR_RPWM, speed);
    ledcWrite(MOTOR_LPWM, 0);
  } else if (speed < 0) {
    ledcWrite(MOTOR_RPWM, 0);
    ledcWrite(MOTOR_LPWM, -speed);
  } else {
    stopMotor();
  }
}

void moveToHall(int index, int direction) {
  stopMotor();

  Serial.printf("[Command] 网页请求%s定位到 H%d\n",
                direction < 0 ? "反向" : "正向", index + 1);
  printHallDiagnostics("BEFORE_MOVE");

  if (index < 0 || index >= static_cast<int>(HALL_COUNT)) {
    motorState = "INVALID_TARGET";
    return;
  }

  if (hallActive(index)) {
    motorState = "AT_H" + String(index + 1);
    Serial.printf("[Motor] 已经位于 H%d\n", index + 1);
    printHallDiagnostics("ALREADY_AT_TARGET");
    return;
  }

  targetHall = index;
  moveDirection = direction < 0 ? -1 : 1;
  motorMoving = true;
  moveStartedMs = millis();
  hallActiveSinceMs = 0;
  lastDiagnosticMs = 0;
  lastTargetRawState = digitalRead(HALL_PINS[targetHall]);
  motorState = String(moveDirection > 0 ? "FORWARD_TO_H" : "REVERSE_TO_H") +
               String(index + 1);
  runMotor(moveDirection * motorSpeed);
  Serial.printf("[Motor] %s寻找 H%d；无自动超时，将持续运行直到到位或手动停止\n",
                moveDirection > 0 ? "正转" : "反转", index + 1);
  printHallDiagnostics("MOVE_STARTED");
}

void updateMotor() {
  if (!motorMoving || targetHall < 0) return;

  const uint32_t now = millis();
  const int targetRawState = digitalRead(HALL_PINS[targetHall]);

  if (targetRawState != lastTargetRawState) {
    Serial.printf("[Hall] H%d 原始电平变化：%s -> %s\n", targetHall + 1,
                  lastTargetRawState == LOW ? "LOW" : "HIGH",
                  targetRawState == LOW ? "LOW" : "HIGH");
    lastTargetRawState = targetRawState;
    printHallDiagnostics("TARGET_EDGE");
  }

  if (lastDiagnosticMs == 0 || now - lastDiagnosticMs >= DIAGNOSTIC_INTERVAL_MS) {
    lastDiagnosticMs = now;
    printHallDiagnostics("MOVING");
  }

  if (targetRawState != LOW) {
    hallActiveSinceMs = 0;
    return;
  }

  if (hallActiveSinceMs == 0) {
    hallActiveSinceMs = now;
    Serial.printf("[Hall] H%d 开始触发，等待连续 LOW %lu ms 去抖\n",
                  targetHall + 1,
                  static_cast<unsigned long>(HALL_DEBOUNCE_MS));
    return;
  }

  if (now - hallActiveSinceMs >= HALL_DEBOUNCE_MS) {
    const int reachedHall = targetHall;
    const String reachedState = "AT_H" + String(reachedHall + 1);
    stopMotor(reachedState.c_str());
    Serial.printf("[Motor] 到达 H%d，电机已停止\n", reachedHall + 1);
    printHallDiagnostics("TARGET_REACHED");

    if (cleaning) {
      ++cleanStep;
      nextCleanStepMs = now + CLEAN_STEP_PAUSE_MS;
      if (cleanStep >= CLEAN_STEP_COUNT) {
        cleaning = false;
        motorState = "CLEAN_COMPLETE_AT_H3";
        Serial.println("[Clean] 清理流程完成，滚筒已回到原点 H3");
        printHallDiagnostics("CLEAN_COMPLETE");
      } else {
        motorState = "CLEAN_PAUSE";
        Serial.printf("[Clean] 第 %u 步完成，暂停 %lu ms\n",
                      static_cast<unsigned int>(cleanStep),
                      static_cast<unsigned long>(CLEAN_STEP_PAUSE_MS));
      }
    }
  }
}

void updateCleaning() {
  if (!cleaning || motorMoving || cleanStep >= CLEAN_STEP_COUNT) return;
  if (millis() < nextCleanStepMs) return;

  const int target = CLEAN_TARGETS[cleanStep];
  const int direction = CLEAN_DIRECTIONS[cleanStep];
  Serial.printf("[Clean] 开始第 %u/%u 步：%s定位到 H%d\n",
                static_cast<unsigned int>(cleanStep + 1),
                static_cast<unsigned int>(CLEAN_STEP_COUNT),
                direction > 0 ? "正向" : "反向", target + 1);

  if (hallActive(target)) {
    Serial.printf("[Clean] H%d 当前已触发，直接进入下一步\n", target + 1);
    ++cleanStep;
    nextCleanStepMs = millis() + CLEAN_STEP_PAUSE_MS;
    if (cleanStep >= CLEAN_STEP_COUNT) {
      cleaning = false;
      motorState = "CLEAN_COMPLETE_AT_H3";
      Serial.println("[Clean] 清理流程完成，滚筒已回到原点 H3");
    }
    return;
  }

  moveToHall(target, direction);
}

void handleStatus() {
  char json[384];
  snprintf(json, sizeof(json),
           "{\"state\":\"%s\",\"moving\":%s,\"target\":%d,"
           "\"direction\":\"%s\",\"speed\":%d,"
           "\"cleaning\":%s,\"cleanStep\":%u,\"home\":3,"
           "\"halls\":[%s,%s,%s,%s],\"rssi\":%ld}",
           motorState.c_str(), motorMoving ? "true" : "false",
           targetHall >= 0 ? targetHall + 1 : 0,
           moveDirection > 0 ? "forward" : moveDirection < 0 ? "reverse" : "stopped",
           motorSpeed,
           cleaning ? "true" : "false",
           static_cast<unsigned int>(cleanStep),
           hallActive(0) ? "true" : "false",
           hallActive(1) ? "true" : "false",
           hallActive(2) ? "true" : "false",
           hallActive(3) ? "true" : "false",
           static_cast<long>(WiFi.RSSI()));
  addCorsHeaders();
  server.send(200, "application/json", json);
}

void handleMove() {
  const String target = server.arg("target");
  if (target.length() != 2 || (target[0] != 'H' && target[0] != 'h') ||
      target[1] < '1' || target[1] > '4') {
    addCorsHeaders();
    server.send(400, "application/json",
                "{\"ok\":false,\"error\":\"target must be H1-H4\"}");
    return;
  }

  const String direction = server.arg("direction");
  if (direction.length() > 0 && direction != "forward" && direction != "reverse") {
    addCorsHeaders();
    server.send(400, "application/json",
                "{\"ok\":false,\"error\":\"direction must be forward or reverse\"}");
    return;
  }

  cleaning = false;
  cleanStep = 0;
  moveToHall(target[1] - '1', direction == "reverse" ? -1 : 1);
  addCorsHeaders();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleClean() {
  if (motorMoving || cleaning) {
    addCorsHeaders();
    server.send(409, "application/json",
                "{\"ok\":false,\"error\":\"motor is busy\"}");
    return;
  }

  if (!hallActive(HOME_HALL_INDEX)) {
    motorState = "CLEAN_REQUIRES_H3";
    Serial.println("[Clean] 无法开始：初始位置必须是原点 H3");
    printHallDiagnostics("CLEAN_REJECTED_NOT_AT_H3");
    addCorsHeaders();
    server.send(409, "application/json",
                "{\"ok\":false,\"error\":\"initial position must be H3\"}");
    return;
  }

  cleaning = true;
  cleanStep = 0;
  nextCleanStepMs = millis() + CLEAN_STEP_PAUSE_MS;
  motorState = "CLEAN_STARTING_AT_H3";
  Serial.println("[Clean] 一键清理开始：H3 -> 反向H2 -> 正向H4 -> 反向H3");
  printHallDiagnostics("CLEAN_STARTED");
  addCorsHeaders();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleSpeed() {
  const String value = server.arg("value");
  bool validNumber = value.length() > 0;
  for (size_t index = 0; index < value.length(); ++index) {
    if (!isDigit(value[index])) validNumber = false;
  }

  const int requestedSpeed = validNumber ? value.toInt() : -1;
  if (requestedSpeed < MIN_MOTOR_SPEED || requestedSpeed > MAX_MOTOR_SPEED) {
    addCorsHeaders();
    server.send(400, "application/json",
                "{\"ok\":false,\"error\":\"speed must be 30-255\"}");
    return;
  }

  motorSpeed = requestedSpeed;
  if (motorMoving && moveDirection != 0) {
    runMotor(moveDirection * motorSpeed);
  }
  Serial.printf("[Speed] 网页设置 PWM=%d%s\n", motorSpeed,
                motorMoving ? "，运行中立即生效" : "，下次定位生效");
  printHallDiagnostics("SPEED_CHANGED");

  char response[48];
  snprintf(response, sizeof(response), "{\"ok\":true,\"speed\":%d}", motorSpeed);
  addCorsHeaders();
  server.send(200, "application/json", response);
}

void initWebServer() {
  server.on("/", HTTP_GET, []() {
    addCorsHeaders();
    server.send(200, "text/plain", "CatLitterTray motor controller");
  });
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/move", HTTP_GET, handleMove);
  server.on("/speed", HTTP_GET, handleSpeed);
  server.on("/clean", HTTP_GET, handleClean);
  server.on("/stop", HTTP_GET, []() {
    printHallDiagnostics("BEFORE_EMERGENCY_STOP");
    stopMotor("EMERGENCY_STOP");
    cleaning = false;
    cleanStep = 0;
    Serial.println("[Safety] 收到网页紧急停止指令");
    printHallDiagnostics("AFTER_EMERGENCY_STOP");
    addCorsHeaders();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.onNotFound([]() {
    addCorsHeaders();
    server.send(404, "text/plain", "not found");
  });
  server.begin();
  Serial.println("[Web] 电机控制服务已启动，端口 80");
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("[Wi-Fi] 正在连接 %s", WIFI_SSID);
  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[Wi-Fi] 已连接");
    Serial.print("Motor ESP32-S3 IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Dashboard motor address: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[Wi-Fi] 连接失败，将在后台重试");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[CatLitterTray] MOTOR CONTROLLER START");

  for (size_t index = 0; index < HALL_COUNT; ++index) {
    pinMode(HALL_PINS[index], INPUT_PULLUP);
  }
  pinMode(MOTOR_R_EN, OUTPUT);
  pinMode(MOTOR_L_EN, OUTPUT);
  ledcAttach(MOTOR_RPWM, 5000, 8);
  ledcAttach(MOTOR_LPWM, 5000, 8);
  stopMotor("IDLE");
  printHallDiagnostics("BOOT");

  // 上电不自动转动，必须由网页明确选择 H1-H4。
  connectWifi();
  initWebServer();
}

void loop() {
  server.handleClient();
  updateMotor();
  updateCleaning();

  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED &&
      now - lastWifiRetryMs >= WIFI_RETRY_INTERVAL_MS) {
    lastWifiRetryMs = now;
    WiFi.reconnect();
  }
  delay(1);
}
