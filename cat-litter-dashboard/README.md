# Cat Litter Dashboard

智能猫砂盆 React 控制面板。摄像头画面来自 ESP32-S3 内置 HTTP 服务，不使用本地 AI 模型。

## 启动

```bash
npm install
npm run dev
```

打开 Vite 显示的网址，分别输入摄像头 S3 与电机 S3 串口输出的 IP。电脑与两块 ESP32-S3 必须处于同一局域网。

也可以预设设备地址：

```bash
VITE_ESP32_HOST=192.168.1.123 npm run dev
```

网页使用：

- `GET http://<S3-IP>/status` 获取设备状态和摄像头帧计数。
- `GET http://<S3-IP>/frame` 获取 `160x120` JPEG 帧。
- Dashboard 让浏览器原生解码 JPEG，并绘制到 Canvas。
- `GET http://<Motor-S3-IP>/status` 获取四路霍尔和电机状态。
- `GET http://<Motor-S3-IP>/move?target=H1&direction=forward` 控制滚筒正向转到 H1；目标可为 H1-H4，方向可为 `forward` 或 `reverse`。
- `GET http://<Motor-S3-IP>/stop` 紧急停止电机。
- `GET http://<Motor-S3-IP>/speed?value=200` 设置 PWM 速度，范围为 30-255；运行中修改会立即生效。
- `GET http://<Motor-S3-IP>/clean` 从原点 H3 启动一键清理：反向 H2、正向 H4、反向回到 H3。

电机定位没有自动超时：会按选定方向持续转动，直到目标霍尔连续触发 30 ms 或收到紧急停止。定位期间固件每 500 ms 在 115200 波特率串口输出 H1-H4 原始电平与运行状态。
