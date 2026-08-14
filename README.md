# catLitterTray

智能猫砂盆项目，包含 ESP32-S3 主控、ESP32-CAM 图像采集端和 Web 控制面板。

## 项目入口

- **AI/开发交接文档**：[`AI_HANDOFF.md`](AI_HANDOFF.md)
- **团队视觉模型评测入口**：[`cat_vision_benchmark.py`](cat_vision_benchmark.py)
- **全量评测结果（7,349 张，含逐图结果）**：[`docs/vision-benchmark-results-full-2026-08-11.md`](docs/vision-benchmark-results-full-2026-08-11.md)
- **宠物粪便四分类评测（307 张，含逐图结果）**：[`docs/poop-vision-benchmark-results-2026-08-11.md`](docs/poop-vision-benchmark-results-2026-08-11.md)
- **小样本评测与复现说明**：[`docs/vision-benchmark-results-2026-08-10.md`](docs/vision-benchmark-results-2026-08-10.md)
- **模型配置**：[`vision_providers.example.json`](vision_providers.example.json)
- **数据标注示例**：[`vision_manifest.example.jsonl`](vision_manifest.example.jsonl)
- `firmware/esp32-cam-video/`：OV3660 摄像头采集与 SPI 图像发送
- `firmware/esp32-s3-main/`：ESP32-S3 SPI 收图、HTTP 图像服务和猫砂盆主控
- `firmware/esp32-s3-motor-homing/`：第二块 ESP32-S3 的四路霍尔与电机绝对复位固件
- `cat-litter-dashboard/`：显示实时摄像头画面的前端控制面板
- `docs/esp32-s3-camera-ai-migration.md`：摄像头链路迁移与当前方案记录

当前唯一设备端图像方案为：`160x120 JPEG -> SPI -> ESP32-S3 HTTP -> Dashboard`。ESP32-S3 不运行本地 AI 模型。
