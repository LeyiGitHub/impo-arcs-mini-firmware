# 官方 SDK 补丁

`tools/prepare_sdk.py` 只接受固定提交的原 SDK，或已完整应用本目录补丁的 SDK；检测到其他已跟踪文件修改时停止。使用独立 Git 临时索引核验预期内容，不修改现有索引。

- `0001-buffer-network-speech.patch`：保留官方 Impo `/tts`、WAV 解码和音频队列，在原有 PSRAM 内缓冲短句；未完成的短句不直接播放。长回复在队列写满前启动。自测等待缓冲完成，失败或超时后不再空录音。
- `0002-bound-tls-reads.patch`：非阻塞 TLS 读取遇到 WANT_READ / WANT_WRITE 时立即交回调用者；有超时时限的读取使用剩余时间，避免内部重试无限延长。保留 SNI、证书链和主机名校验。
- `0003-reduce-small-record-delay.patch`：TCP_NODELAY 和慢写诊断。
- `0004-capture-voice-before-upload.patch`：完整录音结束后通过官方流上传。
- `0005-wake-and-continuous-dialogue.patch`：本地唤醒的麦克风所有权和连续回合。
- `0006-remember-selected-network.patch`：启动时优先尝试最后手动选择成功的网络。
- `0007-official-china-route.patch`：将上游国内路由源码及 Kconfig 接入 ARCS；补齐 TLS `common_name`、独立连接地址、HTTP Host/非默认端口和超时参数，Wi-Fi 地址变化重置地区缓存。国内路由策略和 TLS 透传实现保持官方版本，见 [说明](../docs/CHINA_ROUTE.md)。

- `0008-preferred-wifi-startup-budget.patch`：首选网络任务延后 1 秒启动，为同步存储和连接调用提供与 SDK Wi-Fi 管理任务一致的 8 KiB 栈，并记录栈余量。用于排查启动回归，不能将此改动本身视为栈溢出根因已确认。
- `0009-listenai-sdk-018-ipc.patch`：按聆思生成的 SDK 版本头选择双核初始化路径。0.1.6 保留手动初始化；0.1.8 使用底层 SYS_INIT 已建立的通信并核验共享布局，避免应用重复清空共享环。`version` 同时打印实际编译使用的聆思 SDK 版本。各版本的唤醒控制命令继续由各自聆思 SDK 编译，不能混用头文件或归档。
- `0010-listenai-sdk-018-psram-contract.patch`：为已核对的 0.1.8 AP 提供 CP 内存声明配置。SDK 组件将实际 PSRAM 起点和大小放在镜像 `0x180`；AP 先检查这些字段再启动 CP。构建后还会比较镜像字段和最终配置，缺失或不一致直接停止。0.1.6 使用独立的关闭配置，保持旧布局。

- `0011-official-relay-trust-root.patch`：增加官方新中继网关使用的 SSL.com TLS ECC Root CA 2022；保留原有公共根和强制证书验证。指纹及探测边界见国内连接说明。

这些是本项目的社区修正，不代表上游已接受。TLS、WebSocket、Noise、配对、凭据刷新、设备身份和语音上传协议继续使用原 SDK。网络连接质量和实际听测见 `docs/VALIDATION.md`。
