# Roboflow 宠物粪便四分类视觉模型评测报告

评测图片：307 张（同源原图 ID：246）

模型：`qwen3.7-plus`、`glm-4.6v`

数据来源：[Pet Poop Classifier](https://universe.roboflow.com/poo-detection/pet-poop-classifier)

## 1. 结论摘要

| 模型 | 总体准确率 | 同源图宏平均 | 有效输出 | 平均延迟 | P95延迟 | 输入Token | 输出Token | 估算费用 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `qwen3.7-plus` | 203/307（66.12%） | 64.60% | 307/307 | 1.41s | 1.80s | 63,856 | 2,628 | ¥0.149 |
| `glm-4.6v` | 147/307（47.88%） | 46.78% | 305/307 | 1.21s | 2.99s | 78,385 | 3,662 | ¥0.089 |

- 本测试中总体准确率更高的是 `qwen3.7-plus`：66.12%。
- Roboflow 页面标称其数据集专用模型准确率为 96.6%；本次 `qwen3.7-plus` 比该数字低 30.48 个百分点，`glm-4.6v` 低 48.72 个百分点。该对比仅供参考，因为本地 test 有 307 张，而页面 v1 显示 204 张，且 Roboflow 模型是在该数据集上专门训练的。
- 该数据集四类图片全部含有粪便，因此只能评估粪便状态分类，不能评估‘有没有粪便’的查准率、召回率或误报率。
- 文件中存在增强图或同源变体；总体准确率按 307 个文件计算，同源图宏平均用于降低重复变体的权重。
- 标签 `diare` 是数据集原始拼写，报告和模型输出均原样保留。结果只描述图片分类能力，不构成兽医诊断。
- 运行质量：`qwen3.7-plus` 有效输出 307/307；`glm-4.6v` 有效输出 305/307。GLM 的 2 张失败均为平台内容安全过滤，并按错误计入总体准确率。

## 2. 数据分布

| 类别 | 图片数 | 含义 |
|---|---:|---|
| normal | 77 | 正常、成形 |
| diare | 58 | 水样或明显腹泻 |
| lack-of-water | 88 | 干燥、坚硬或缺水状 |
| soft-poop | 84 | 偏软但非明显水样 |

## 3. 各类别准确率

| 类别 | 图片数 | `qwen3.7-plus` | `glm-4.6v` |
|---|---:|---:|---:|
| normal | 77 | 66/77（85.71%） | 73/77（94.81%） |
| diare | 58 | 39/58（67.24%） | 39/58（67.24%） |
| lack-of-water | 88 | 52/88（59.09%） | 18/88（20.45%） |
| soft-poop | 84 | 46/84（54.76%） | 17/84（20.24%） |

## 4. 混淆矩阵

### qwen3.7-plus

| 标准答案 \ 预测 | normal | diare | lack-of-water | soft-poop | ERROR |
|---|---:|---:|---:|---:|---:|
| normal | 66 | 1 | 5 | 5 | 0 |
| diare | 1 | 39 | 2 | 16 | 0 |
| lack-of-water | 34 | 2 | 52 | 0 | 0 |
| soft-poop | 26 | 1 | 11 | 46 | 0 |

### glm-4.6v

| 标准答案 \ 预测 | normal | diare | lack-of-water | soft-poop | ERROR |
|---|---:|---:|---:|---:|---:|
| normal | 73 | 0 | 2 | 1 | 1 |
| diare | 11 | 39 | 6 | 2 | 0 |
| lack-of-water | 70 | 0 | 18 | 0 | 0 |
| soft-poop | 55 | 6 | 5 | 17 | 1 |

## 5. 每张图片结果

`✓/✗` 表示与文件夹标准标签是否一致。

| 图片 | 标准标签 | `qwen3.7-plus`预测 | 对错 | 延迟 | `glm-4.6v`预测 | 对错 | 延迟 |
|---|---|---|---|---:|---|---|---:|
| [`N-104_jpg.rf.5b59b6735dd7cba82fff02a22f035fab.jpg`](../datasets/test/normal/N-104_jpg.rf.5b59b6735dd7cba82fff02a22f035fab.jpg) | normal | normal | ✓ | 1.42s | normal | ✓ | 1.04s |
| [`N-104_jpg.rf.6783fd842049119f7385e311cf12d7e2.jpg`](../datasets/test/normal/N-104_jpg.rf.6783fd842049119f7385e311cf12d7e2.jpg) | normal | normal | ✓ | 1.76s | normal | ✓ | 1.36s |
| [`N-105_jpg.rf.1eb197bcba052b8e42f85ad52f7fe1bc.jpg`](../datasets/test/normal/N-105_jpg.rf.1eb197bcba052b8e42f85ad52f7fe1bc.jpg) | normal | normal | ✓ | 1.84s | normal | ✓ | 1.02s |
| [`N-105_jpg.rf.dd59d8824f0e5b9ea071971a6a325e28.jpg`](../datasets/test/normal/N-105_jpg.rf.dd59d8824f0e5b9ea071971a6a325e28.jpg) | normal | normal | ✓ | 1.80s | normal | ✓ | 1.16s |
| [`N-109_jpg.rf.30679094a4da43be97d85736819e491b.jpg`](../datasets/test/normal/N-109_jpg.rf.30679094a4da43be97d85736819e491b.jpg) | normal | normal | ✓ | 1.23s | normal | ✓ | 0.84s |
| [`N-11_jpg.rf.47ae194c51b98957b4f603996cbdf942.jpg`](../datasets/test/normal/N-11_jpg.rf.47ae194c51b98957b4f603996cbdf942.jpg) | normal | normal | ✓ | 1.79s | normal | ✓ | 0.80s |
| [`N-124_jpg.rf.e0d9a8ccdfcbabfd02d4261fb2d6914a.jpg`](../datasets/test/normal/N-124_jpg.rf.e0d9a8ccdfcbabfd02d4261fb2d6914a.jpg) | normal | normal | ✓ | 1.19s | normal | ✓ | 0.90s |
| [`N-125_jpg.rf.3c090b53e7696f754f1149278f9d2a79.jpg`](../datasets/test/normal/N-125_jpg.rf.3c090b53e7696f754f1149278f9d2a79.jpg) | normal | normal | ✓ | 1.66s | normal | ✓ | 0.77s |
| [`N-127_jpg.rf.f8eae1999a03402003bf98257f153dae.jpg`](../datasets/test/normal/N-127_jpg.rf.f8eae1999a03402003bf98257f153dae.jpg) | normal | lack-of-water | ✗ | 1.34s | normal | ✓ | 1.84s |
| [`N-130_jpg.rf.217a503495a5ca8dc4d2d3b74649d230.jpg`](../datasets/test/normal/N-130_jpg.rf.217a503495a5ca8dc4d2d3b74649d230.jpg) | normal | normal | ✓ | 1.28s | normal | ✓ | 0.86s |
| [`N-131_jpg.rf.badf38cc440209e5d467ff99ba281c63.jpg`](../datasets/test/normal/N-131_jpg.rf.badf38cc440209e5d467ff99ba281c63.jpg) | normal | normal | ✓ | 1.19s | normal | ✓ | 2.11s |
| [`N-134_jpg.rf.fe8cb728ee7f76b2961c4375e8483c7a.jpg`](../datasets/test/normal/N-134_jpg.rf.fe8cb728ee7f76b2961c4375e8483c7a.jpg) | normal | normal | ✓ | 1.11s | lack-of-water | ✗ | 1.02s |
| [`N-138_jpg.rf.f36c14f15ed59a816082911674024bad.jpg`](../datasets/test/normal/N-138_jpg.rf.f36c14f15ed59a816082911674024bad.jpg) | normal | normal | ✓ | 1.05s | normal | ✓ | 2.58s |
| [`N-143_jpg.rf.282871f1a6aee2b6e35c304df58cc593.jpg`](../datasets/test/normal/N-143_jpg.rf.282871f1a6aee2b6e35c304df58cc593.jpg) | normal | normal | ✓ | 1.41s | normal | ✓ | 0.82s |
| [`N-148_jpg.rf.076b468d70e86e1258c49510a4aec520.jpg`](../datasets/test/normal/N-148_jpg.rf.076b468d70e86e1258c49510a4aec520.jpg) | normal | normal | ✓ | 1.49s | normal | ✓ | 0.76s |
| [`N-14_jpg.rf.29b724570c23d8035ca191bad89a80cd.jpg`](../datasets/test/normal/N-14_jpg.rf.29b724570c23d8035ca191bad89a80cd.jpg) | normal | normal | ✓ | 1.30s | normal | ✓ | 0.65s |
| [`N-14_jpg.rf.5b687151947f31cfb22f86b96d00a03c.jpg`](../datasets/test/normal/N-14_jpg.rf.5b687151947f31cfb22f86b96d00a03c.jpg) | normal | normal | ✓ | 1.48s | normal | ✓ | 1.10s |
| [`N-14_jpg.rf.db00483d4c9b0efef05c04f84627189d.jpg`](../datasets/test/normal/N-14_jpg.rf.db00483d4c9b0efef05c04f84627189d.jpg) | normal | normal | ✓ | 1.20s | normal | ✓ | 0.91s |
| [`N-152_jpg.rf.ce9a49089702c1fad059cdae2d2266fe.jpg`](../datasets/test/normal/N-152_jpg.rf.ce9a49089702c1fad059cdae2d2266fe.jpg) | normal | normal | ✓ | 1.42s | normal | ✓ | 0.76s |
| [`N-154_jpg.rf.cace507ae5b7e9c4eaa3a9c2db446c3a.jpg`](../datasets/test/normal/N-154_jpg.rf.cace507ae5b7e9c4eaa3a9c2db446c3a.jpg) | normal | normal | ✓ | 1.34s | normal | ✓ | 1.20s |
| [`N-154_jpg.rf.e8e0a30da86555f3de267942c8e808a8.jpg`](../datasets/test/normal/N-154_jpg.rf.e8e0a30da86555f3de267942c8e808a8.jpg) | normal | normal | ✓ | 1.20s | normal | ✓ | 0.66s |
| [`N-15_jpg.rf.c085be427f149c038fee7bdfd2cda2f9.jpg`](../datasets/test/normal/N-15_jpg.rf.c085be427f149c038fee7bdfd2cda2f9.jpg) | normal | normal | ✓ | 1.30s | normal | ✓ | 0.90s |
| [`N-15_jpg.rf.e471d75e5b2d56194ae3aa57c52f0afc.jpg`](../datasets/test/normal/N-15_jpg.rf.e471d75e5b2d56194ae3aa57c52f0afc.jpg) | normal | normal | ✓ | 1.49s | normal | ✓ | 0.84s |
| [`N-15_jpg.rf.f50255535c840af80327a16564a0ca8d.jpg`](../datasets/test/normal/N-15_jpg.rf.f50255535c840af80327a16564a0ca8d.jpg) | normal | lack-of-water | ✗ | 1.14s | normal | ✓ | 1.00s |
| [`N-162_jpg.rf.74fe4f43d65fc491623239b9f5388d8c.jpg`](../datasets/test/normal/N-162_jpg.rf.74fe4f43d65fc491623239b9f5388d8c.jpg) | normal | normal | ✓ | 1.36s | normal | ✓ | 1.24s |
| [`N-165_jpg.rf.f08602517e72d32a7759d5ccd1fbb7c6.jpg`](../datasets/test/normal/N-165_jpg.rf.f08602517e72d32a7759d5ccd1fbb7c6.jpg) | normal | normal | ✓ | 1.44s | normal | ✓ | 0.77s |
| [`N-16_jpg.rf.91275eee787a20f27b3efd9fc7203e25.jpg`](../datasets/test/normal/N-16_jpg.rf.91275eee787a20f27b3efd9fc7203e25.jpg) | normal | normal | ✓ | 1.10s | normal | ✓ | 0.88s |
| [`N-170_jpg.rf.ce341b4b537501d06ff9cc55c8ed734e.jpg`](../datasets/test/normal/N-170_jpg.rf.ce341b4b537501d06ff9cc55c8ed734e.jpg) | normal | normal | ✓ | 1.43s | normal | ✓ | 0.91s |
| [`N-171_jpg.rf.44aa67695fc81e35fb9f822e087d1f7b.jpg`](../datasets/test/normal/N-171_jpg.rf.44aa67695fc81e35fb9f822e087d1f7b.jpg) | normal | soft-poop | ✗ | 1.93s | normal | ✓ | 3.51s |
| [`N-188_jpg.rf.0ac2ccc226be0416e34be45512370a34.jpg`](../datasets/test/normal/N-188_jpg.rf.0ac2ccc226be0416e34be45512370a34.jpg) | normal | normal | ✓ | 1.27s | normal | ✓ | 1.67s |
| [`N-196_jpg.rf.55887e4ac5d84c7cc79e09bbd7bca520.jpg`](../datasets/test/normal/N-196_jpg.rf.55887e4ac5d84c7cc79e09bbd7bca520.jpg) | normal | soft-poop | ✗ | 1.48s | normal | ✓ | 3.24s |
| [`N-198_jpg.rf.30ecefa33284cd8b181a778f30b38367.jpg`](../datasets/test/normal/N-198_jpg.rf.30ecefa33284cd8b181a778f30b38367.jpg) | normal | normal | ✓ | 1.13s | normal | ✓ | 1.57s |
| [`N-19_jpg.rf.02103d11559b01e8d81d2e89c01a9859.jpg`](../datasets/test/normal/N-19_jpg.rf.02103d11559b01e8d81d2e89c01a9859.jpg) | normal | diare | ✗ | 1.38s | normal | ✓ | 2.09s |
| [`N-1_jpg.rf.c8cc338049ffa112d41d914065d5d41f.jpg`](../datasets/test/normal/N-1_jpg.rf.c8cc338049ffa112d41d914065d5d41f.jpg) | normal | normal | ✓ | 1.35s | normal | ✓ | 0.82s |
| [`N-208_jpg.rf.a4294a739bb3e5dadafece8e6cf95c99.jpg`](../datasets/test/normal/N-208_jpg.rf.a4294a739bb3e5dadafece8e6cf95c99.jpg) | normal | soft-poop | ✗ | 1.19s | normal | ✓ | 2.32s |
| [`N-20_jpg.rf.173025cf9a5701ed42ca73b6cb912c0a.jpg`](../datasets/test/normal/N-20_jpg.rf.173025cf9a5701ed42ca73b6cb912c0a.jpg) | normal | normal | ✓ | 1.24s | normal | ✓ | 0.68s |
| [`N-20_jpg.rf.b795c3c9d0b83782c8b76bfa20bce1c1.jpg`](../datasets/test/normal/N-20_jpg.rf.b795c3c9d0b83782c8b76bfa20bce1c1.jpg) | normal | normal | ✓ | 1.15s | normal | ✓ | 0.73s |
| [`N-210-3-_jpg.rf.01411f01c497078af1f7dda71e46f008.jpg`](../datasets/test/normal/N-210-3-_jpg.rf.01411f01c497078af1f7dda71e46f008.jpg) | normal | normal | ✓ | 1.19s | normal | ✓ | 3.11s |
| [`N-210-6-_jpg.rf.a4cc06217b61a331ee437acfd5f74977.jpg`](../datasets/test/normal/N-210-6-_jpg.rf.a4cc06217b61a331ee437acfd5f74977.jpg) | normal | normal | ✓ | 1.44s | normal | ✓ | 0.81s |
| [`N-23_jpg.rf.7659000ca9b412ee7eae0910397a40be.jpg`](../datasets/test/normal/N-23_jpg.rf.7659000ca9b412ee7eae0910397a40be.jpg) | normal | lack-of-water | ✗ | 1.24s | normal | ✓ | 1.27s |
| [`N-24_jpg.rf.3f111cec1727a688db3d068ebd153308.jpg`](../datasets/test/normal/N-24_jpg.rf.3f111cec1727a688db3d068ebd153308.jpg) | normal | normal | ✓ | 9.86s | normal | ✓ | 1.26s |
| [`N-24_jpg.rf.5e703c016229b58e7946c2a7723d15ba.jpg`](../datasets/test/normal/N-24_jpg.rf.5e703c016229b58e7946c2a7723d15ba.jpg) | normal | normal | ✓ | 1.34s | normal | ✓ | 0.85s |
| [`N-24_jpg.rf.97b574e7801504ad7eab2b628d611b48.jpg`](../datasets/test/normal/N-24_jpg.rf.97b574e7801504ad7eab2b628d611b48.jpg) | normal | normal | ✓ | 1.47s | normal | ✓ | 1.73s |
| [`N-24_jpg.rf.d29f812226975420b52490a1339d8fc2.jpg`](../datasets/test/normal/N-24_jpg.rf.d29f812226975420b52490a1339d8fc2.jpg) | normal | normal | ✓ | 1.65s | lack-of-water | ✗ | 1.10s |
| [`N-25_jpg.rf.b59a9b13c0fe2b71ee2ac4ad3e9f6060.jpg`](../datasets/test/normal/N-25_jpg.rf.b59a9b13c0fe2b71ee2ac4ad3e9f6060.jpg) | normal | normal | ✓ | 1.12s | normal | ✓ | 1.41s |
| [`N-26_jpg.rf.b668c4c78ac45db1fef36dd9fde45ac3.jpg`](../datasets/test/normal/N-26_jpg.rf.b668c4c78ac45db1fef36dd9fde45ac3.jpg) | normal | normal | ✓ | 1.30s | normal | ✓ | 0.80s |
| [`N-28_jpg.rf.f501a561a1717e00f5a37d0cf7c01e14.jpg`](../datasets/test/normal/N-28_jpg.rf.f501a561a1717e00f5a37d0cf7c01e14.jpg) | normal | normal | ✓ | 1.22s | normal | ✓ | 1.08s |
| [`N-29_jpg.rf.d2d6da302a69606c658e057435f9b5bc.jpg`](../datasets/test/normal/N-29_jpg.rf.d2d6da302a69606c658e057435f9b5bc.jpg) | normal | soft-poop | ✗ | 1.12s | soft-poop | ✗ | 3.45s |
| [`N-2_jpg.rf.701b5ff030ea4445d78e56c72a1f67c5.jpg`](../datasets/test/normal/N-2_jpg.rf.701b5ff030ea4445d78e56c72a1f67c5.jpg) | normal | normal | ✓ | 6.52s | normal | ✓ | 0.82s |
| [`N-2_jpg.rf.7c4138823936268237aa0d7bb63bb8c3.jpg`](../datasets/test/normal/N-2_jpg.rf.7c4138823936268237aa0d7bb63bb8c3.jpg) | normal | normal | ✓ | 1.01s | normal | ✓ | 0.67s |
| [`N-30_jpg.rf.513d10e67937ec0e8f463cc09381b469.jpg`](../datasets/test/normal/N-30_jpg.rf.513d10e67937ec0e8f463cc09381b469.jpg) | normal | normal | ✓ | 1.34s | normal | ✓ | 0.98s |
| [`N-30_jpg.rf.9b46d7dd42a5164b3cb82cf61e4dd3ae.jpg`](../datasets/test/normal/N-30_jpg.rf.9b46d7dd42a5164b3cb82cf61e4dd3ae.jpg) | normal | normal | ✓ | 1.26s | normal | ✓ | 1.71s |
| [`N-33_jpg.rf.121781e0a5604b6a614f4fd383b8bf83.jpg`](../datasets/test/normal/N-33_jpg.rf.121781e0a5604b6a614f4fd383b8bf83.jpg) | normal | normal | ✓ | 1.18s | normal | ✓ | 0.78s |
| [`N-33_jpg.rf.6df005ec495532dc9ef27e08bd521158.jpg`](../datasets/test/normal/N-33_jpg.rf.6df005ec495532dc9ef27e08bd521158.jpg) | normal | normal | ✓ | 1.32s | normal | ✓ | 0.74s |
| [`N-34_jpg.rf.0561d6baf393543ece4fe051d271d01b.jpg`](../datasets/test/normal/N-34_jpg.rf.0561d6baf393543ece4fe051d271d01b.jpg) | normal | normal | ✓ | 1.21s | ERROR | ✗ | — |
| [`N-38_jpg.rf.1f9def8e282a8d0d93fa61aa1fd931a6.jpg`](../datasets/test/normal/N-38_jpg.rf.1f9def8e282a8d0d93fa61aa1fd931a6.jpg) | normal | lack-of-water | ✗ | 1.39s | normal | ✓ | 0.98s |
| [`N-38_jpg.rf.96a616ac25565b08cc5e802c8dfb72b8.jpg`](../datasets/test/normal/N-38_jpg.rf.96a616ac25565b08cc5e802c8dfb72b8.jpg) | normal | normal | ✓ | 1.21s | normal | ✓ | 1.61s |
| [`N-44_jpg.rf.0155b3de0c06256207b30621e2023605.jpg`](../datasets/test/normal/N-44_jpg.rf.0155b3de0c06256207b30621e2023605.jpg) | normal | normal | ✓ | 1.49s | normal | ✓ | 0.73s |
| [`N-44_jpg.rf.2e3c6222fb3126a5e302545ae0541613.jpg`](../datasets/test/normal/N-44_jpg.rf.2e3c6222fb3126a5e302545ae0541613.jpg) | normal | normal | ✓ | 1.21s | normal | ✓ | 0.63s |
| [`N-45_jpg.rf.68f177fc875e2de45be272c7c8cc7c8c.jpg`](../datasets/test/normal/N-45_jpg.rf.68f177fc875e2de45be272c7c8cc7c8c.jpg) | normal | normal | ✓ | 1.07s | normal | ✓ | 0.67s |
| [`N-46_jpg.rf.95f2e5df85399e9cc89fcd986d1db368.jpg`](../datasets/test/normal/N-46_jpg.rf.95f2e5df85399e9cc89fcd986d1db368.jpg) | normal | normal | ✓ | 1.17s | normal | ✓ | 1.11s |
| [`N-46_jpg.rf.f4803ab1b5340242cdf1a7824a790b26.jpg`](../datasets/test/normal/N-46_jpg.rf.f4803ab1b5340242cdf1a7824a790b26.jpg) | normal | normal | ✓ | 1.22s | normal | ✓ | 1.35s |
| [`N-48_jpg.rf.b419ae937d6bcc1a1e1bb04b47580f1e.jpg`](../datasets/test/normal/N-48_jpg.rf.b419ae937d6bcc1a1e1bb04b47580f1e.jpg) | normal | normal | ✓ | 1.18s | normal | ✓ | 0.86s |
| [`N-49_jpg.rf.9554b0bdc886530fe7b1768da4bae97e.jpg`](../datasets/test/normal/N-49_jpg.rf.9554b0bdc886530fe7b1768da4bae97e.jpg) | normal | normal | ✓ | 1.03s | normal | ✓ | 0.98s |
| [`N-4_jpg.rf.888f694a174a86dc107da2a85dbe600b.jpg`](../datasets/test/normal/N-4_jpg.rf.888f694a174a86dc107da2a85dbe600b.jpg) | normal | soft-poop | ✗ | 1.47s | normal | ✓ | 1.38s |
| [`N-50_jpg.rf.fc95affcec4c65a43da71fb4586a4ffc.jpg`](../datasets/test/normal/N-50_jpg.rf.fc95affcec4c65a43da71fb4586a4ffc.jpg) | normal | normal | ✓ | 1.10s | normal | ✓ | 0.62s |
| [`N-56_jpg.rf.53c8238ec4f3617cd8345c6aa4464b2f.jpg`](../datasets/test/normal/N-56_jpg.rf.53c8238ec4f3617cd8345c6aa4464b2f.jpg) | normal | lack-of-water | ✗ | 1.21s | normal | ✓ | 0.84s |
| [`N-5_jpg.rf.000fa2a79bcd98b263446afab0fc7d2f.jpg`](../datasets/test/normal/N-5_jpg.rf.000fa2a79bcd98b263446afab0fc7d2f.jpg) | normal | normal | ✓ | 1.45s | normal | ✓ | 1.49s |
| [`N-5_jpg.rf.d51df52c44ab3405ca047ebeb4925e02.jpg`](../datasets/test/normal/N-5_jpg.rf.d51df52c44ab3405ca047ebeb4925e02.jpg) | normal | normal | ✓ | 1.20s | normal | ✓ | 0.83s |
| [`N-70_jpg.rf.37f6db0f8a5f8438e90243152a840f1a.jpg`](../datasets/test/normal/N-70_jpg.rf.37f6db0f8a5f8438e90243152a840f1a.jpg) | normal | normal | ✓ | 1.09s | normal | ✓ | 0.87s |
| [`N-70_jpg.rf.38aca07d395fd5925cd4cd58bf4ef95f.jpg`](../datasets/test/normal/N-70_jpg.rf.38aca07d395fd5925cd4cd58bf4ef95f.jpg) | normal | normal | ✓ | 1.12s | normal | ✓ | 0.77s |
| [`N-71_jpg.rf.54e25bab76924e6f0167e1bb9d52f31f.jpg`](../datasets/test/normal/N-71_jpg.rf.54e25bab76924e6f0167e1bb9d52f31f.jpg) | normal | normal | ✓ | 1.28s | normal | ✓ | 2.21s |
| [`N-73_jpg.rf.16dbf77308043471d3129e060e671074.jpg`](../datasets/test/normal/N-73_jpg.rf.16dbf77308043471d3129e060e671074.jpg) | normal | normal | ✓ | 1.63s | normal | ✓ | 0.67s |
| [`N-7_jpg.rf.b776ab6485ca05859f2c5fe7dc0f19d6.jpg`](../datasets/test/normal/N-7_jpg.rf.b776ab6485ca05859f2c5fe7dc0f19d6.jpg) | normal | normal | ✓ | 1.37s | normal | ✓ | 1.87s |
| [`N-87_jpg.rf.518c33558993898a46053ef397c4044b.jpg`](../datasets/test/normal/N-87_jpg.rf.518c33558993898a46053ef397c4044b.jpg) | normal | normal | ✓ | 1.38s | normal | ✓ | 1.97s |
| [`N-98_jpg.rf.08ebf8bbf4f7743c0f4f546f181f003a.jpg`](../datasets/test/normal/N-98_jpg.rf.08ebf8bbf4f7743c0f4f546f181f003a.jpg) | normal | normal | ✓ | 1.40s | normal | ✓ | 0.76s |
| [`SP-109_jpg.rf.7fc67ba4eac8a5a0d5c9bc9d1d548226.jpg`](../datasets/test/normal/SP-109_jpg.rf.7fc67ba4eac8a5a0d5c9bc9d1d548226.jpg) | normal | normal | ✓ | 1.21s | normal | ✓ | 3.13s |
| [`D-103_jpg.rf.0038af26fb487570f99f716dfa089446.jpg`](../datasets/test/diare/D-103_jpg.rf.0038af26fb487570f99f716dfa089446.jpg) | diare | diare | ✓ | 1.17s | diare | ✓ | 0.78s |
| [`D-103_jpg.rf.b05a63690ca5bce1ff5e165360dfc1ac.jpg`](../datasets/test/diare/D-103_jpg.rf.b05a63690ca5bce1ff5e165360dfc1ac.jpg) | diare | diare | ✓ | 1.53s | diare | ✓ | 2.81s |
| [`D-105_jpg.rf.1ec0d4f55521548b906f2e8babf2fb09.jpg`](../datasets/test/diare/D-105_jpg.rf.1ec0d4f55521548b906f2e8babf2fb09.jpg) | diare | diare | ✓ | 1.27s | normal | ✗ | 0.66s |
| [`D-107_jpg.rf.31fbb17d90f54712aee8e7b46e551d94.jpg`](../datasets/test/diare/D-107_jpg.rf.31fbb17d90f54712aee8e7b46e551d94.jpg) | diare | soft-poop | ✗ | 1.49s | diare | ✓ | 2.38s |
| [`D-108_jpg.rf.8117f7eca4408e476a5f88d3a2d4b895.jpg`](../datasets/test/diare/D-108_jpg.rf.8117f7eca4408e476a5f88d3a2d4b895.jpg) | diare | diare | ✓ | 1.25s | soft-poop | ✗ | 0.96s |
| [`D-112_jpg.rf.3f38b5192a320da0e6a98641a62e36fc.jpg`](../datasets/test/diare/D-112_jpg.rf.3f38b5192a320da0e6a98641a62e36fc.jpg) | diare | diare | ✓ | 1.16s | diare | ✓ | 0.66s |
| [`D-11_jpg.rf.fac65915e2637063fb8abc6a2ded4987.jpg`](../datasets/test/diare/D-11_jpg.rf.fac65915e2637063fb8abc6a2ded4987.jpg) | diare | lack-of-water | ✗ | 1.46s | lack-of-water | ✗ | 0.76s |
| [`D-122_jpg.rf.c2675c2ebce7c6f59212cf5c6bbaf4a5.jpg`](../datasets/test/diare/D-122_jpg.rf.c2675c2ebce7c6f59212cf5c6bbaf4a5.jpg) | diare | diare | ✓ | 1.22s | diare | ✓ | 0.87s |
| [`D-129_jpg.rf.4658a73a6decc48d92a79922c9ed678d.jpg`](../datasets/test/diare/D-129_jpg.rf.4658a73a6decc48d92a79922c9ed678d.jpg) | diare | soft-poop | ✗ | 1.30s | diare | ✓ | 1.12s |
| [`D-12_jpg.rf.1973025a7a24db56ae1de1ba9b43a543.jpg`](../datasets/test/diare/D-12_jpg.rf.1973025a7a24db56ae1de1ba9b43a543.jpg) | diare | soft-poop | ✗ | 1.24s | normal | ✗ | 0.82s |
| [`D-134_jpg.rf.97fef79a6453e3bd9d3923a53aaee738.jpg`](../datasets/test/diare/D-134_jpg.rf.97fef79a6453e3bd9d3923a53aaee738.jpg) | diare | diare | ✓ | 1.35s | diare | ✓ | 0.86s |
| [`D-141_jpg.rf.1bf8dc99452e5d46543314b837b0d490.jpg`](../datasets/test/diare/D-141_jpg.rf.1bf8dc99452e5d46543314b837b0d490.jpg) | diare | diare | ✓ | 7.76s | diare | ✓ | 0.72s |
| [`D-142_jpg.rf.20f552da19d1904be01d60d1b0c12d91.jpg`](../datasets/test/diare/D-142_jpg.rf.20f552da19d1904be01d60d1b0c12d91.jpg) | diare | soft-poop | ✗ | 1.20s | lack-of-water | ✗ | 0.92s |
| [`D-146_jpg.rf.8b3686d34a03ce931cc7550fa881fc7e.jpg`](../datasets/test/diare/D-146_jpg.rf.8b3686d34a03ce931cc7550fa881fc7e.jpg) | diare | soft-poop | ✗ | 1.37s | normal | ✗ | 1.25s |
| [`D-14_jpg.rf.39003b47b9c991c6b14cc820cacc2e4b.jpg`](../datasets/test/diare/D-14_jpg.rf.39003b47b9c991c6b14cc820cacc2e4b.jpg) | diare | diare | ✓ | 1.29s | lack-of-water | ✗ | 0.98s |
| [`D-14_jpg.rf.def8d915cc8be25227995fbeba63f993.jpg`](../datasets/test/diare/D-14_jpg.rf.def8d915cc8be25227995fbeba63f993.jpg) | diare | lack-of-water | ✗ | 1.37s | normal | ✗ | 0.78s |
| [`D-150_jpg.rf.5938c80d69972068117091824d812d3e.jpg`](../datasets/test/diare/D-150_jpg.rf.5938c80d69972068117091824d812d3e.jpg) | diare | diare | ✓ | 1.42s | diare | ✓ | 6.04s |
| [`D-156_jpg.rf.c4af977b71e732e4837af2bb5a700786.jpg`](../datasets/test/diare/D-156_jpg.rf.c4af977b71e732e4837af2bb5a700786.jpg) | diare | soft-poop | ✗ | 1.18s | diare | ✓ | 0.76s |
| [`D-16_jpg.rf.870152882ad23e79a5f905c96ba38a1a.jpg`](../datasets/test/diare/D-16_jpg.rf.870152882ad23e79a5f905c96ba38a1a.jpg) | diare | diare | ✓ | 1.21s | normal | ✗ | 2.86s |
| [`D-17_jpg.rf.2bfd0df56ea786a39c2b217434503906.jpg`](../datasets/test/diare/D-17_jpg.rf.2bfd0df56ea786a39c2b217434503906.jpg) | diare | diare | ✓ | 1.31s | diare | ✓ | 2.66s |
| [`D-186_jpg.rf.edd2a47d55d1fe8bcdf6283dff0d4fd2.jpg`](../datasets/test/diare/D-186_jpg.rf.edd2a47d55d1fe8bcdf6283dff0d4fd2.jpg) | diare | soft-poop | ✗ | 1.59s | normal | ✗ | 2.99s |
| [`D-191_jpg.rf.2a96c0bf3135903559ec75e426776f81.jpg`](../datasets/test/diare/D-191_jpg.rf.2a96c0bf3135903559ec75e426776f81.jpg) | diare | diare | ✓ | 1.29s | diare | ✓ | 0.85s |
| [`D-19_jpg.rf.e2910e20c6b5f3d7d4e323f647ec5912.jpg`](../datasets/test/diare/D-19_jpg.rf.e2910e20c6b5f3d7d4e323f647ec5912.jpg) | diare | diare | ✓ | 1.37s | diare | ✓ | 1.29s |
| [`D-19_jpg.rf.e627a27ce83ba1866bdf5d742c769c24.jpg`](../datasets/test/diare/D-19_jpg.rf.e627a27ce83ba1866bdf5d742c769c24.jpg) | diare | diare | ✓ | 1.11s | diare | ✓ | 1.60s |
| [`D-209_jpg.rf.b9769c3a21ec7cdbe4bd0f91a281024e.jpg`](../datasets/test/diare/D-209_jpg.rf.b9769c3a21ec7cdbe4bd0f91a281024e.jpg) | diare | diare | ✓ | 2.13s | diare | ✓ | 0.72s |
| [`D-21_jpg.rf.c9dc599a54f87c709b7545a76083eb92.jpg`](../datasets/test/diare/D-21_jpg.rf.c9dc599a54f87c709b7545a76083eb92.jpg) | diare | diare | ✓ | 1.37s | lack-of-water | ✗ | 2.02s |
| [`D-22_jpg.rf.de8304afd7909cb2661b2b3e5d1d754c.jpg`](../datasets/test/diare/D-22_jpg.rf.de8304afd7909cb2661b2b3e5d1d754c.jpg) | diare | diare | ✓ | 1.58s | diare | ✓ | 0.80s |
| [`D-23_jpg.rf.4d70b484c946e7898fef8d0f82511f79.jpg`](../datasets/test/diare/D-23_jpg.rf.4d70b484c946e7898fef8d0f82511f79.jpg) | diare | soft-poop | ✗ | 1.41s | soft-poop | ✗ | 2.50s |
| [`D-23_jpg.rf.ae90fffc1f52b07eea8ae2958d33a29e.jpg`](../datasets/test/diare/D-23_jpg.rf.ae90fffc1f52b07eea8ae2958d33a29e.jpg) | diare | soft-poop | ✗ | 1.37s | normal | ✗ | 0.60s |
| [`D-24_jpg.rf.f178699ccf049f0e6587ed60e0b2bc25.jpg`](../datasets/test/diare/D-24_jpg.rf.f178699ccf049f0e6587ed60e0b2bc25.jpg) | diare | soft-poop | ✗ | 1.15s | diare | ✓ | 2.54s |
| [`D-27_jpg.rf.294befc32ded4dc7fe3e6d4f2cde3a37.jpg`](../datasets/test/diare/D-27_jpg.rf.294befc32ded4dc7fe3e6d4f2cde3a37.jpg) | diare | diare | ✓ | 1.16s | diare | ✓ | 0.82s |
| [`D-28_jpg.rf.04cda1028083bac9e8e6e568cb6f56a7.jpg`](../datasets/test/diare/D-28_jpg.rf.04cda1028083bac9e8e6e568cb6f56a7.jpg) | diare | diare | ✓ | 1.22s | lack-of-water | ✗ | 0.77s |
| [`D-29_jpg.rf.0d6f78a00dfa952b91be1d0adabfe271.jpg`](../datasets/test/diare/D-29_jpg.rf.0d6f78a00dfa952b91be1d0adabfe271.jpg) | diare | diare | ✓ | 1.16s | diare | ✓ | 1.26s |
| [`D-30_jpg.rf.3fb42c60df979ece100f9156ddac6d3f.jpg`](../datasets/test/diare/D-30_jpg.rf.3fb42c60df979ece100f9156ddac6d3f.jpg) | diare | diare | ✓ | 1.17s | diare | ✓ | 0.79s |
| [`D-32_jpg.rf.e6265a16854dbf87be89aef77d4cca28.jpg`](../datasets/test/diare/D-32_jpg.rf.e6265a16854dbf87be89aef77d4cca28.jpg) | diare | diare | ✓ | 1.08s | normal | ✗ | 2.26s |
| [`D-35_jpg.rf.aaad8020ea98ea019e8e7f47afa0d868.jpg`](../datasets/test/diare/D-35_jpg.rf.aaad8020ea98ea019e8e7f47afa0d868.jpg) | diare | soft-poop | ✗ | 1.21s | normal | ✗ | 0.81s |
| [`D-37_jpg.rf.3a39619e73b61a90e1746f2d3ef2d806.jpg`](../datasets/test/diare/D-37_jpg.rf.3a39619e73b61a90e1746f2d3ef2d806.jpg) | diare | diare | ✓ | 1.08s | diare | ✓ | 0.82s |
| [`D-38_jpg.rf.187bc4527fed2ef3522cbad5f3a476c1.jpg`](../datasets/test/diare/D-38_jpg.rf.187bc4527fed2ef3522cbad5f3a476c1.jpg) | diare | diare | ✓ | 1.42s | lack-of-water | ✗ | 0.73s |
| [`D-3_jpg.rf.bff40a84c307c10aa4fea3f381a79802.jpg`](../datasets/test/diare/D-3_jpg.rf.bff40a84c307c10aa4fea3f381a79802.jpg) | diare | diare | ✓ | 1.71s | diare | ✓ | 1.95s |
| [`D-42_jpg.rf.c6ace936799c1397b5408bb2e7e8af4e.jpg`](../datasets/test/diare/D-42_jpg.rf.c6ace936799c1397b5408bb2e7e8af4e.jpg) | diare | soft-poop | ✗ | 1.16s | diare | ✓ | 0.72s |
| [`D-44_jpg.rf.045649e0292286d55f598dd3a1d7ce9b.jpg`](../datasets/test/diare/D-44_jpg.rf.045649e0292286d55f598dd3a1d7ce9b.jpg) | diare | diare | ✓ | 1.44s | diare | ✓ | 0.96s |
| [`D-47_jpg.rf.0e712691bdb178de41a02e498b7dad03.jpg`](../datasets/test/diare/D-47_jpg.rf.0e712691bdb178de41a02e498b7dad03.jpg) | diare | diare | ✓ | 1.19s | diare | ✓ | 1.24s |
| [`D-4_jpg.rf.4b30cbb521acfa11793be407575e5513.jpg`](../datasets/test/diare/D-4_jpg.rf.4b30cbb521acfa11793be407575e5513.jpg) | diare | soft-poop | ✗ | 1.11s | diare | ✓ | 1.72s |
| [`D-4_jpg.rf.5bc42289012d1bae00a923f3d92db08c.jpg`](../datasets/test/diare/D-4_jpg.rf.5bc42289012d1bae00a923f3d92db08c.jpg) | diare | soft-poop | ✗ | 1.37s | diare | ✓ | 0.85s |
| [`D-4_jpg.rf.685449fa36219eb9ee300002fbafd488.jpg`](../datasets/test/diare/D-4_jpg.rf.685449fa36219eb9ee300002fbafd488.jpg) | diare | soft-poop | ✗ | 1.22s | diare | ✓ | 0.83s |
| [`D-4_jpg.rf.d67a2cb2ad53d77c60375069a558be61.jpg`](../datasets/test/diare/D-4_jpg.rf.d67a2cb2ad53d77c60375069a558be61.jpg) | diare | diare | ✓ | 1.44s | diare | ✓ | 1.27s |
| [`D-51_jpg.rf.4bb473a9fa8c2e97083bb200156ebd62.jpg`](../datasets/test/diare/D-51_jpg.rf.4bb473a9fa8c2e97083bb200156ebd62.jpg) | diare | diare | ✓ | 1.25s | normal | ✗ | 0.56s |
| [`D-52_jpg.rf.511dfdc5eeba1cf176674f9f889528f2.jpg`](../datasets/test/diare/D-52_jpg.rf.511dfdc5eeba1cf176674f9f889528f2.jpg) | diare | diare | ✓ | 1.23s | diare | ✓ | 4.03s |
| [`D-53_jpg.rf.b504d563809a8eb3b8854cbfb39062f0.jpg`](../datasets/test/diare/D-53_jpg.rf.b504d563809a8eb3b8854cbfb39062f0.jpg) | diare | diare | ✓ | 1.30s | diare | ✓ | 1.13s |
| [`D-56_jpg.rf.4698c783117b11db11cc1068471af56c.jpg`](../datasets/test/diare/D-56_jpg.rf.4698c783117b11db11cc1068471af56c.jpg) | diare | diare | ✓ | 1.17s | diare | ✓ | 0.79s |
| [`D-58_jpg.rf.f0549c959da450fb0f862e0e8c1170da.jpg`](../datasets/test/diare/D-58_jpg.rf.f0549c959da450fb0f862e0e8c1170da.jpg) | diare | diare | ✓ | 1.25s | diare | ✓ | 0.80s |
| [`D-5_jpg.rf.0db554928843c870c3f239774dab104e.jpg`](../datasets/test/diare/D-5_jpg.rf.0db554928843c870c3f239774dab104e.jpg) | diare | normal | ✗ | 1.45s | normal | ✗ | 2.05s |
| [`D-60_jpg.rf.42f927be80bad95166b102908722b8ff.jpg`](../datasets/test/diare/D-60_jpg.rf.42f927be80bad95166b102908722b8ff.jpg) | diare | diare | ✓ | 1.41s | diare | ✓ | 0.90s |
| [`D-60_jpg.rf.908d6f348fbdfb39aea01183e23655fe.jpg`](../datasets/test/diare/D-60_jpg.rf.908d6f348fbdfb39aea01183e23655fe.jpg) | diare | diare | ✓ | 1.80s | diare | ✓ | 0.71s |
| [`D-7_jpg.rf.2a255eb6ab7ede3c03fcfa4ae27f049e.jpg`](../datasets/test/diare/D-7_jpg.rf.2a255eb6ab7ede3c03fcfa4ae27f049e.jpg) | diare | diare | ✓ | 4.11s | diare | ✓ | 0.83s |
| [`D-7_jpg.rf.3b99d9ddae05acbb0ecd6eb2ce181e15.jpg`](../datasets/test/diare/D-7_jpg.rf.3b99d9ddae05acbb0ecd6eb2ce181e15.jpg) | diare | diare | ✓ | 1.80s | diare | ✓ | 0.75s |
| [`D-7_jpg.rf.50cb01ddbc608beec41f713d271379c6.jpg`](../datasets/test/diare/D-7_jpg.rf.50cb01ddbc608beec41f713d271379c6.jpg) | diare | diare | ✓ | 1.03s | diare | ✓ | 4.32s |
| [`D-81_jpg.rf.d145e301ef07be079b3e8b826047fb67.jpg`](../datasets/test/diare/D-81_jpg.rf.d145e301ef07be079b3e8b826047fb67.jpg) | diare | soft-poop | ✗ | 1.05s | diare | ✓ | 1.35s |
| [`LW-100_jpg.rf.214d3d4510c27a7af99c59532c7f261d.jpg`](../datasets/test/lack-of-water/LW-100_jpg.rf.214d3d4510c27a7af99c59532c7f261d.jpg) | lack-of-water | normal | ✗ | 1.06s | normal | ✗ | 0.80s |
| [`LW-100_jpg.rf.77be973a299543c6e8cb9feddde66835.jpg`](../datasets/test/lack-of-water/LW-100_jpg.rf.77be973a299543c6e8cb9feddde66835.jpg) | lack-of-water | normal | ✗ | 1.53s | normal | ✗ | 0.71s |
| [`LW-125-1_jpg.rf.a010283117429edf8ddc8931d96fc55e.jpg`](../datasets/test/lack-of-water/LW-125-1_jpg.rf.a010283117429edf8ddc8931d96fc55e.jpg) | lack-of-water | normal | ✗ | 1.43s | normal | ✗ | 5.33s |
| [`LW-125-2_jpg.rf.e08c4c87d776a23349b8b1feff974850.jpg`](../datasets/test/lack-of-water/LW-125-2_jpg.rf.e08c4c87d776a23349b8b1feff974850.jpg) | lack-of-water | lack-of-water | ✓ | 1.40s | normal | ✗ | 0.71s |
| [`LW-125-3_jpg.rf.9204c2ee370b1edf8219dd5e4d984e34.jpg`](../datasets/test/lack-of-water/LW-125-3_jpg.rf.9204c2ee370b1edf8219dd5e4d984e34.jpg) | lack-of-water | lack-of-water | ✓ | 1.14s | normal | ✗ | 2.62s |
| [`LW-125_jpg.rf.44f3f08f443647e87b973f1bcac3c787.jpg`](../datasets/test/lack-of-water/LW-125_jpg.rf.44f3f08f443647e87b973f1bcac3c787.jpg) | lack-of-water | normal | ✗ | 1.21s | normal | ✗ | 1.02s |
| [`LW-127_jpg.rf.af8c308d3c3b40a6ad337d7e6417a40c.jpg`](../datasets/test/lack-of-water/LW-127_jpg.rf.af8c308d3c3b40a6ad337d7e6417a40c.jpg) | lack-of-water | lack-of-water | ✓ | 1.49s | lack-of-water | ✓ | 1.00s |
| [`LW-131_jpg.rf.8862a2db4fb46fd1c6d508f7524dcf57.jpg`](../datasets/test/lack-of-water/LW-131_jpg.rf.8862a2db4fb46fd1c6d508f7524dcf57.jpg) | lack-of-water | normal | ✗ | 1.12s | normal | ✗ | 0.91s |
| [`LW-133_jpg.rf.2008940536d791a589b7b40353c0c525.jpg`](../datasets/test/lack-of-water/LW-133_jpg.rf.2008940536d791a589b7b40353c0c525.jpg) | lack-of-water | lack-of-water | ✓ | 1.20s | normal | ✗ | 0.82s |
| [`LW-13_jpg.rf.8c8b1e862755716990b1854ad2bc9660.jpg`](../datasets/test/lack-of-water/LW-13_jpg.rf.8c8b1e862755716990b1854ad2bc9660.jpg) | lack-of-water | lack-of-water | ✓ | 1.22s | normal | ✗ | 0.83s |
| [`LW-147_jpg.rf.f6eb2504a22da644189f86213e7d8ca4.jpg`](../datasets/test/lack-of-water/LW-147_jpg.rf.f6eb2504a22da644189f86213e7d8ca4.jpg) | lack-of-water | normal | ✗ | 1.13s | normal | ✗ | 0.94s |
| [`LW-14_jpg.rf.a6c9ad2e1dbee82ca2fb30d4b1c6a35f.jpg`](../datasets/test/lack-of-water/LW-14_jpg.rf.a6c9ad2e1dbee82ca2fb30d4b1c6a35f.jpg) | lack-of-water | lack-of-water | ✓ | 1.50s | lack-of-water | ✓ | 0.88s |
| [`LW-14_jpg.rf.bdc09381a0fe5e8ac2ef744e5a611242.jpg`](../datasets/test/lack-of-water/LW-14_jpg.rf.bdc09381a0fe5e8ac2ef744e5a611242.jpg) | lack-of-water | normal | ✗ | 1.33s | normal | ✗ | 0.93s |
| [`LW-14_jpg.rf.c544f6fd84b8818d7a013f47ece19e50.jpg`](../datasets/test/lack-of-water/LW-14_jpg.rf.c544f6fd84b8818d7a013f47ece19e50.jpg) | lack-of-water | lack-of-water | ✓ | 1.26s | normal | ✗ | 0.94s |
| [`LW-151_jpg.rf.b494ed2e49dab28d63c2f5274d384778.jpg`](../datasets/test/lack-of-water/LW-151_jpg.rf.b494ed2e49dab28d63c2f5274d384778.jpg) | lack-of-water | lack-of-water | ✓ | 1.22s | normal | ✗ | 0.60s |
| [`LW-159_jpg.rf.57d6e5b25f71df8bdcfd93f22ace8168.jpg`](../datasets/test/lack-of-water/LW-159_jpg.rf.57d6e5b25f71df8bdcfd93f22ace8168.jpg) | lack-of-water | normal | ✗ | 1.98s | normal | ✗ | 0.95s |
| [`LW-15_jpg.rf.42daddedcf8d7fb9a851ce1aaf0d798c.jpg`](../datasets/test/lack-of-water/LW-15_jpg.rf.42daddedcf8d7fb9a851ce1aaf0d798c.jpg) | lack-of-water | normal | ✗ | 1.27s | normal | ✗ | 0.73s |
| [`LW-15_jpg.rf.cd84ded435a2969d6b6561fe6f1febc2.jpg`](../datasets/test/lack-of-water/LW-15_jpg.rf.cd84ded435a2969d6b6561fe6f1febc2.jpg) | lack-of-water | lack-of-water | ✓ | 1.50s | lack-of-water | ✓ | 1.12s |
| [`LW-160_jpg.rf.87637d21a7b5ab91fcdc33784c933ba8.jpg`](../datasets/test/lack-of-water/LW-160_jpg.rf.87637d21a7b5ab91fcdc33784c933ba8.jpg) | lack-of-water | lack-of-water | ✓ | 1.11s | normal | ✗ | 0.69s |
| [`LW-162_jpg.rf.5ca743990fc63aca795b50ee701dcdb1.jpg`](../datasets/test/lack-of-water/LW-162_jpg.rf.5ca743990fc63aca795b50ee701dcdb1.jpg) | lack-of-water | lack-of-water | ✓ | 1.48s | lack-of-water | ✓ | 0.85s |
| [`LW-170_jpg.rf.c7c1aab2f325e9c3967c41637f79904b.jpg`](../datasets/test/lack-of-water/LW-170_jpg.rf.c7c1aab2f325e9c3967c41637f79904b.jpg) | lack-of-water | lack-of-water | ✓ | 1.37s | lack-of-water | ✓ | 0.77s |
| [`LW-171_jpg.rf.4c2a0606c5c0e1834064b972ac66e695.jpg`](../datasets/test/lack-of-water/LW-171_jpg.rf.4c2a0606c5c0e1834064b972ac66e695.jpg) | lack-of-water | lack-of-water | ✓ | 1.29s | lack-of-water | ✓ | 2.03s |
| [`LW-171_jpg.rf.bb711cd6ed912623a4e5cf2ffaf4c5f5.jpg`](../datasets/test/lack-of-water/LW-171_jpg.rf.bb711cd6ed912623a4e5cf2ffaf4c5f5.jpg) | lack-of-water | lack-of-water | ✓ | 1.20s | lack-of-water | ✓ | 0.71s |
| [`LW-172_jpg.rf.481a5ce2bfc2c64ab4896842a3e69dba.jpg`](../datasets/test/lack-of-water/LW-172_jpg.rf.481a5ce2bfc2c64ab4896842a3e69dba.jpg) | lack-of-water | normal | ✗ | 1.14s | lack-of-water | ✓ | 0.66s |
| [`LW-175_jpg.rf.d78a20355a0c6c70b952bcf2c088ab41.jpg`](../datasets/test/lack-of-water/LW-175_jpg.rf.d78a20355a0c6c70b952bcf2c088ab41.jpg) | lack-of-water | lack-of-water | ✓ | 1.42s | lack-of-water | ✓ | 0.75s |
| [`LW-178_jpg.rf.759730e8b5ba067debb16a80bfe8369e.jpg`](../datasets/test/lack-of-water/LW-178_jpg.rf.759730e8b5ba067debb16a80bfe8369e.jpg) | lack-of-water | lack-of-water | ✓ | 1.19s | normal | ✗ | 1.17s |
| [`LW-17_jpg.rf.0991e4f02b1a474729980f642251ae1b.jpg`](../datasets/test/lack-of-water/LW-17_jpg.rf.0991e4f02b1a474729980f642251ae1b.jpg) | lack-of-water | normal | ✗ | 1.33s | normal | ✗ | 0.90s |
| [`LW-17_jpg.rf.62a84e1f8e7720c33f294c7d1cf4b2da.jpg`](../datasets/test/lack-of-water/LW-17_jpg.rf.62a84e1f8e7720c33f294c7d1cf4b2da.jpg) | lack-of-water | lack-of-water | ✓ | 1.13s | normal | ✗ | 0.83s |
| [`LW-188_jpg.rf.72d878bc095233111a69c84ee8854eaf.jpg`](../datasets/test/lack-of-water/LW-188_jpg.rf.72d878bc095233111a69c84ee8854eaf.jpg) | lack-of-water | normal | ✗ | 1.31s | normal | ✗ | 0.71s |
| [`LW-193_jpg.rf.56dfa341bebefad8c408f4c61f9c70a7.jpg`](../datasets/test/lack-of-water/LW-193_jpg.rf.56dfa341bebefad8c408f4c61f9c70a7.jpg) | lack-of-water | lack-of-water | ✓ | 2.39s | lack-of-water | ✓ | 0.85s |
| [`LW-195_jpg.rf.3d72d742c3717bba01941ae3f4ebc3c5.jpg`](../datasets/test/lack-of-water/LW-195_jpg.rf.3d72d742c3717bba01941ae3f4ebc3c5.jpg) | lack-of-water | lack-of-water | ✓ | 1.27s | lack-of-water | ✓ | 0.92s |
| [`LW-198_jpg.rf.250b9333ef0d9a111ed6673eea00bad6.jpg`](../datasets/test/lack-of-water/LW-198_jpg.rf.250b9333ef0d9a111ed6673eea00bad6.jpg) | lack-of-water | lack-of-water | ✓ | 1.26s | normal | ✗ | 3.55s |
| [`LW-198_jpg.rf.41a2a69d2dc2549617d7e968f2311d46.jpg`](../datasets/test/lack-of-water/LW-198_jpg.rf.41a2a69d2dc2549617d7e968f2311d46.jpg) | lack-of-water | lack-of-water | ✓ | 1.21s | normal | ✗ | 1.14s |
| [`LW-19_jpg.rf.25af2b539eab26ca212321cbaf05fba9.jpg`](../datasets/test/lack-of-water/LW-19_jpg.rf.25af2b539eab26ca212321cbaf05fba9.jpg) | lack-of-water | lack-of-water | ✓ | 1.22s | normal | ✗ | 0.74s |
| [`LW-19_jpg.rf.6e37f7f7cf106c94d7ba2d304c02ed79.jpg`](../datasets/test/lack-of-water/LW-19_jpg.rf.6e37f7f7cf106c94d7ba2d304c02ed79.jpg) | lack-of-water | lack-of-water | ✓ | 3.67s | normal | ✗ | 1.36s |
| [`LW-19_jpg.rf.d5e4029a8e7376b82e87decbbe90ee4e.jpg`](../datasets/test/lack-of-water/LW-19_jpg.rf.d5e4029a8e7376b82e87decbbe90ee4e.jpg) | lack-of-water | diare | ✗ | 1.40s | normal | ✗ | 0.95s |
| [`LW-1_jpg.rf.b11945f04dcb41ca36d404825014c875.jpg`](../datasets/test/lack-of-water/LW-1_jpg.rf.b11945f04dcb41ca36d404825014c875.jpg) | lack-of-water | lack-of-water | ✓ | 1.33s | lack-of-water | ✓ | 1.20s |
| [`LW-200_jpg.rf.b77e2d1ed15deee7cdf42ffcb657e57c.jpg`](../datasets/test/lack-of-water/LW-200_jpg.rf.b77e2d1ed15deee7cdf42ffcb657e57c.jpg) | lack-of-water | lack-of-water | ✓ | 1.20s | normal | ✗ | 0.94s |
| [`LW-203_jpg.rf.1a05f3dccc8ce1ee9fdb77c23ceb88f9.jpg`](../datasets/test/lack-of-water/LW-203_jpg.rf.1a05f3dccc8ce1ee9fdb77c23ceb88f9.jpg) | lack-of-water | lack-of-water | ✓ | 1.25s | normal | ✗ | 1.29s |
| [`LW-204_jpg.rf.41ae887b1496edc3e56fb9b372c3a05c.jpg`](../datasets/test/lack-of-water/LW-204_jpg.rf.41ae887b1496edc3e56fb9b372c3a05c.jpg) | lack-of-water | lack-of-water | ✓ | 1.14s | normal | ✗ | 1.07s |
| [`LW-20_jpg.rf.1dc59e2c6924d7e4d813d11e698c23b6.jpg`](../datasets/test/lack-of-water/LW-20_jpg.rf.1dc59e2c6924d7e4d813d11e698c23b6.jpg) | lack-of-water | lack-of-water | ✓ | 1.25s | normal | ✗ | 0.76s |
| [`LW-21_jpg.rf.f2adb68bbd2c24a25958d2d428ddbaaf.jpg`](../datasets/test/lack-of-water/LW-21_jpg.rf.f2adb68bbd2c24a25958d2d428ddbaaf.jpg) | lack-of-water | lack-of-water | ✓ | 1.18s | normal | ✗ | 1.24s |
| [`LW-23_jpg.rf.805a47d06f7855ef4f60522c985d6701.jpg`](../datasets/test/lack-of-water/LW-23_jpg.rf.805a47d06f7855ef4f60522c985d6701.jpg) | lack-of-water | lack-of-water | ✓ | 1.62s | lack-of-water | ✓ | 1.04s |
| [`LW-24_jpg.rf.4d5e41c5e87f0a33e887d9423c53a40f.jpg`](../datasets/test/lack-of-water/LW-24_jpg.rf.4d5e41c5e87f0a33e887d9423c53a40f.jpg) | lack-of-water | normal | ✗ | 1.19s | normal | ✗ | 1.97s |
| [`LW-26_jpg.rf.b08b72e657e415ba706d336ecf9d43a7.jpg`](../datasets/test/lack-of-water/LW-26_jpg.rf.b08b72e657e415ba706d336ecf9d43a7.jpg) | lack-of-water | lack-of-water | ✓ | 1.31s | normal | ✗ | 0.79s |
| [`LW-28_jpg.rf.68f34059ba25c23fa7e4271310ce11f4.jpg`](../datasets/test/lack-of-water/LW-28_jpg.rf.68f34059ba25c23fa7e4271310ce11f4.jpg) | lack-of-water | lack-of-water | ✓ | 1.27s | normal | ✗ | 0.66s |
| [`LW-28_jpg.rf.c096a2fb69dbe7e1f329ba29dbc62cf9.jpg`](../datasets/test/lack-of-water/LW-28_jpg.rf.c096a2fb69dbe7e1f329ba29dbc62cf9.jpg) | lack-of-water | lack-of-water | ✓ | 1.43s | normal | ✗ | 0.89s |
| [`LW-29_jpg.rf.30528eba2d459969a0a507dfd22c6903.jpg`](../datasets/test/lack-of-water/LW-29_jpg.rf.30528eba2d459969a0a507dfd22c6903.jpg) | lack-of-water | lack-of-water | ✓ | 1.16s | normal | ✗ | 1.37s |
| [`LW-2_jpg.rf.9e3cbf5d4963376d367e7f5c2787f972.jpg`](../datasets/test/lack-of-water/LW-2_jpg.rf.9e3cbf5d4963376d367e7f5c2787f972.jpg) | lack-of-water | lack-of-water | ✓ | 1.34s | normal | ✗ | 0.77s |
| [`LW-30_jpg.rf.d36a40da0db7bf63c086b37450f09af2.jpg`](../datasets/test/lack-of-water/LW-30_jpg.rf.d36a40da0db7bf63c086b37450f09af2.jpg) | lack-of-water | normal | ✗ | 2.13s | normal | ✗ | 0.79s |
| [`LW-31_jpg.rf.a648c4225d63e6c3ee038ba11af4f169.jpg`](../datasets/test/lack-of-water/LW-31_jpg.rf.a648c4225d63e6c3ee038ba11af4f169.jpg) | lack-of-water | normal | ✗ | 1.31s | normal | ✗ | 0.77s |
| [`LW-31_jpg.rf.eb53a7a20d4879a9df906df6bf587560.jpg`](../datasets/test/lack-of-water/LW-31_jpg.rf.eb53a7a20d4879a9df906df6bf587560.jpg) | lack-of-water | lack-of-water | ✓ | 1.57s | normal | ✗ | 1.59s |
| [`LW-33_jpg.rf.9db8148c54c38878fff98c3f87abfdfc.jpg`](../datasets/test/lack-of-water/LW-33_jpg.rf.9db8148c54c38878fff98c3f87abfdfc.jpg) | lack-of-water | lack-of-water | ✓ | 1.27s | lack-of-water | ✓ | 0.62s |
| [`LW-35_jpg.rf.34ffbaf3a09ccb707775aa9c5cfa7ac2.jpg`](../datasets/test/lack-of-water/LW-35_jpg.rf.34ffbaf3a09ccb707775aa9c5cfa7ac2.jpg) | lack-of-water | normal | ✗ | 1.29s | normal | ✗ | 0.64s |
| [`LW-35_jpg.rf.bdd38da466493777f4c948f60f835e84.jpg`](../datasets/test/lack-of-water/LW-35_jpg.rf.bdd38da466493777f4c948f60f835e84.jpg) | lack-of-water | normal | ✗ | 1.18s | normal | ✗ | 0.90s |
| [`LW-38_jpg.rf.e143bc03ee0baa9ae85e379de97c77a8.jpg`](../datasets/test/lack-of-water/LW-38_jpg.rf.e143bc03ee0baa9ae85e379de97c77a8.jpg) | lack-of-water | normal | ✗ | 1.33s | normal | ✗ | 0.73s |
| [`LW-3_jpg.rf.c3fdddf1eff7b44d3cc15dcb1161c8b3.jpg`](../datasets/test/lack-of-water/LW-3_jpg.rf.c3fdddf1eff7b44d3cc15dcb1161c8b3.jpg) | lack-of-water | lack-of-water | ✓ | 1.11s | lack-of-water | ✓ | 0.58s |
| [`LW-40_jpg.rf.c747d36c84a8b10b5adbf8d014080ce5.jpg`](../datasets/test/lack-of-water/LW-40_jpg.rf.c747d36c84a8b10b5adbf8d014080ce5.jpg) | lack-of-water | normal | ✗ | 1.04s | normal | ✗ | 0.66s |
| [`LW-42_jpg.rf.6f7efdad95e2dc1c89bb3742c48ac124.jpg`](../datasets/test/lack-of-water/LW-42_jpg.rf.6f7efdad95e2dc1c89bb3742c48ac124.jpg) | lack-of-water | lack-of-water | ✓ | 1.24s | normal | ✗ | 0.79s |
| [`LW-43_jpg.rf.4e46fb7fca05015dd2a2ea32c49cda66.jpg`](../datasets/test/lack-of-water/LW-43_jpg.rf.4e46fb7fca05015dd2a2ea32c49cda66.jpg) | lack-of-water | lack-of-water | ✓ | 1.33s | normal | ✗ | 0.94s |
| [`LW-44_jpg.rf.b796b14e59f22a618ea914ff5c8489c3.jpg`](../datasets/test/lack-of-water/LW-44_jpg.rf.b796b14e59f22a618ea914ff5c8489c3.jpg) | lack-of-water | normal | ✗ | 1.03s | normal | ✗ | 0.67s |
| [`LW-45-1_jpg.rf.c421dff2a2b4503d06c5f3878fb4e687.jpg`](../datasets/test/lack-of-water/LW-45-1_jpg.rf.c421dff2a2b4503d06c5f3878fb4e687.jpg) | lack-of-water | normal | ✗ | 1.17s | normal | ✗ | 0.92s |
| [`LW-45-2_jpg.rf.a546febb1527ca0cd3678ecc1a8122e4.jpg`](../datasets/test/lack-of-water/LW-45-2_jpg.rf.a546febb1527ca0cd3678ecc1a8122e4.jpg) | lack-of-water | lack-of-water | ✓ | 1.18s | normal | ✗ | 0.89s |
| [`LW-45_jpg.rf.91192f4e9756c3f8980efe1734af212f.jpg`](../datasets/test/lack-of-water/LW-45_jpg.rf.91192f4e9756c3f8980efe1734af212f.jpg) | lack-of-water | lack-of-water | ✓ | 1.62s | normal | ✗ | 0.82s |
| [`LW-46-1_jpg.rf.b40e82ae68b63e9942fa37e0b9f191dd.jpg`](../datasets/test/lack-of-water/LW-46-1_jpg.rf.b40e82ae68b63e9942fa37e0b9f191dd.jpg) | lack-of-water | lack-of-water | ✓ | 1.42s | normal | ✗ | 0.80s |
| [`LW-46_jpg.rf.b9240759889477df9ecb0aa59e99022f.jpg`](../datasets/test/lack-of-water/LW-46_jpg.rf.b9240759889477df9ecb0aa59e99022f.jpg) | lack-of-water | lack-of-water | ✓ | 1.22s | lack-of-water | ✓ | 0.78s |
| [`LW-46_jpg.rf.fb4091a83083698e24deb068a33c7fca.jpg`](../datasets/test/lack-of-water/LW-46_jpg.rf.fb4091a83083698e24deb068a33c7fca.jpg) | lack-of-water | normal | ✗ | 1.24s | normal | ✗ | 4.83s |
| [`LW-47_jpg.rf.364b110cbd50f367b9684f0cb392b980.jpg`](../datasets/test/lack-of-water/LW-47_jpg.rf.364b110cbd50f367b9684f0cb392b980.jpg) | lack-of-water | lack-of-water | ✓ | 1.28s | lack-of-water | ✓ | 0.89s |
| [`LW-47_jpg.rf.c7ba64cb0b7d8851ef418c9ee8dcc0bd.jpg`](../datasets/test/lack-of-water/LW-47_jpg.rf.c7ba64cb0b7d8851ef418c9ee8dcc0bd.jpg) | lack-of-water | lack-of-water | ✓ | 1.49s | normal | ✗ | 1.15s |
| [`LW-49_jpg.rf.2b0fc005b686265dcdc5b1010f829cd1.jpg`](../datasets/test/lack-of-water/LW-49_jpg.rf.2b0fc005b686265dcdc5b1010f829cd1.jpg) | lack-of-water | normal | ✗ | 1.81s | normal | ✗ | 2.13s |
| [`LW-4_jpg.rf.e95e35b750f54a88a1db56c3e3cd524a.jpg`](../datasets/test/lack-of-water/LW-4_jpg.rf.e95e35b750f54a88a1db56c3e3cd524a.jpg) | lack-of-water | lack-of-water | ✓ | 1.40s | normal | ✗ | 2.72s |
| [`LW-60_jpg.rf.0a795dc7d0748cba06a209f838a53db1.jpg`](../datasets/test/lack-of-water/LW-60_jpg.rf.0a795dc7d0748cba06a209f838a53db1.jpg) | lack-of-water | normal | ✗ | 1.09s | normal | ✗ | 1.10s |
| [`LW-60_jpg.rf.27447f6de3ed81e625e3aaacddce2eb5.jpg`](../datasets/test/lack-of-water/LW-60_jpg.rf.27447f6de3ed81e625e3aaacddce2eb5.jpg) | lack-of-water | lack-of-water | ✓ | 1.24s | normal | ✗ | 0.97s |
| [`LW-61_jpg.rf.f1974d4a68a3451041e4666bb9369a3e.jpg`](../datasets/test/lack-of-water/LW-61_jpg.rf.f1974d4a68a3451041e4666bb9369a3e.jpg) | lack-of-water | normal | ✗ | 1.08s | lack-of-water | ✓ | 0.77s |
| [`LW-63_jpg.rf.9d92b585ad99ce7bf8883587e0406bca.jpg`](../datasets/test/lack-of-water/LW-63_jpg.rf.9d92b585ad99ce7bf8883587e0406bca.jpg) | lack-of-water | normal | ✗ | 1.42s | normal | ✗ | 2.27s |
| [`LW-69_jpg.rf.b9f4997948df15cef8942641363e3c7f.jpg`](../datasets/test/lack-of-water/LW-69_jpg.rf.b9f4997948df15cef8942641363e3c7f.jpg) | lack-of-water | normal | ✗ | 1.05s | normal | ✗ | 0.66s |
| [`LW-69_jpg.rf.f45087bff2dceff0d5b3c2d4134492dd.jpg`](../datasets/test/lack-of-water/LW-69_jpg.rf.f45087bff2dceff0d5b3c2d4134492dd.jpg) | lack-of-water | lack-of-water | ✓ | 1.48s | normal | ✗ | 0.84s |
| [`LW-6_jpg.rf.0f2681de40437543767ceae5266bb875.jpg`](../datasets/test/lack-of-water/LW-6_jpg.rf.0f2681de40437543767ceae5266bb875.jpg) | lack-of-water | normal | ✗ | 1.40s | normal | ✗ | 0.72s |
| [`LW-70_jpg.rf.bf0c4cbf6df60a3b250a6c108e3375c5.jpg`](../datasets/test/lack-of-water/LW-70_jpg.rf.bf0c4cbf6df60a3b250a6c108e3375c5.jpg) | lack-of-water | normal | ✗ | 1.49s | normal | ✗ | 2.86s |
| [`LW-71_jpg.rf.8636629ef3635cc9b4bbe6b227dff828.jpg`](../datasets/test/lack-of-water/LW-71_jpg.rf.8636629ef3635cc9b4bbe6b227dff828.jpg) | lack-of-water | normal | ✗ | 1.17s | normal | ✗ | 0.63s |
| [`LW-72_jpg.rf.fb9cb0f14d1b82f1d28c544ef19d77a5.jpg`](../datasets/test/lack-of-water/LW-72_jpg.rf.fb9cb0f14d1b82f1d28c544ef19d77a5.jpg) | lack-of-water | normal | ✗ | 1.15s | normal | ✗ | 0.81s |
| [`LW-73_jpg.rf.2a0d95adc2d90fb6cbfd525dc9a7cd4f.jpg`](../datasets/test/lack-of-water/LW-73_jpg.rf.2a0d95adc2d90fb6cbfd525dc9a7cd4f.jpg) | lack-of-water | normal | ✗ | 1.11s | normal | ✗ | 0.63s |
| [`LW-7_jpg.rf.0170db228a30a655245d898fdc28f719.jpg`](../datasets/test/lack-of-water/LW-7_jpg.rf.0170db228a30a655245d898fdc28f719.jpg) | lack-of-water | normal | ✗ | 1.43s | normal | ✗ | 0.69s |
| [`LW-86_jpg.rf.79a8591b4b3f24b5acfe67b4c7d0981f.jpg`](../datasets/test/lack-of-water/LW-86_jpg.rf.79a8591b4b3f24b5acfe67b4c7d0981f.jpg) | lack-of-water | normal | ✗ | 1.09s | normal | ✗ | 0.84s |
| [`LW-89_jpg.rf.70ab76253ccd1b5aa9d429418faec0c1.jpg`](../datasets/test/lack-of-water/LW-89_jpg.rf.70ab76253ccd1b5aa9d429418faec0c1.jpg) | lack-of-water | diare | ✗ | 1.12s | normal | ✗ | 0.79s |
| [`LW-8_jpg.rf.04dd4849969736e65efaf3fb5789b5a7.jpg`](../datasets/test/lack-of-water/LW-8_jpg.rf.04dd4849969736e65efaf3fb5789b5a7.jpg) | lack-of-water | lack-of-water | ✓ | 1.60s | normal | ✗ | 1.80s |
| [`LW-8_jpg.rf.579e02b86881b4eb738efba4666965a0.jpg`](../datasets/test/lack-of-water/LW-8_jpg.rf.579e02b86881b4eb738efba4666965a0.jpg) | lack-of-water | lack-of-water | ✓ | 1.36s | normal | ✗ | 1.31s |
| [`LW-92_jpg.rf.1c0743eb5f919e30a5b072ffdaeb318a.jpg`](../datasets/test/lack-of-water/LW-92_jpg.rf.1c0743eb5f919e30a5b072ffdaeb318a.jpg) | lack-of-water | lack-of-water | ✓ | 1.32s | normal | ✗ | 0.89s |
| [`SP-108_jpg.rf.b52f3e420097a27a61390d241a4ae87f.jpg`](../datasets/test/soft-poop/SP-108_jpg.rf.b52f3e420097a27a61390d241a4ae87f.jpg) | soft-poop | lack-of-water | ✗ | 1.38s | lack-of-water | ✗ | 0.85s |
| [`SP-108_jpg.rf.dc6e8f022c15892b58f1de4d28eb3803.jpg`](../datasets/test/soft-poop/SP-108_jpg.rf.dc6e8f022c15892b58f1de4d28eb3803.jpg) | soft-poop | lack-of-water | ✗ | 1.18s | lack-of-water | ✗ | 1.66s |
| [`SP-10_jpg.rf.486ed0f81315b1afc4fbe74c0fbd0d0a.jpg`](../datasets/test/soft-poop/SP-10_jpg.rf.486ed0f81315b1afc4fbe74c0fbd0d0a.jpg) | soft-poop | normal | ✗ | 1.31s | normal | ✗ | 0.66s |
| [`SP-10_jpg.rf.eba1e6f1cf30341e50bb11408d504a35.jpg`](../datasets/test/soft-poop/SP-10_jpg.rf.eba1e6f1cf30341e50bb11408d504a35.jpg) | soft-poop | soft-poop | ✓ | 1.55s | lack-of-water | ✗ | 0.76s |
| [`SP-111_jpg.rf.c30bab5675401110474c1b1922a9b70a.jpg`](../datasets/test/soft-poop/SP-111_jpg.rf.c30bab5675401110474c1b1922a9b70a.jpg) | soft-poop | soft-poop | ✓ | 1.13s | soft-poop | ✓ | 0.71s |
| [`SP-118_jpg.rf.80deafe9478a7b61d48102764b86fd26.jpg`](../datasets/test/soft-poop/SP-118_jpg.rf.80deafe9478a7b61d48102764b86fd26.jpg) | soft-poop | normal | ✗ | 2.07s | normal | ✗ | 1.04s |
| [`SP-119_jpg.rf.66ef187afcc40989448d4a48bc0e0967.jpg`](../datasets/test/soft-poop/SP-119_jpg.rf.66ef187afcc40989448d4a48bc0e0967.jpg) | soft-poop | soft-poop | ✓ | 1.37s | normal | ✗ | 0.62s |
| [`SP-131_jpg.rf.525479c7977665f8a81e320b7c6286e2.jpg`](../datasets/test/soft-poop/SP-131_jpg.rf.525479c7977665f8a81e320b7c6286e2.jpg) | soft-poop | normal | ✗ | 1.33s | normal | ✗ | 0.74s |
| [`SP-133_jpg.rf.13adb49b1dd4275378568db82f1b4216.jpg`](../datasets/test/soft-poop/SP-133_jpg.rf.13adb49b1dd4275378568db82f1b4216.jpg) | soft-poop | normal | ✗ | 1.36s | normal | ✗ | 0.72s |
| [`SP-134_jpg.rf.846a3335f810900b4d0a46b6bd2287cf.jpg`](../datasets/test/soft-poop/SP-134_jpg.rf.846a3335f810900b4d0a46b6bd2287cf.jpg) | soft-poop | soft-poop | ✓ | 1.93s | soft-poop | ✓ | 1.07s |
| [`SP-135_jpg.rf.ab6d4c389ad24cedce98965559798f16.jpg`](../datasets/test/soft-poop/SP-135_jpg.rf.ab6d4c389ad24cedce98965559798f16.jpg) | soft-poop | normal | ✗ | 1.38s | normal | ✗ | 0.81s |
| [`SP-136_jpg.rf.b3e33207775c93f389ea867eac5089ff.jpg`](../datasets/test/soft-poop/SP-136_jpg.rf.b3e33207775c93f389ea867eac5089ff.jpg) | soft-poop | soft-poop | ✓ | 1.21s | normal | ✗ | 3.59s |
| [`SP-137_jpg.rf.699463c8b94c3a5ea37fbf5a0c557fa6.jpg`](../datasets/test/soft-poop/SP-137_jpg.rf.699463c8b94c3a5ea37fbf5a0c557fa6.jpg) | soft-poop | lack-of-water | ✗ | 1.37s | normal | ✗ | 0.75s |
| [`SP-142_jpg.rf.36da629e446c114a465207460d2a2445.jpg`](../datasets/test/soft-poop/SP-142_jpg.rf.36da629e446c114a465207460d2a2445.jpg) | soft-poop | soft-poop | ✓ | 1.52s | diare | ✗ | 1.71s |
| [`SP-143_jpg.rf.baaf29aee951adab8a19587011026fbc.jpg`](../datasets/test/soft-poop/SP-143_jpg.rf.baaf29aee951adab8a19587011026fbc.jpg) | soft-poop | normal | ✗ | 1.24s | normal | ✗ | 0.84s |
| [`SP-144_jpg.rf.3dcd11f9b8311afa794c26a46b85766f.jpg`](../datasets/test/soft-poop/SP-144_jpg.rf.3dcd11f9b8311afa794c26a46b85766f.jpg) | soft-poop | normal | ✗ | 1.07s | normal | ✗ | 0.80s |
| [`SP-14_jpg.rf.78cec68e3c4e04f40b6542c5beaa074a.jpg`](../datasets/test/soft-poop/SP-14_jpg.rf.78cec68e3c4e04f40b6542c5beaa074a.jpg) | soft-poop | lack-of-water | ✗ | 1.16s | normal | ✗ | 0.68s |
| [`SP-14_jpg.rf.b2cd132c2cf2f59e34ad81cddd7de953.jpg`](../datasets/test/soft-poop/SP-14_jpg.rf.b2cd132c2cf2f59e34ad81cddd7de953.jpg) | soft-poop | normal | ✗ | 1.18s | normal | ✗ | 0.74s |
| [`SP-153_jpg.rf.75ccfbe2bb538209f2201e5c572ad19f.jpg`](../datasets/test/soft-poop/SP-153_jpg.rf.75ccfbe2bb538209f2201e5c572ad19f.jpg) | soft-poop | soft-poop | ✓ | 1.39s | normal | ✗ | 0.77s |
| [`SP-155_jpg.rf.c6ba03acdae0ff776a28685207ea0c7c.jpg`](../datasets/test/soft-poop/SP-155_jpg.rf.c6ba03acdae0ff776a28685207ea0c7c.jpg) | soft-poop | normal | ✗ | 1.33s | normal | ✗ | 0.72s |
| [`SP-15_jpg.rf.a4ef8e4d3a1252fea1cc829e3de5aa99.jpg`](../datasets/test/soft-poop/SP-15_jpg.rf.a4ef8e4d3a1252fea1cc829e3de5aa99.jpg) | soft-poop | soft-poop | ✓ | 1.12s | diare | ✗ | 0.89s |
| [`SP-15_jpg.rf.fd9bdcf2b65c3d25852080f3b6e58864.jpg`](../datasets/test/soft-poop/SP-15_jpg.rf.fd9bdcf2b65c3d25852080f3b6e58864.jpg) | soft-poop | soft-poop | ✓ | 1.39s | normal | ✗ | 1.91s |
| [`SP-161_jpg.rf.e43d9508f46786b15225f3eeebf32293.jpg`](../datasets/test/soft-poop/SP-161_jpg.rf.e43d9508f46786b15225f3eeebf32293.jpg) | soft-poop | normal | ✗ | 1.60s | normal | ✗ | 0.71s |
| [`SP-165_jpg.rf.c9c5a94ebe10f9380809b42a699eb345.jpg`](../datasets/test/soft-poop/SP-165_jpg.rf.c9c5a94ebe10f9380809b42a699eb345.jpg) | soft-poop | soft-poop | ✓ | 1.43s | normal | ✗ | 2.37s |
| [`SP-16_jpg.rf.55117caadfe78a8354d81014d6f535c4.jpg`](../datasets/test/soft-poop/SP-16_jpg.rf.55117caadfe78a8354d81014d6f535c4.jpg) | soft-poop | soft-poop | ✓ | 1.23s | soft-poop | ✓ | 1.02s |
| [`SP-170_jpg.rf.64219a069b901591376b659a00ba0f4c.jpg`](../datasets/test/soft-poop/SP-170_jpg.rf.64219a069b901591376b659a00ba0f4c.jpg) | soft-poop | soft-poop | ✓ | 1.34s | normal | ✗ | 0.65s |
| [`SP-172_jpg.rf.889565d24ef5a4591126037056703c53.jpg`](../datasets/test/soft-poop/SP-172_jpg.rf.889565d24ef5a4591126037056703c53.jpg) | soft-poop | soft-poop | ✓ | 1.21s | normal | ✗ | 0.70s |
| [`SP-175_jpg.rf.dd2909514b97ccd663dcf689714e716f.jpg`](../datasets/test/soft-poop/SP-175_jpg.rf.dd2909514b97ccd663dcf689714e716f.jpg) | soft-poop | soft-poop | ✓ | 1.42s | ERROR | ✗ | — |
| [`SP-177_jpg.rf.c87a04a836390b934301a86d1c886e03.jpg`](../datasets/test/soft-poop/SP-177_jpg.rf.c87a04a836390b934301a86d1c886e03.jpg) | soft-poop | soft-poop | ✓ | 1.19s | normal | ✗ | 0.87s |
| [`SP-178_jpg.rf.5769ad8d2250e579947a9bbf2580bcb1.jpg`](../datasets/test/soft-poop/SP-178_jpg.rf.5769ad8d2250e579947a9bbf2580bcb1.jpg) | soft-poop | normal | ✗ | 1.37s | normal | ✗ | 0.95s |
| [`SP-178_jpg.rf.8e84a2fbcbf219ee005c442f1e6dd06e.jpg`](../datasets/test/soft-poop/SP-178_jpg.rf.8e84a2fbcbf219ee005c442f1e6dd06e.jpg) | soft-poop | normal | ✗ | 1.45s | normal | ✗ | 1.09s |
| [`SP-17_jpg.rf.2aaaab7fc6ae68348a5ff330106c5db4.jpg`](../datasets/test/soft-poop/SP-17_jpg.rf.2aaaab7fc6ae68348a5ff330106c5db4.jpg) | soft-poop | normal | ✗ | 1.22s | normal | ✗ | 0.99s |
| [`SP-181_jpg.rf.d6dbfdfc4bf738efe439bc7c5381dedf.jpg`](../datasets/test/soft-poop/SP-181_jpg.rf.d6dbfdfc4bf738efe439bc7c5381dedf.jpg) | soft-poop | soft-poop | ✓ | 1.29s | diare | ✗ | 0.78s |
| [`SP-189_jpg.rf.b5ccc287783303248d923d8735d93a51.jpg`](../datasets/test/soft-poop/SP-189_jpg.rf.b5ccc287783303248d923d8735d93a51.jpg) | soft-poop | lack-of-water | ✗ | 1.24s | soft-poop | ✓ | 0.79s |
| [`SP-191_jpg.rf.392191c5d9284f243a13b6955eb60567.jpg`](../datasets/test/soft-poop/SP-191_jpg.rf.392191c5d9284f243a13b6955eb60567.jpg) | soft-poop | soft-poop | ✓ | 1.37s | soft-poop | ✓ | 0.74s |
| [`SP-191_jpg.rf.4a8e6691795daf274c48d2d5f914710a.jpg`](../datasets/test/soft-poop/SP-191_jpg.rf.4a8e6691795daf274c48d2d5f914710a.jpg) | soft-poop | soft-poop | ✓ | 1.29s | soft-poop | ✓ | 0.65s |
| [`SP-192_jpg.rf.bba6179cfb3139161edc099148eb6cc4.jpg`](../datasets/test/soft-poop/SP-192_jpg.rf.bba6179cfb3139161edc099148eb6cc4.jpg) | soft-poop | soft-poop | ✓ | 1.27s | lack-of-water | ✗ | 0.85s |
| [`SP-194_jpg.rf.a5629b0261240e2d80dc0c5544114aa4.jpg`](../datasets/test/soft-poop/SP-194_jpg.rf.a5629b0261240e2d80dc0c5544114aa4.jpg) | soft-poop | normal | ✗ | 1.36s | normal | ✗ | 3.66s |
| [`SP-199_jpg.rf.4a79cb190514c35f19bd96b8812b756d.jpg`](../datasets/test/soft-poop/SP-199_jpg.rf.4a79cb190514c35f19bd96b8812b756d.jpg) | soft-poop | lack-of-water | ✗ | 1.22s | normal | ✗ | 0.76s |
| [`SP-19_jpg.rf.0ca5a79ebeae0ed7a9e408e88ec832ec.jpg`](../datasets/test/soft-poop/SP-19_jpg.rf.0ca5a79ebeae0ed7a9e408e88ec832ec.jpg) | soft-poop | soft-poop | ✓ | 1.36s | normal | ✗ | 0.80s |
| [`SP-1_jpg.rf.46b8c67bd94ffe8bcc1a1e6984498478.jpg`](../datasets/test/soft-poop/SP-1_jpg.rf.46b8c67bd94ffe8bcc1a1e6984498478.jpg) | soft-poop | soft-poop | ✓ | 1.23s | normal | ✗ | 0.66s |
| [`SP-207_jpg.rf.b5c4541097c19e484fa2883ba752f657.jpg`](../datasets/test/soft-poop/SP-207_jpg.rf.b5c4541097c19e484fa2883ba752f657.jpg) | soft-poop | normal | ✗ | 1.08s | normal | ✗ | 0.59s |
| [`SP-209_jpg.rf.947e5da43a21821ab8d53c5484c3adfa.jpg`](../datasets/test/soft-poop/SP-209_jpg.rf.947e5da43a21821ab8d53c5484c3adfa.jpg) | soft-poop | soft-poop | ✓ | 1.18s | soft-poop | ✓ | 1.47s |
| [`SP-209_jpg.rf.f2a169ff77fa3a28b546bcd34924434b.jpg`](../datasets/test/soft-poop/SP-209_jpg.rf.f2a169ff77fa3a28b546bcd34924434b.jpg) | soft-poop | soft-poop | ✓ | 1.27s | soft-poop | ✓ | 0.81s |
| [`SP-21_jpg.rf.bb3ed5d1352df166c5b6a48f3110afb7.jpg`](../datasets/test/soft-poop/SP-21_jpg.rf.bb3ed5d1352df166c5b6a48f3110afb7.jpg) | soft-poop | normal | ✗ | 1.09s | normal | ✗ | 0.79s |
| [`SP-22_jpg.rf.3240a5e57f68f51a5bea6a4639cb0743.jpg`](../datasets/test/soft-poop/SP-22_jpg.rf.3240a5e57f68f51a5bea6a4639cb0743.jpg) | soft-poop | soft-poop | ✓ | 1.36s | normal | ✗ | 0.83s |
| [`SP-22_jpg.rf.e4279685b663afcf5158ca1f3126eb9f.jpg`](../datasets/test/soft-poop/SP-22_jpg.rf.e4279685b663afcf5158ca1f3126eb9f.jpg) | soft-poop | soft-poop | ✓ | 1.37s | soft-poop | ✓ | 0.82s |
| [`SP-26_jpg.rf.0afff5563a0e0ce8796907baa79b5cb7.jpg`](../datasets/test/soft-poop/SP-26_jpg.rf.0afff5563a0e0ce8796907baa79b5cb7.jpg) | soft-poop | soft-poop | ✓ | 1.41s | normal | ✗ | 0.73s |
| [`SP-26_jpg.rf.43b32baaca4a49345344be34f9a4b304.jpg`](../datasets/test/soft-poop/SP-26_jpg.rf.43b32baaca4a49345344be34f9a4b304.jpg) | soft-poop | soft-poop | ✓ | 1.51s | normal | ✗ | 0.68s |
| [`SP-26_jpg.rf.c513db415be629d4ac49b6c8ff781628.jpg`](../datasets/test/soft-poop/SP-26_jpg.rf.c513db415be629d4ac49b6c8ff781628.jpg) | soft-poop | soft-poop | ✓ | 1.38s | normal | ✗ | 1.03s |
| [`SP-27_jpg.rf.01e6a532e4ceb7a7852e551f3293c315.jpg`](../datasets/test/soft-poop/SP-27_jpg.rf.01e6a532e4ceb7a7852e551f3293c315.jpg) | soft-poop | soft-poop | ✓ | 1.57s | diare | ✗ | 0.92s |
| [`SP-28_jpg.rf.24504cbb460182ecd9f3ea88227ce932.jpg`](../datasets/test/soft-poop/SP-28_jpg.rf.24504cbb460182ecd9f3ea88227ce932.jpg) | soft-poop | soft-poop | ✓ | 1.27s | normal | ✗ | 1.11s |
| [`SP-28_jpg.rf.364079b72a16d46db2fd9a45bedeafc9.jpg`](../datasets/test/soft-poop/SP-28_jpg.rf.364079b72a16d46db2fd9a45bedeafc9.jpg) | soft-poop | normal | ✗ | 1.20s | normal | ✗ | 0.71s |
| [`SP-28_jpg.rf.7a0c236f6dfacfcdfa556dcf2dbf29df.jpg`](../datasets/test/soft-poop/SP-28_jpg.rf.7a0c236f6dfacfcdfa556dcf2dbf29df.jpg) | soft-poop | soft-poop | ✓ | 1.37s | normal | ✗ | 0.76s |
| [`SP-29_jpg.rf.483bbb30cf6a7b765db2fba527340d8e.jpg`](../datasets/test/soft-poop/SP-29_jpg.rf.483bbb30cf6a7b765db2fba527340d8e.jpg) | soft-poop | normal | ✗ | 1.19s | normal | ✗ | 0.62s |
| [`SP-29_jpg.rf.8921b16a83be374fb7d6755a05fb0dbd.jpg`](../datasets/test/soft-poop/SP-29_jpg.rf.8921b16a83be374fb7d6755a05fb0dbd.jpg) | soft-poop | soft-poop | ✓ | 1.20s | normal | ✗ | 0.67s |
| [`SP-30_jpg.rf.3d92a52a9e2c670d9175ef9e5f446609.jpg`](../datasets/test/soft-poop/SP-30_jpg.rf.3d92a52a9e2c670d9175ef9e5f446609.jpg) | soft-poop | soft-poop | ✓ | 1.21s | lack-of-water | ✗ | 0.95s |
| [`SP-31_jpg.rf.5f22c2d15653de9f1c331024a7f0ebbb.jpg`](../datasets/test/soft-poop/SP-31_jpg.rf.5f22c2d15653de9f1c331024a7f0ebbb.jpg) | soft-poop | soft-poop | ✓ | 1.68s | normal | ✗ | 0.88s |
| [`SP-33_jpg.rf.7ae06a23d9a42c0ed492f6ceabec7588.jpg`](../datasets/test/soft-poop/SP-33_jpg.rf.7ae06a23d9a42c0ed492f6ceabec7588.jpg) | soft-poop | lack-of-water | ✗ | 1.27s | soft-poop | ✓ | 1.49s |
| [`SP-35_jpg.rf.16b2c117f0a2e26da945e389f944f492.jpg`](../datasets/test/soft-poop/SP-35_jpg.rf.16b2c117f0a2e26da945e389f944f492.jpg) | soft-poop | soft-poop | ✓ | 1.29s | normal | ✗ | 1.01s |
| [`SP-36_jpg.rf.1681bb86a6afecdec4be8ecae6191b76.jpg`](../datasets/test/soft-poop/SP-36_jpg.rf.1681bb86a6afecdec4be8ecae6191b76.jpg) | soft-poop | lack-of-water | ✗ | 1.25s | soft-poop | ✓ | 0.89s |
| [`SP-39_jpg.rf.0f429f8006ba708ff12da7e0fe30778c.jpg`](../datasets/test/soft-poop/SP-39_jpg.rf.0f429f8006ba708ff12da7e0fe30778c.jpg) | soft-poop | normal | ✗ | 1.66s | soft-poop | ✓ | 1.62s |
| [`SP-3_jpg.rf.b397fa5a41c14586d0e3cc0ecfb7ac0a.jpg`](../datasets/test/soft-poop/SP-3_jpg.rf.b397fa5a41c14586d0e3cc0ecfb7ac0a.jpg) | soft-poop | normal | ✗ | 1.23s | normal | ✗ | 0.98s |
| [`SP-41_jpg.rf.23b9e6ab0bfe9dab611df905df53a5f0.jpg`](../datasets/test/soft-poop/SP-41_jpg.rf.23b9e6ab0bfe9dab611df905df53a5f0.jpg) | soft-poop | lack-of-water | ✗ | 1.21s | normal | ✗ | 0.84s |
| [`SP-43_jpg.rf.5101e9c90f5b64c10439c7eeb5f012c3.jpg`](../datasets/test/soft-poop/SP-43_jpg.rf.5101e9c90f5b64c10439c7eeb5f012c3.jpg) | soft-poop | soft-poop | ✓ | 1.39s | normal | ✗ | 1.14s |
| [`SP-44_jpg.rf.62b19ab1a7f48727e3065821fa8d6a62.jpg`](../datasets/test/soft-poop/SP-44_jpg.rf.62b19ab1a7f48727e3065821fa8d6a62.jpg) | soft-poop | soft-poop | ✓ | 1.47s | soft-poop | ✓ | 1.63s |
| [`SP-46_jpg.rf.a470d4bec648c761fdb84aef4a36408f.jpg`](../datasets/test/soft-poop/SP-46_jpg.rf.a470d4bec648c761fdb84aef4a36408f.jpg) | soft-poop | normal | ✗ | 1.36s | normal | ✗ | 0.91s |
| [`SP-47_jpg.rf.ab5ed3a5f629d1529de765b7a35dd1bc.jpg`](../datasets/test/soft-poop/SP-47_jpg.rf.ab5ed3a5f629d1529de765b7a35dd1bc.jpg) | soft-poop | soft-poop | ✓ | 1.60s | soft-poop | ✓ | 3.12s |
| [`SP-48_jpg.rf.6b490db9365ad141dba76720ee702b7f.jpg`](../datasets/test/soft-poop/SP-48_jpg.rf.6b490db9365ad141dba76720ee702b7f.jpg) | soft-poop | soft-poop | ✓ | 1.32s | soft-poop | ✓ | 1.09s |
| [`SP-49_jpg.rf.8a0ae398709f6ae15c1ba9df3d02baed.jpg`](../datasets/test/soft-poop/SP-49_jpg.rf.8a0ae398709f6ae15c1ba9df3d02baed.jpg) | soft-poop | normal | ✗ | 1.16s | normal | ✗ | 1.07s |
| [`SP-53_jpg.rf.3f1a01077326397b9f94c6f2583612c8.jpg`](../datasets/test/soft-poop/SP-53_jpg.rf.3f1a01077326397b9f94c6f2583612c8.jpg) | soft-poop | soft-poop | ✓ | 1.35s | normal | ✗ | 0.56s |
| [`SP-56_jpg.rf.da62a4ac2f8be26f813d89d778d7cd70.jpg`](../datasets/test/soft-poop/SP-56_jpg.rf.da62a4ac2f8be26f813d89d778d7cd70.jpg) | soft-poop | normal | ✗ | 1.39s | normal | ✗ | 0.81s |
| [`SP-56_jpg.rf.e474b4aebe9067aa61912448a7bd55e3.jpg`](../datasets/test/soft-poop/SP-56_jpg.rf.e474b4aebe9067aa61912448a7bd55e3.jpg) | soft-poop | lack-of-water | ✗ | 1.23s | normal | ✗ | 0.72s |
| [`SP-57_jpg.rf.b8e356d4c79b74ffb9dc14d7cd0dbc95.jpg`](../datasets/test/soft-poop/SP-57_jpg.rf.b8e356d4c79b74ffb9dc14d7cd0dbc95.jpg) | soft-poop | soft-poop | ✓ | 1.44s | normal | ✗ | 1.40s |
| [`SP-59_jpg.rf.693136e3673af4290004d54bbbfde0a2.jpg`](../datasets/test/soft-poop/SP-59_jpg.rf.693136e3673af4290004d54bbbfde0a2.jpg) | soft-poop | soft-poop | ✓ | 1.27s | soft-poop | ✓ | 1.01s |
| [`SP-61_jpg.rf.bdef15be8c5ed3aa66b428563e922afb.jpg`](../datasets/test/soft-poop/SP-61_jpg.rf.bdef15be8c5ed3aa66b428563e922afb.jpg) | soft-poop | soft-poop | ✓ | 1.14s | normal | ✗ | 0.87s |
| [`SP-61_jpg.rf.f4abd7f8edcf98db4ee6ff8b1d2f4b51.jpg`](../datasets/test/soft-poop/SP-61_jpg.rf.f4abd7f8edcf98db4ee6ff8b1d2f4b51.jpg) | soft-poop | soft-poop | ✓ | 1.24s | normal | ✗ | 0.76s |
| [`SP-6_jpg.rf.a5f137dca4d0dda0a02e48e43833e4f9.jpg`](../datasets/test/soft-poop/SP-6_jpg.rf.a5f137dca4d0dda0a02e48e43833e4f9.jpg) | soft-poop | normal | ✗ | 1.35s | normal | ✗ | 7.17s |
| [`SP-80_jpg.rf.bb6774a8615ad69d108050f98c88bc01.jpg`](../datasets/test/soft-poop/SP-80_jpg.rf.bb6774a8615ad69d108050f98c88bc01.jpg) | soft-poop | lack-of-water | ✗ | 1.26s | normal | ✗ | 1.23s |
| [`SP-83_jpg.rf.1732fb59eabb4f8fb94ebfab52a1e2b1.jpg`](../datasets/test/soft-poop/SP-83_jpg.rf.1732fb59eabb4f8fb94ebfab52a1e2b1.jpg) | soft-poop | normal | ✗ | 1.35s | normal | ✗ | 1.99s |
| [`SP-86_jpg.rf.5c4424431113631a7a32941142204a14.jpg`](../datasets/test/soft-poop/SP-86_jpg.rf.5c4424431113631a7a32941142204a14.jpg) | soft-poop | soft-poop | ✓ | 1.63s | normal | ✗ | 0.64s |
| [`SP-8_jpg.rf.8cb6ce28068045304edf840200c2b4bd.jpg`](../datasets/test/soft-poop/SP-8_jpg.rf.8cb6ce28068045304edf840200c2b4bd.jpg) | soft-poop | normal | ✗ | 1.13s | soft-poop | ✓ | 0.78s |
| [`SP-90_jpg.rf.45314a64c58d312b26ef854d71fbdead.jpg`](../datasets/test/soft-poop/SP-90_jpg.rf.45314a64c58d312b26ef854d71fbdead.jpg) | soft-poop | soft-poop | ✓ | 1.39s | diare | ✗ | 0.64s |
| [`SP-99_jpg.rf.bf845e8109e231a539b0fe982b664e00.jpg`](../datasets/test/soft-poop/SP-99_jpg.rf.bf845e8109e231a539b0fe982b664e00.jpg) | soft-poop | diare | ✗ | 1.41s | diare | ✗ | 2.31s |

## 6. 原始结果

- `qwen3.7-plus`：`vision_benchmark_results/poop-test-2026-08-11/qwen__qwen3.7-plus__612ae61bc2bd.jsonl`
- `glm-4.6v`：`vision_benchmark_results/poop-test-2026-08-11/glm__glm-4.6v__9eda824ad2aa.jsonl`
