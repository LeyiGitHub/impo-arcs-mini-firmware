# 官方 SDK 补丁

`tools/prepare_sdk.py` 只接受固定提交的原 SDK，或已完整应用本目录补丁的 SDK；检测到其他已跟踪文件修改时停止。使用独立 Git 临时索引核验预期内容，不修改现有索引。

- `0001-buffer-network-speech.patch`：保留官方 Impo `/tts`、WAV 解码和音频队列，在原有 PSRAM 内缓冲短句；未完成的短句不直接播放。长回复在队列写满前启动。自测等待缓冲完成，失败或超时后不再空录音。

这些是本项目的社区修正，不代表上游已接受。TLS、WebSocket、Noise、配对、凭据刷新、设备身份和语音上传协议继续使用原 SDK。网络连接质量和实际听测见 `docs/VALIDATION.md`。
