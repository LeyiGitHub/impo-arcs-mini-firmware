# 固定语料交错测试

此工具执行真实麦克风收音、官方 Link 上传和原有 `/tts` 回答。问题由 Mac
扬声器播放同一音频文件，不让云端每轮重新合成问题。先依据
[刷写流程](FLASHING.md) 核对设备身份、保留 AP、SDK 和精确镜像，再采样。
SDK 0.1.8 Mini2 与 0.1.6 云宝的结果分别统计。

Mac 麦克风可选录制同一时间线内的问题和回答。录制程序使用已获授权的麦克风，
没有权限时退出；不修改系统输入、输出设备。可以显式指定临时播放音量以提高
信噪比，结束后恢复原设置；若检测到用户中途改音量则停止并保留用户的新设置。
准备固定的单声道
PCM16 WAV 作为分析参考，测试时不移动设备、Mac 或改变网络。原始录音和日志
可能包含现场声音与设备身份，工具只允许输出到仓库外的私有目录。

```sh
swiftc tools/macos_voice_capture.swift -o /private/path/macos_voice_capture
python3 tools/voice_latency_bench.py --adb /path/to/adb \
  --serial IDENTIFIED_RUNTIME_SERIAL --expected-boot VERIFIED_BOOT_ID \
  --stimulus /private/path/fixed-question.wav \
  --recorder /private/path/macos_voice_capture \
  --output /private/path/new-bench-directory --cycles 30 \
  --modes batch:1200 overlap:1200 \
  --speaker-volume 70 --mac-output-volume 40 --volume 4
python3 tools/analyze_voice_bench.py /private/path/new-bench-directory \
  --reference /private/path/fixed-question.wav
```

每轮先等待空闲，切换配置，通过按键等价命令开始收音，留出 700 ms 安静校准，
再播放同一问题。每个循环轮换模式顺序，并交替正反顺序；不自动重试失败问题。
收到本轮摘要后结束连续会话的下一轮收音，避免把新一轮空白收音计入问题。
所有尝试、无语音、超时和分析拒绝都保留。中断或结束时恢复 `batch 1200` 和唤醒。

连接恢复后可以用相同命令显式加 `--resume`，只运行尚未尝试的计划位置。失败
或结果未知的轮次仍计入原数据，不重发；原始单轮文件及分段会话日志均保留。
续测要求相同启动标识、镜像、语料、模式和测试音量；改变条件须用新目录。

`speech_timing` 记录首响应体字节和 WAV 解码器首次输出 PCM；`sound_first_write`
记录音频驱动首次接受写入，包含声音代次。驱动写入不是 DMA 回调，也不是声学
首声。设备阶段差值只使用附带的 `mono_ms`；日志行前缀与该时钟的起点不同，
不能直接混减。主机只用日志到达时间约束声音搜索范围。

分析工具需要 `numpy`。它通过归一化相关匹配固定问题，再按参考音频最后一个
有效 10 ms 帧定位说完位置。在回答缓冲释放附近，寻找连续三个 10 ms 帧超过
现场噪声阈值的声音；阈值为此前噪声 RMS 的 95 分位数乘三。相关分数不足
0.15、回答峰值/噪声不足四倍、没有完整刺激尾部对齐时拒绝该声学样本。阈值、
拒绝原因及所有单轮数据写入分析结果，不将失败或拒绝静默移出总尝试数。

这里测量的是自动声学代理指标，不能替代人工听感或校准声压下的可懂度验收。
报告每种模式的尝试数、失败数、有效声学样本数、P50/P95，以及固定问题下的
局限。30 轮属于探索，不能扩大为全部问题、噪声环境或长期稳定性的结论。

可独立运行分析测试：

```sh
python3 tests/test_voice_bench_analysis.py
python3 tests/test_voice_bench_schedule.py
```


音量校准是测试条件，不是固件默认配置或 TTS 音色变更。清单记录原音量和临时
音量；低信噪比试跑、无语音失败和正式固定条件数据分开保存，不能只保留快样本。
两个模式连续失败时自动停止，先调查原因再开始新的一组测试。

`--reply-mode full|incremental` 可指定增量播报实验，结束恢复全文。该模式在采集
结束时提前保存快照，以启动、回合和采集结束时间同时匹配绑定最后有效语音，
避免连续对话重置或末尾摘要缺行造成错配；不从端点配置推算丢失时间戳。
增量 TTS 可早于最终文字，相关设备区间不能按串行相加。实际结果与工具修正见
[最终镜像语音验证](INCREMENTAL_REPLY_VALIDATION_20261125.md)。

端点完整性可用 `tools/voice_endpoint_check.py` 单独检查。该工具先完整播放带
句中停顿的语料，捕获录音结束事件后立即取消批量上传；确认没有完整上传或 ACK，
再比较 VAD 最后有效帧与固定语料尾部。它检查输入截尾，不等待云端答案，也不计算
回答延迟。需要提供已识别的设备、当前启动标识、录制程序及新的私有输出目录，
参数见 `--help`。默认比较 1200 / 800 ms，不能把截短输入造成的更快结束算作收益。

## 公开可复核数据

`tools/export_voice_bench.py DIRECTORY --output public.json` 只导出固定字段的
汇总、每轮计数及自动声学测量；不复制原始日志、录音、设备/会话标识、主机时间戳
或异常正文。分享前仍核对公开文件。现有 [60轮短问数据](data/voice-latency-20261124-short-60.json)
保留全部尝试和唯一失败，分位数仅来自成功且声学有效的样本。

GitHub Actions 启用时，`Host checks / sdk-source` 任务为每个提交生成固定源码 ZIP 和摘要清单，
可从该提交的 Actions 运行下载 `impo-arcs-mini-sdk-source`。源码包本身不包含
ListenAI SDK、固件镜像、模型、现场录音或配对凭据。SDK 使用与硬件验收分别记录。
