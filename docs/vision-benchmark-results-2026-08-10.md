# 猫视觉模型 API 小样本评测报告

评测日期：2026-08-10

评测模型：Qwen3.7-Plus、GLM-5V-Turbo

数据来源：Oxford-IIIT Pet

> **团队复测主程序：[`cat_vision_benchmark.py`](../cat_vision_benchmark.py)**
>
> 同事应使用同一主程序、[`vision_providers.example.json`](../vision_providers.example.json) 和相同数据清单，保证提示词、抽样、解析及统计口径一致。

## 团队复测快速入口

### 1. 配置密钥

```bash
cp vision-benchmark.env.example .env.local
```

在 `.env.local` 中填写密钥，不要把真实密钥提交到 Git：

```dotenv
DASHSCOPE_API_KEY=你的千问Key
ZHIPU_API_KEY=你的智谱Key
```

### 2. 生成 Oxford 标准清单

```bash
python3 cat_vision_benchmark.py prepare-oxford \
  --images datasets/oxford/images \
  --list datasets/oxford/annotations/list.txt \
  --output datasets/oxford/manifest.jsonl
```

### 3. 先做单图冒烟测试

每个模型只调用一张图片，确认密钥、余额、模型权限和 JSON 输出正常：

```bash
python3 cat_vision_benchmark.py run \
  --manifest datasets/oxford/manifest.jsonl \
  --providers vision_providers.example.json \
  --output-dir vision_benchmark_results/team-smoke \
  --max-per-group 1 \
  --max-no-cat 12 \
  --smoke \
  --delay 0
```

### 4. 复现本报告的24张小样本测试

```bash
python3 cat_vision_benchmark.py run \
  --manifest datasets/oxford/manifest.jsonl \
  --providers vision_providers.example.json \
  --output-dir vision_benchmark_results/team-compact-24 \
  --max-per-group 1 \
  --max-no-cat 12 \
  --delay 0.2
```

该命令对每个模型调用24次：12个猫品种各1张，加12张无猫负样本。团队成员或更换 API Key 后应使用新的 `--output-dir`，防止断点续跑复用旧结果。

### 5. 加入猫砂盆排泄物数据

复制 [`vision_manifest.example.jsonl`](../vision_manifest.example.jsonl)，逐张标注：

```json
{"image":"/absolute/path/litter.jpg","ground_truth":{"has_cat":false,"has_cat_waste":true,"cat_breed":null},"group":"scene:no_cat:waste","source":"self-collected"}
```

将 Oxford 与猫砂盆清单同时传入：

```bash
python3 cat_vision_benchmark.py run \
  --manifest datasets/oxford/manifest.jsonl \
  --manifest datasets/litter-scenes.jsonl \
  --providers vision_providers.example.json \
  --output-dir vision_benchmark_results/team-with-waste \
  --max-per-group 5 \
  --delay 0.2
```

### 6. 输出文件

- `comparison.json`：模型总体准确率、延迟和 Token 对比；
- `*.summary.json`：单个模型汇总；
- `*.jsonl`：每张图片的标准答案、预测、错误、延迟和 Token；
- 已成功的记录支持断点续跑，HTTP或格式失败记录会在下次运行时重试。

## 1. 评测目的

比较国产视觉大模型在以下任务上的表现：

1. 判断图片中是否有猫；
2. 判断猫的品种；
3. 判断是否存在猫排泄物。

本轮 Oxford-IIIT Pet 数据不包含排泄物标签，因此只统计“是否有猫”和“猫品种”两项。排泄物识别需要使用另外标注的猫砂盆现场数据集。

## 2. 样本与方法

- 正式测试图片：24 张；
- 猫图片：12 张，Oxford 的 12 个猫品种各 1 张；
- 非猫图片：12 张，从狗图片中固定随机抽样；
- 每个模型处理完全相同的 24 张图片；
- 模型关闭思考模式，温度设为 0；
- 输出使用严格 JSON；
- 正式测试共调用 48 次，另有 2 次单图接口冒烟测试；
- HTTP 请求成功率和 JSON 解析成功率均纳入统计。

由于每个猫品种只有一张图片，本报告只能用于低成本初筛，不能视为最终模型排名。

## 3. 总体结果

| 模型 | 是否有猫 | 猫品种 | HTTP成功率 | JSON成功率 | 平均延迟 | P95延迟 |
|---|---:|---:|---:|---:|---:|---:|
| Qwen3.7-Plus | 24/24（100%） | 8/12（66.7%） | 24/24（100%） | 24/24（100%） | 1.70 秒 | 2.25 秒 |
| GLM-5V-Turbo | 23/24（95.8%） | 10/12（83.3%） | 24/24（100%） | 24/24（100%） | 9.83 秒 | 33.40 秒 |

## 4. 每张图片的完整结果

说明：`✓/✗` 表示该字段相对 Oxford 标准答案是否正确；Oxford 没有排泄物标签，因此“排泄物”列只记录模型输出，不参与准确率计算。两家模型在这 24 张图片上均输出“无排泄物”。狗图片没有猫品种标准答案，模型正确返回 `null`；GLM 将 Scottish Terrier 误判为猫时返回了 `unknown`。

| 图片 | 标准猫标签 | 标准品种 | Qwen猫 | Qwen品种 | Qwen排泄物 | Qwen延迟 | GLM猫 | GLM品种 | GLM排泄物 | GLM延迟 |
|---|---|---|---|---|---|---:|---|---|---|---:|
| [`Abyssinian_17.jpg`](../datasets/oxford/images/Abyssinian_17.jpg) | 有猫 | Abyssinian | 有猫 ✓ | Abyssinian ✓ | 无 | 5.43s | 有猫 ✓ | Abyssinian ✓ | 无 | 13.26s |
| [`Bengal_64.jpg`](../datasets/oxford/images/Bengal_64.jpg) | 有猫 | Bengal | 有猫 ✓ | Bengal ✓ | 无 | 1.89s | 有猫 ✓ | Bengal ✓ | 无 | 2.69s |
| [`Birman_96.jpg`](../datasets/oxford/images/Birman_96.jpg) | 有猫 | Birman | 有猫 ✓ | Birman ✓ | 无 | 1.40s | 有猫 ✓ | Siamese ✗ | 无 | 1.47s |
| [`Bombay_87.jpg`](../datasets/oxford/images/Bombay_87.jpg) | 有猫 | Bombay | 有猫 ✓ | unknown ✗ | 无 | 1.69s | 有猫 ✓ | Bombay ✓ | 无 | 4.30s |
| [`British_Shorthair_114.jpg`](../datasets/oxford/images/British_Shorthair_114.jpg) | 有猫 | British Shorthair | 有猫 ✓ | British Shorthair ✓ | 无 | 1.43s | 有猫 ✓ | British Shorthair ✓ | 无 | 1.67s |
| [`Egyptian_Mau_67.jpg`](../datasets/oxford/images/Egyptian_Mau_67.jpg) | 有猫 | Egyptian Mau | 有猫 ✓ | unknown ✗ | 无 | 1.37s | 有猫 ✓ | Egyptian Mau ✓ | 无 | 1.56s |
| [`Maine_Coon_207.jpg`](../datasets/oxford/images/Maine_Coon_207.jpg) | 有猫 | Maine Coon | 有猫 ✓ | unknown ✗ | 无 | 1.55s | 有猫 ✓ | unknown ✗ | 无 | 1.32s |
| [`Persian_179.jpg`](../datasets/oxford/images/Persian_179.jpg) | 有猫 | Persian | 有猫 ✓ | Persian ✓ | 无 | 1.55s | 有猫 ✓ | Persian ✓ | 无 | 10.13s |
| [`Ragdoll_44.jpg`](../datasets/oxford/images/Ragdoll_44.jpg) | 有猫 | Ragdoll | 有猫 ✓ | Ragdoll ✓ | 无 | 1.64s | 有猫 ✓ | Ragdoll ✓ | 无 | 13.15s |
| [`Russian_Blue_98.jpg`](../datasets/oxford/images/Russian_Blue_98.jpg) | 有猫 | Russian Blue | 有猫 ✓ | British Shorthair ✗ | 无 | 1.64s | 有猫 ✓ | Russian Blue ✓ | 无 | 9.27s |
| [`Siamese_35.jpg`](../datasets/oxford/images/Siamese_35.jpg) | 有猫 | Siamese | 有猫 ✓ | Siamese ✓ | 无 | 1.40s | 有猫 ✓ | Siamese ✓ | 无 | 2.05s |
| [`Sphynx_49.jpg`](../datasets/oxford/images/Sphynx_49.jpg) | 有猫 | Sphynx | 有猫 ✓ | Sphynx ✓ | 无 | 1.56s | 有猫 ✓ | Sphynx ✓ | 无 | 1.88s |
| [`basset_hound_61.jpg`](../datasets/oxford/images/basset_hound_61.jpg) | 无猫 | — | 无猫 ✓ | null | 无 | 1.63s | 无猫 ✓ | null | 无 | 12.71s |
| [`boxer_103.jpg`](../datasets/oxford/images/boxer_103.jpg) | 无猫 | — | 无猫 ✓ | null | 无 | 1.44s | 无猫 ✓ | null | 无 | 1.77s |
| [`chihuahua_46.jpg`](../datasets/oxford/images/chihuahua_46.jpg) | 无猫 | — | 无猫 ✓ | null | 无 | 1.45s | 无猫 ✓ | null | 无 | 6.17s |
| [`english_setter_114.jpg`](../datasets/oxford/images/english_setter_114.jpg) | 无猫 | — | 无猫 ✓ | null | 无 | 1.47s | 无猫 ✓ | null | 无 | 5.26s |
| [`german_shorthaired_157.jpg`](../datasets/oxford/images/german_shorthaired_157.jpg) | 无猫 | — | 无猫 ✓ | null | 无 | 1.42s | 无猫 ✓ | null | 无 | 1.70s |
| [`great_pyrenees_159.jpg`](../datasets/oxford/images/great_pyrenees_159.jpg) | 无猫 | — | 无猫 ✓ | null | 无 | 1.36s | 无猫 ✓ | null | 无 | 14.33s |
| [`keeshond_96.jpg`](../datasets/oxford/images/keeshond_96.jpg) | 无猫 | — | 无猫 ✓ | null | 无 | 1.38s | 无猫 ✓ | null | 无 | 33.40s |
| [`pomeranian_77.jpg`](../datasets/oxford/images/pomeranian_77.jpg) | 无猫 | — | 无猫 ✓ | null | 无 | 1.61s | 无猫 ✓ | null | 无 | 14.59s |
| [`pug_159.jpg`](../datasets/oxford/images/pug_159.jpg) | 无猫 | — | 无猫 ✓ | null | 无 | 1.21s | 无猫 ✓ | null | 无 | 11.21s |
| [`scottish_terrier_96.jpg`](../datasets/oxford/images/scottish_terrier_96.jpg) | 无猫 | — | 无猫 ✓ | null | 无 | 1.47s | 有猫 ✗ | unknown | 无 | 22.69s |
| [`staffordshire_bull_terrier_26.jpg`](../datasets/oxford/images/staffordshire_bull_terrier_26.jpg) | 无猫 | — | 无猫 ✓ | null | 无 | 2.25s | 无猫 ✓ | null | 无 | 5.74s |
| [`yorkshire_terrier_81.jpg`](../datasets/oxford/images/yorkshire_terrier_81.jpg) | 无猫 | — | 无猫 ✓ | null | 无 | 1.58s | 无猫 ✓ | null | 无 | 43.69s |

## 5. 错误明细

### Qwen3.7-Plus

“是否有猫”24 张全部正确。

猫品种错误 4 张：

| 图片 | 标准答案 | 模型答案 |
|---|---|---|
| `Bombay_87.jpg` | Bombay | unknown |
| `Egyptian_Mau_67.jpg` | Egyptian Mau | unknown |
| `Maine_Coon_207.jpg` | Maine Coon | unknown |
| `Russian_Blue_98.jpg` | Russian Blue | British Shorthair |

Qwen 对不确定品种更倾向于输出 `unknown`，因此避免了部分强行误分类，但降低了品种准确率。

### GLM-5V-Turbo

“是否有猫”错误 1 张：

| 图片 | 标准答案 | 模型答案 |
|---|---|---|
| `scottish_terrier_96.jpg` | 无猫（Scottish Terrier） | 有猫 |

猫品种错误 2 张：

| 图片 | 标准答案 | 模型答案 |
|---|---|---|
| `Birman_96.jpg` | Birman | Siamese |
| `Maine_Coon_207.jpg` | Maine Coon | unknown |

`Birman_96.jpg` 具有重点色外观，与 Siamese 容易混淆；`scottish_terrier_96.jpg` 中狗的竖耳和脸部轮廓可能导致猫狗误判。

## 6. 延迟与 Token 用量

| 模型 | 输入 Token | 输出 Token | 总 Token | 平均延迟 | P95延迟 |
|---|---:|---:|---:|---:|---:|
| Qwen3.7-Plus | 9,171 | 523 | 9,694 | 1.70 秒 | 2.25 秒 |
| GLM-5V-Turbo | 10,648 | 541 | 11,189 | 9.83 秒 | 33.40 秒 |

GLM 本轮猫品种准确率更高，但平均延迟约为 Qwen 的 5.8 倍，且存在超过 30 秒的长尾请求。供应商配置尚未填写每百万 Token 的实际单价，因此本报告不计算人民币费用。

## 7. 初步结论

- 判断是否有猫：本轮 Qwen 更好，准确率更高且速度明显更快；
- 判断猫品种：本轮 GLM 更好，但样本过小，差异不足以形成最终结论；
- 工程响应速度：Qwen 明显占优；
- 接口稳定性：两家正式测试均无 HTTP 或 JSON 格式失败；
- 排泄物识别：本轮没有有效标签，暂时无法比较。

如果产品优先考虑低延迟和高频调用，建议先以 Qwen3.7-Plus 为主模型；如果更重视猫品种识别，可扩大真实样本后继续比较 GLM。

## 8. 下一步建议

1. 收集至少 100～200 张猫砂盆现场图片，并人工标注 `has_cat` 和 `has_cat_waste`；
2. 排泄物数据应同时包含空猫砂、粪便、尿团、被猫砂覆盖、阴影和脏镜头；
3. 每个猫品种增加到至少 20 张，再判断品种识别差异；
4. 最终产品应增加 `unknown` 阈值和多帧投票，避免单张图片误判；
5. 按最新供应商价格补充单价配置，再计算每 1,000 张图片的成本。

## 9. 原始结果

- 汇总结果：[`comparison.json`](../vision_benchmark_results/retest-compact-24/comparison.json)
- Qwen逐图结果：[`qwen__qwen3.7-plus__24b3ed5464c9.jsonl`](../vision_benchmark_results/retest-compact-24/qwen__qwen3.7-plus__24b3ed5464c9.jsonl)
- GLM逐图结果：[`glm__glm-5v-turbo__f6f24ed815f3.jsonl`](../vision_benchmark_results/retest-compact-24/glm__glm-5v-turbo__f6f24ed815f3.jsonl)
