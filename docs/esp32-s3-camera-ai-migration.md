# ESP32-S3 摄像头网页链路

项目已停止使用 ESP32-S3 本地 TFLite 模型，当前只保留一个图像方案：

```text
OV3660 / ESP32-CAM
  -> 160x120 JPEG
  -> SPI MODE1 / 1 MHz / 1024-byte blocks
  -> ESP32-S3
  -> HTTP /frame
  -> React Canvas
```

## 接线

两块板必须共地。

| 信号 | ESP32-CAM | ESP32-S3 |
| --- | ---: | ---: |
| SCLK | GPIO 14 | GPIO 12 |
| MOSI | GPIO 13 | GPIO 11 |
| CS | GPIO 15 | GPIO 13 |
| GND | GND | GND |

ESP32-S3 的 GPIO 11、12、13 已固定给摄像头 SPI，不能同时连接顶盖、抽屉或 PIR 传感器。相关安全传感器需要后续重新分配引脚。

## 固件

- `firmware/esp32-cam-video/esp32-cam-video.ino`：采集并发送 JPEG。
- `firmware/esp32-s3-main/esp32-s3-main.ino`：在独立 FreeRTOS 任务收图，并通过 HTTP 提供图像与状态。

ESP32-CAM 帧协议：6 字节帧头 `AA 55 + uint32 little-endian length`，随后发送对应长度的 JPEG 数据，单帧上限为 64 KiB。

ESP32-S3 HTTP 接口：

- `GET /status`：设备、传感器和摄像头帧计数 JSON。
- `GET /frame`：最近一帧 JPEG，响应类型为 `image/jpeg`。
- `GET /command?cmd=STOP`：控制命令入口。目前仅 STOP 会确保电机输出为低，其他运动命令在安全状态机接入前不会驱动电机。

## 网页

运行：

```bash
cd cat-litter-dashboard
npm run dev
```

S3 启动后串口会打印：

```text
Dashboard device address: http://192.168.x.x
```

把该 IP 输入网页顶部的“ESP32-S3 地址”，点击“连接摄像头”。网页轮询 `/status`，只在帧计数变化时读取 `/frame`，然后使用浏览器原生 JPEG 解码器绘制到 Canvas。

使用板载 USB 转串口（端口名类似 `/dev/cu.usbserial-*`）时，Arduino IDE 中应设置 `Tools -> USB CDC On Boot -> Disabled`，串口监视器选择 `115200 baud`。若串口显示连续乱码，先核对这两项，然后重新烧录并按一下 RESET。

## 已验证

- 旧版 RGB565 的 ESP32-CAM 到 S3 SPI 图像传输已实机跑通；JPEG 固件需要重新烧录两块板后实机确认。
- 前端 `npm run build` 通过。
- S3 固件使用 ESP32 Arduino core 3.3.11 编译通过：程序空间约 29%，动态内存约 14%。
