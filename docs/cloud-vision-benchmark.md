# 猫砂盆云端视觉模型横评

`cat_vision_benchmark.py` 只使用 Python 标准库，并以同一提示词和同一批图片比较多个兼容
Chat Completions 的视觉 API，评测：

- `has_cat`：是否有猫；
- `has_cat_waste`：是否有猫粪便或尿液结团；
- `cat_breed`：猫品种。

标准答案未知的字段写 `null`，该字段不会参与准确率计算。不要把 Oxford
图片的排泄物答案强行标成 `false`；Oxford 只适合测猫存在和品种。

## 1. 生成 Oxford 清单

```bash
python3 cat_vision_benchmark.py prepare-oxford \
  --images /path/to/oxford/images \
  --list /path/to/oxford/annotations/list.txt \
  --output datasets/oxford.jsonl
```

## 2. 准备猫砂盆现场清单

复制 `vision_manifest.example.jsonl`，为自采图片人工标注。排泄物测试必须同时
包含正样本和负样本，且应覆盖粪便、尿团、被猫砂覆盖、阴影和脏镜头。

## 3. 配置密钥并运行

复制 `vision_providers.example.json` 后配置模型。密钥可以放在已被 Git 忽略的
`.env.local`：

```dotenv
DASHSCOPE_API_KEY=你的千问Key
ZHIPU_API_KEY=你的智谱Key
```

也可以直接使用环境变量：

```bash
export DASHSCOPE_API_KEY='...'
export ZHIPU_API_KEY='...'

python3 cat_vision_benchmark.py run \
  --manifest datasets/oxford.jsonl \
  --manifest datasets/litter-scenes.jsonl \
  --providers vision_providers.example.json \
  --output-dir vision_benchmark_results \
  --max-per-group 20 \
  --workers 4
```

首次连接真实 API 时可加 `--smoke`，每个模型只请求一张图片。

`--max-per-group` 使用固定随机种子按 `group` 分层抽样。第一次建议每组 5～20
张检查接口和输出；正式结论应跑完整自采测试集。

结果文件不会跨模型复用。文件名包含供应商、模型和本次输入指纹；中断后使用
相同配置重跑会断点续传。`comparison.json` 汇总准确率、格式成功率、平均/P95
延迟和 Token 用量。若在供应商配置中填写每百万 Token 单价，还会估算人民币成本：

```json
{
  "input_cny_per_million_tokens": 1.0,
  "output_cny_per_million_tokens": 10.0
}
```

当前项目配置只比较 Qwen 与 GLM。
