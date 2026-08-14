#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "driver/spi_slave.h"
#include "esp_heap_caps.h"

// Fill in the same 2.4 GHz Wi-Fi used by the dashboard computer.
const char *WIFI_SSID = "TP-LINK_FA80";
const char *WIFI_PASSWORD = "";

// ESP32-CAM -> ESP32-S3. GPIO 11/12/13 are reserved for camera transport.
constexpr int PIN_CAMERA_MOSI = 11;
constexpr int PIN_CAMERA_SCLK = 12;
constexpr int PIN_CAMERA_CS = 13;

constexpr int PIN_MOTOR_RPWM = 40;
constexpr int PIN_MOTOR_LPWM = 41;
constexpr int PIN_HALL_1 = 4;
constexpr int PIN_HALL_2 = 5;
constexpr int PIN_HALL_3 = 6;
constexpr int PIN_HALL_4 = 7;
constexpr int PIN_IR_UP = 17;
constexpr int PIN_IR_DOWN = 18;
constexpr int PIN_BIN_FULL = 21;

constexpr uint16_t FRAME_WIDTH = 160;
constexpr uint16_t FRAME_HEIGHT = 120;
constexpr size_t MAX_FRAME_BYTES = 64 * 1024;
constexpr size_t BLOCK_BYTES = 1024;
constexpr size_t HEADER_BYTES = 6;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;

WebServer server(80);

uint8_t *spiFrame = nullptr;
uint8_t *webFrame = nullptr;
uint8_t *dmaBlock = nullptr;
uint8_t *dmaHeader = nullptr;
SemaphoreHandle_t frameMutex = nullptr;

volatile bool frameReady = false;
volatile size_t webFrameSize = 0;
volatile uint32_t receivedFrameCount = 0;
volatile uint32_t droppedFrameCount = 0;

String currentStatus = "空闲";
uint32_t lastWifiRetryMs = 0;

void halt(const char *message) {
  Serial.println(message);
  while (true) delay(1000);
}

void stopMotor() {
  digitalWrite(PIN_MOTOR_RPWM, LOW);
  digitalWrite(PIN_MOTOR_LPWM, LOW);
  currentStatus = "空闲";
}

void initPins() {
  pinMode(PIN_HALL_1, INPUT_PULLUP);
  pinMode(PIN_HALL_2, INPUT_PULLUP);
  pinMode(PIN_HALL_3, INPUT_PULLUP);
  pinMode(PIN_HALL_4, INPUT_PULLUP);
  pinMode(PIN_IR_UP, INPUT_PULLUP);
  pinMode(PIN_IR_DOWN, INPUT_PULLUP);
  pinMode(PIN_BIN_FULL, INPUT_PULLUP);
  pinMode(PIN_MOTOR_RPWM, OUTPUT);
  pinMode(PIN_MOTOR_LPWM, OUTPUT);
  stopMotor();
}

uint8_t *allocateFrameBuffer() {
  uint8_t *buffer = static_cast<uint8_t *>(
      heap_caps_malloc(MAX_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!buffer) {
    buffer = static_cast<uint8_t *>(
        heap_caps_malloc(MAX_FRAME_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  return buffer;
}

void initBuffers() {
  spiFrame = allocateFrameBuffer();
  webFrame = allocateFrameBuffer();
  dmaBlock = static_cast<uint8_t *>(
      heap_caps_malloc(BLOCK_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  dmaHeader = static_cast<uint8_t *>(
      heap_caps_malloc(HEADER_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  frameMutex = xSemaphoreCreateMutex();

  if (!spiFrame || !webFrame || !dmaBlock || !dmaHeader || !frameMutex) {
    halt("CAMERA BUFFER ALLOCATION FAILED");
  }
  Serial.println("CAMERA BUFFERS READY");
}

void initCameraSPI() {
  spi_bus_config_t busConfig = {};
  busConfig.mosi_io_num = PIN_CAMERA_MOSI;
  busConfig.miso_io_num = -1;
  busConfig.sclk_io_num = PIN_CAMERA_SCLK;
  busConfig.quadwp_io_num = -1;
  busConfig.quadhd_io_num = -1;
  busConfig.max_transfer_sz = BLOCK_BYTES;

  spi_slave_interface_config_t slaveConfig = {};
  slaveConfig.spics_io_num = PIN_CAMERA_CS;
  slaveConfig.queue_size = 1;
  slaveConfig.mode = 1;

  const esp_err_t result = spi_slave_initialize(
      SPI2_HOST, &busConfig, &slaveConfig, SPI_DMA_CH_AUTO);
  if (result != ESP_OK) {
    Serial.printf("CAMERA SPI INIT FAILED: %d\n", result);
    halt("CAMERA SPI STOPPED");
  }
  Serial.println("CAMERA SPI READY");
}

bool receiveSPITransaction(uint8_t *destination, size_t length) {
  spi_slave_transaction_t transaction = {};
  transaction.length = length * 8;
  transaction.rx_buffer = destination;
  if (spi_slave_transmit(SPI2_HOST, &transaction, portMAX_DELAY) != ESP_OK) {
    return false;
  }
  return transaction.trans_len / 8 == length;
}

void receiveCameraFrame() {
  if (!receiveSPITransaction(dmaHeader, HEADER_BYTES)) return;
  if (dmaHeader[0] != 0xAA || dmaHeader[1] != 0x55) {
    Serial.println("CAMERA BAD HEADER");
    return;
  }

  uint32_t frameSize = 0;
  memcpy(&frameSize, dmaHeader + 2, sizeof(frameSize));
  if (frameSize < 4 || frameSize > MAX_FRAME_BYTES) {
    Serial.printf("CAMERA SIZE ERROR: %lu\n",
                  static_cast<unsigned long>(frameSize));
    return;
  }

  size_t received = 0;
  while (received < frameSize) {
    const size_t remaining = static_cast<size_t>(frameSize) - received;
    const size_t blockLength = remaining < BLOCK_BYTES ? remaining : BLOCK_BYTES;
    if (!receiveSPITransaction(dmaBlock, blockLength)) return;
    memcpy(spiFrame + received, dmaBlock, blockLength);
    received += blockLength;
  }

  if (spiFrame[0] != 0xFF || spiFrame[1] != 0xD8 ||
      spiFrame[frameSize - 2] != 0xFF || spiFrame[frameSize - 1] != 0xD9) {
    Serial.println("CAMERA JPEG ERROR");
    ++droppedFrameCount;
    return;
  }

  if (xSemaphoreTake(frameMutex, pdMS_TO_TICKS(250)) != pdTRUE) {
    ++droppedFrameCount;
    return;
  }
  if (frameReady) ++droppedFrameCount;
  memcpy(webFrame, spiFrame, frameSize);
  webFrameSize = frameSize;
  frameReady = true;
  ++receivedFrameCount;
  xSemaphoreGive(frameMutex);
}

void cameraTask(void *parameter) {
  (void)parameter;
  while (true) receiveCameraFrame();
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("Connecting to Wi-Fi: %s", WIFI_SSID);
  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WI-FI CONNECTED");
    Serial.print("ESP32-S3 IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Dashboard device address: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WI-FI NOT CONNECTED; CAMERA RECEIVE CONTINUES");
  }
}

void addNoCacheHeaders() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
}

void handleFrame() {
  if (!frameReady) {
    server.send(503, "text/plain", "camera frame not ready");
    return;
  }
  if (xSemaphoreTake(frameMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
    server.send(503, "text/plain", "camera frame busy");
    return;
  }

  addNoCacheHeaders();
  const size_t frameSize = webFrameSize;
  server.setContentLength(frameSize);
  server.send(200, "image/jpeg", "");
  server.sendContent(reinterpret_cast<const char *>(webFrame), frameSize);
  frameReady = false;
  xSemaphoreGive(frameMutex);
}

void handleStatus() {
  char json[512];
  snprintf(
      json, sizeof(json),
      "{\"type\":\"telemetry\",\"status\":\"%s\","
      "\"camera\":{\"width\":%u,\"height\":%u,\"frames\":%lu,"
      "\"dropped\":%lu,\"bytes\":%u,\"format\":\"jpeg\"},"
      "\"halls\":[%s,%s,%s,%s],"
      "\"safety\":{\"irUp\":%s,\"irDown\":%s},\"binFull\":%s}",
      currentStatus.c_str(), FRAME_WIDTH, FRAME_HEIGHT,
      static_cast<unsigned long>(receivedFrameCount),
      static_cast<unsigned long>(droppedFrameCount),
      static_cast<unsigned int>(webFrameSize),
      digitalRead(PIN_HALL_1) ? "false" : "true",
      digitalRead(PIN_HALL_2) ? "false" : "true",
      digitalRead(PIN_HALL_3) ? "false" : "true",
      digitalRead(PIN_HALL_4) ? "false" : "true",
      digitalRead(PIN_IR_UP) ? "false" : "true",
      digitalRead(PIN_IR_DOWN) ? "false" : "true",
      digitalRead(PIN_BIN_FULL) ? "false" : "true");

  addNoCacheHeaders();
  server.send(200, "application/json", json);
}

void handleCommand() {
  const String command = server.arg("cmd");
  if (command == "STOP") {
    stopMotor();
    Serial.println("COMMAND: STOP");
  } else if (command == "CLEAN" || command == "DUMP" || command == "FLAT") {
    // Movement stays disabled until the full safety state machine is merged.
    currentStatus = "等待安全控制接入";
    Serial.printf("COMMAND RECEIVED (MOTOR DISABLED): %s\n", command.c_str());
  } else {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void initHttpServer() {
  server.enableCORS(true);
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/plain", "CatLitterTray camera gateway");
  });
  server.on("/frame", HTTP_GET, handleFrame);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/command", HTTP_GET, handleCommand);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
  Serial.println("HTTP CAMERA SERVER READY ON PORT 80");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[CatLitterTray] CAMERA WEB GATEWAY START");
  initPins();
  initBuffers();
  initCameraSPI();

  if (xTaskCreatePinnedToCore(cameraTask, "camera-rx", 4096, nullptr, 2,
                              nullptr, 0) != pdPASS) {
    halt("CAMERA TASK START FAILED");
  }

  // Start consuming SPI immediately. The ESP32-CAM may already be sending
  // while this board is still joining Wi-Fi.
  connectWifi();
  initHttpServer();
}

void loop() {
  server.handleClient();

  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED &&
      now - lastWifiRetryMs >= WIFI_RETRY_INTERVAL_MS) {
    lastWifiRetryMs = now;
    WiFi.reconnect();
  }
  delay(1);
}
