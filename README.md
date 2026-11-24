# IMPO + ARCS Mini SDK

基于 [Impo 官方 Gadget SDK](https://github.com/impoai/impo-gadget-sdk) 的可复用 Arcs-Mini2 设备 SDK，包含公共 C/C++ 接口、产品配置、应用扩展和云宝参考固件。设备使用官方 BLE 配对、Noise 链路、设备命令和语音回合，默认启用官方国内路由：大陆网络自动选择 `gadget.itoy.cn:9443`，TLS 和 HTTP Host 使用官方网关名 `gw.itoy.cn`；其他地区或国内线路连续失败时按官方策略直连。不依赖原来的 Mac / ACK / LifeLike 桥接服务。详见 [国内连接](docs/CHINA_ROUTE.md)。

这是独立的社区适配仓库。SDK 版本 **0.1.0**，面向与当前云宝相同接线的设备；不同产品通过 JSON 配置名称、平台产品 ID、连续对话和唤醒资源。接入、示例及资源约束见 [SDK 使用指南](docs/SDK.md)。官方 SDK 按 `dependencies.lock.json` 固定到 `ad0e6b7527501248631569516483e4845e2e5665`，板级适配位于 `firmware/boards/arcs_mini_yunbao`。`patches/` 记录 ARCS 音频缓冲、TLS 读取调度、TCP 小包发送、录音后上传、连续对话、选网优先级及国内路由兼容层；共享链路、路由和语音代码不另写一份。

## 复用方式

- 默认云宝：`python3 tools/build.py --docker --product products/yunbao.json`。
- 保留聆思 0.1.8 AP、原机“小聆小聆”模型的同款设备：提供对应 `ARCS_BASE`，使用 `--arcs-sdk-version 0.1.8 --product products/arcs-mini2-xiaoling.json`。
- 同硬件新产品：复制 `examples/button_companion/`，修改 JSON 与应用组件，通过 `--product` 和 `--app-dir` 构建。
- 不同唤醒词：引用目标词对应的模型清单，SDK 校验设备现有资源后启用；关闭唤醒时可独立使用按键对话。仅修改文本不会改变声学模型。
- C/C++ 应用入口：`sdk/include/impo_arcs_mini.h`；SDK 继续管理启动、配对和语音主流程。
- `python3 tools/package_sdk.py` 可导出固定提交的源码包，不包含 ListenAI SDK、模型或设备数据。

完整示例命令：

```sh
python3 tools/build.py --docker \
  --product examples/button_companion/product.json \
  --app-dir examples/button_companion
```

## 当前状态

- SDK 0.1.0 通过主机检查、C/C++ 接口检查及默认产品/独立应用示例的完整编译。保留聆思 0.1.8 AP 的第二台 Mini2 已使用通用产品配置完成 CP 更新和独立 ROM 校验；修复 AP 启动兼容及 TLS 读取阻塞后，国内入口注册、语音订阅通过，用户确认“小聆小聆”唤醒后两轮回答完整、静音回到待机，屏幕显示“Arcs Mini 2”。这是该镜像的实机短测；具体镜像摘要、日志和人工结果分别记录于 [验收记录](docs/VALIDATION.md)。

- 2026-11-24 已升级到官方国内路由与 TLS 透传版本，完整主机回归及交叉编译通过；云宝 CP 写入与独立读回通过，其他分区逐字节不变。启动栈修正后，屏幕、语音及连续对话已获人工确认；实机识别大陆后因国内业务 TLS 连续被重置，按官方策略回退直连。官方新提交采用 `gw.itoy.cn` 后，云宝新固件已写入并核验，实机经国内中继完成业务 TLS/Noise、注册确认和语音订阅，未回退直连；本版也已通过“你好云宝”唤醒、两轮完整回答、静音退出及屏幕的人工确认；这是一次短测，长期稳定性仍需继续观察。详见 [国内连接](docs/CHINA_ROUTE.md)。
- 云宝板级目标已实现：扬声器功放 PA01、左声道麦克风、PB04 按键。
- 2026-11-23 已通过一次实物问答：用户短按后问“二加三等于几”，Impo 回复“五”，用户确认听到完整回答；该回合约 24 秒后开始播放。当前仍存在连接停顿与重连，不能据此宣称稳定性通过。
- 已接入原机“你好云宝”本地唤醒和连续对话，实机结果见验收记录；短按开始/退出，静音约 5 秒退出，长按沿用 SDK 关机策略。详见 [唤醒与连续对话](docs/WAKE_DIALOGUE.md)。
- ST7789P3 单屏中文状态页已刷入实机，用户确认“云宝”、眼睛表情及中文状态显示正常，方向和文字正确。同一屏幕版固件在原网络上因 TLS 上传超时重连，切到手机热点后通过一次完整问答与人工听测，短按释放到回答开始约 38 秒。网络环境会影响结果，SDK 仍有慢写，长期稳定性未通过，详见 [屏幕适配](docs/SCREEN.md) 和验收记录。
- 不注册机器狗的电机、步态、IMU、触摸或摄像头能力；独立状态灯和电池曲线尚未适配。
- 主机测试、完整编译、实机刷写、Impo 配对和真实语音验收分别记录在 [验收状态](docs/VALIDATION.md)。代码存在不表示实机通过。

## 获取和构建

```sh
git clone https://github.com/LeyiGitHub/impo-arcs-mini-firmware.git
cd impo-arcs-mini-firmware
export ARCS_BASE=/path/to/listenai-arcs-sdk-0.1.6
python3 tools/build.py --docker
```

需要 Python 3、Git、Docker 和你有权使用的 ListenAI ARCS SDK。默认使用 **0.1.6**；保留原机 0.1.8 AP 时显式选择 `--arcs-sdk-version 0.1.8`，SDK 使用对应的双核初始化及唤醒 API。源码共用，构建目录按底层版本分开；构建工具拒绝声明版本与 `ARCS_BASE` 不符。工具链镜像、Impo SDK 提交和聆思源码树记录于 `dependencies.lock.json`。不分发 ListenAI SDK、AP、boot、唤醒资源和原机闪存。

macOS 已安装官方 RISC-V 工具链、cmake、ninja 时，可省去 `--docker`。默认输出为 `upstream/arcs/build-yunbao/impo-gadget.bin`；其他产品输出到 `upstream/arcs/build-<产品 id>/`。宿主需要 Python 3.9+。

## 配对与使用

先核实板型、备份完整闪存，按 [刷写和恢复说明](docs/FLASHING.md) 只更新 CP 应用。接着在 Impo 的设备设置打开开发者模式，通过 BLE 添加 `ImpoGadget-XXXXXX`。SSID、密码和设备凭据通过官方配对保存到设备；公开固件不内置凭据。默认云宝产品配对后可说“你好云宝”或短按 PB04 主功能键启动对话；提示音后说话，停顿后由 Impo 处理并通过盒子扬声器回复。回答结束、再次提示收音后可继续说下一句，静音约 5 秒或短按退出。已通过真实问答与完整听测，包括屏幕版在手机热点上的一次问答；原网络仍出现 TLS 写超时、语音下载停顿和重连问题，尚不能作为稳定可用固件。

成功配网或通过原生 `wifi connect` 手动连接后，会保存该网络的优先选择；重启先尝试它，失败时再回退到 SDK 已保存网络的自动选择。密码仍由官方 Wi-Fi 管理器保存，不内置于源码。

屏幕“待机”跟随 SDK 客户端在线状态。通过单个 ADB 交互 shell 读取 `pair`、`link`、`gadget device.health`：`paired=1` 表示已保存配对，`state=online` 表示已建立 Impo 链路；注册成功还应检查 `link.register acked`，语音需单独听测。`screen` 可查看显示写帧状态，`screen test` 可测试颜色；驱动回执不能代替实物验收。不要把这些命令的原始输出发布到公开仓库。

基础命令直接来自官方 SDK：`device.health`、`device.set_name`、`speaker.say`、`speaker.play_url`、`speaker.set_volume`、`speaker.beep`、`speaker.stop`、`speaker.status`、`display.show_animation`。`python3 tools/gen_manifest.py` 可生成与实际命令一致的产品清单；不借用机器狗产品 ID。

## 测试

```sh
sh tests/run.sh
sh tests/run_cloud.sh
ARCS_BASE=/path/to/arcs-sdk-0.1.6 sh tests/run_tls.sh
ARCS_BASE=/path/to/arcs-sdk-0.1.6 sh upstream/arcs/tests/host/run.sh
```

`run.sh` 获取固定 SDK、应用补丁，并检查按键、屏幕、音频队列、选网和国内路由。`run_cloud.sh` 可单独运行官方路由与真实 ARCS HTTP/TLS 兼容层回归；`run_tls.sh` 检查平台 TLS 读取边界、TCP_NODELAY、SNI 和强制证书校验配置。最后一项运行官方 SDK 的配对、音频、语音、策略与链路纯逻辑测试。设置了 `ARCS_BASE` 时，第一项也会运行 TLS 读取测试。

设置 `ARCS_BASE` 后，第一项还会执行真实语音任务的录音与上传测试；也可单独运行 `sh tests/run_voice.sh`。它检查网络调用发生前麦克风已停止、PCM 顺序与 WAV 长度、慢上传、静音、失败和取消期限，以及连续两轮、静音退出、按键取消、麦克风独占和唤醒资源边界。

## 许可证

新增代码采用 MIT；保留的 ListenAI 板级声明采用 Apache-2.0，中文位图字形采用 SIL OFL 1.1。参见 `LICENSE`、`LICENSE-APACHE`、`LICENSE-OFL`、`NOTICE` 及下载的官方 SDK 授权文件。凭据、配对、设备日志、原机备份与用户状态禁止提交。
