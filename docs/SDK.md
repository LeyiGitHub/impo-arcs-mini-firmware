# Impo ARCS Mini SDK 0.2.3

本 SDK 面向与当前云宝相同接线的 Arcs-Mini2：LS26、240×240 ST7789P3 屏幕、PB04 主功能键、PA01 扬声器功放、左声道麦克风。产品名称、平台产品 ID、对话说明、连续对话和唤醒资源由产品配置决定。`arcs_mini_yunbao` 是保留的硬件构建标识，不再决定产品名称或唤醒词。同款硬件共用源码；保留原机 AP 时按其运行时选择聆思 0.1.6 或 0.1.8 构建，不承诺同一个二进制跨版本通刷。

本项目 SDK **0.2.3** 与聆思底层 SDK **0.1.6 / 0.1.8** 分别编号。公共 C/C++ 接口和源码共用，两种底层依赖、版本配置及固件产物分开，详见 [SDK 版本对照](SDK_VERSIONS.md)。

官方 ESP32 也以 [Device SDK](https://github.com/impoai/impo-gadget-sdk/tree/main/esp32) 名义交付完整固件、通用组件、板级描述和构建配置。本项目沿用该组织方式，继续调用固定官方 SDK 的配对、Noise、语音和国内路由代码。它是社区 ARCS 适配 SDK，依赖开发者另行提供的 ListenAI SDK；不是一个脱离这些依赖的二进制库。

## 目录与接口

| 目录 | 用途 |
| --- | --- |
| `sdk/include/`、`sdk/src/` | 公共 C/C++ 头文件、产品 API、应用扩展及通用唤醒资源校验 |
| `firmware/boards/arcs_mini_yunbao/` | 同款硬件的引脚、屏幕、按键、功放和 AP 音频接入 |
| `products/yunbao.json` | 保持“你好云宝”的默认产品 |
| `examples/voice_companion/` | 无模型依赖的按键语音应用，显式选用边录边传并查询回合计数 |
| `examples/button_companion/` | 名称为 Mini、关闭本地唤醒、保留连续对话及一个自定义命令的完整应用 |
| `resources/wake/` | 唤醒资源的元数据；不包含模型或原机资源 |
| `patches/` | 固定官方 SDK 上的可复核修正 |

公共入口是 `sdk/include/impo_arcs_mini.h`：

- `impo_arcs_mini_product()`：读取本次构建的产品及唤醒配置，返回静态只读数据。
- `impo_arcs_mini_begin_conversation()`：在线且空闲时请求官方语音回合；离线或正在对话时返回 −1。
- `impo_arcs_mini_interrupt_conversation()`：启用 `wake_interrupt` 且在线时，停止当前回答并请求新一轮提示收音；正在录音或按键退出处理中返回 −1。
- `impo_arcs_mini_say(text)`：通过官方 `/tts` 发起播放，立即返回；不代表播放完成。
- `impo_arcs_mini_set_wake_enabled(bool)`、`impo_arcs_mini_wake_available()`：暂停/恢复已初始化的本地唤醒及读取可用性。产品关闭唤醒或初始化失败时不能凭此启用引擎。
- `impo_arcs_mini_send_event()`：沿用官方事件及 JSON 所有权约定。

接口用于任务上下文。应用的 `impo_arcs_mini_application()` 返回静态描述，可增加官方格式的 `gadget_command_t`、`gadget_event_t`，提供初始化、状态、100 ms tick 和短按所有权回调。`on_tick` 运行于 SDK 状态任务，必须有界且快速返回；`on_button` 返回 true 时消费本次短按，长按电源策略仍由板级处理。`on_init` 在板级初始化中运行，此时音频和网络尚未就绪；`on_status` 在 SDK 状态任务上运行，必须快速返回。长任务放到应用自己的 FreeRTOS 任务。标准命令、协议和 `main()` 均继续由官方工程管理。

## 构建默认产品

```sh
git clone https://github.com/LeyiGitHub/impo-arcs-mini-firmware.git
cd impo-arcs-mini-firmware
export ARCS_BASE=/path/to/listenai-arcs-sdk-0.1.6
python3 tools/build.py --docker --product products/yunbao.json
```

产物：`upstream/arcs/build-yunbao/impo-gadget.bin`。生成的产品头文件、字体、Kconfig 和元数据在 `upstream/arcs/products/yunbao/`。构建不会自动烧录设备。构建器检查生成归档的格式，发现损坏时仅删除当前构建目录中的相关产物并重建；一次执行最多额外重建一次，仍失败则停止。

### 保留 0.1.8 AP 的同款设备

`0.1.6` / `0.1.8` 指聆思芯片 SDK，和 Impo SDK、原机应用的 `3.x` 版本分别记录。先读原机 AP 的版本及模型摘要，再显式选择构建目标。已提供通用名称“Arcs Mini 2”、原机“小聆小聆”模型的独立产品配置：

```sh
export ARCS_BASE=/path/to/listenai-arcs-sdk-0.1.8
python3 tools/build.py --docker --arcs-sdk-version 0.1.8 \
  --product products/arcs-mini2-xiaoling.json
```

产物独立存放于 `upstream/arcs/build-arcs-mini2-xiaoling-arcs018/`，附 `impo-sdk-build.json` 记录底层版本、产品和镜像摘要。默认构建仍选择 0.1.6；`ARCS_BASE` 与显式版本不符时在准备源码前拒绝。0.1.8 使用聆思自动初始化的双核通信，0.1.6 保留旧初始化；不复制协议库，也不把一个版本的预编译归档放进另一个版本。源码树来源见 `dependencies.lock.json`。

已核对的 0.1.8 AP 还要求 CP 在镜像 `0x180` 声明 PSRAM 区间。本板配置为 `0x28800000 + 0x00800000`，由实际构建配置生成、底层链接器固定位置，构建器再次核验。缺少这两个字段时，AP 会显示固件不匹配并在约 10 秒后关机，CP 尚未运行。不能只核对标准 ARCS 头或 SDK 版本号；AP 的内存和 IPC 契约仍须与原机匹配。修正声明不改写 AP，也不关闭它的兼容检查。

Boot 1.1.9 还会保留旧 AP 看门狗。0.1.8 组件在最早的系统初始化阶段补回 0.1.6 的接管行为，按当前配置关闭该旧看门狗；Impo 的 `wdt0` 硬件看门狗及任务监控继续工作。启动日志报告旧 AP 看门狗接管前后的控制值和启用位，避免把写入校验通过当作持续运行通过。

TLS 使用非阻塞 socket 及接收 BIO，由平台层控制握手和读取期限。聆思 0.1.8 将 `mbedtls_net_recv_timeout(..., 0)` 改成无限等待，因此不能用它实现共享链路的立即轮询，否则接收等待会堵住语音上传。适配共用于 0.1.6 和 0.1.8；不修改供应商 SDK，也不放宽证书校验。

屏幕标题默认取 `display_name`，也可用 `screen_title` 单独设置；平台名称、型号和节点前缀同样由产品 JSON 生成。板级 overlay 不再设置产品身份。“云宝”只作为 `products/yunbao.json` 示例名称，底层目录名 `arcs_mini_yunbao` 是历史硬件标识，不决定设备名称。当前 Mini2 示例使用 “Arcs Mini 2” 和“小聆小聆”，名称与唤醒词相互独立。

`products/arcs-mini2-xiaoling.json` 只接受清单中精确匹配的原机模型和 wrap；并非所有标为“小聆小聆”的模型都具有相同字节。原机资源摘要不同则须重新生成资源描述。启动、联网、屏幕和唤醒听测的实际验收边界见 `VALIDATION.md`。

## 新建同款硬件产品

复制 `examples/button_companion/` 到自己的项目目录，修改 `product.json` 和 `app.c`：

```sh
python3 tools/build.py --docker \
  --product /path/to/my-product/product.json \
  --app-dir /path/to/my-product
```

可直接构建仓库内示例：

```sh
python3 tools/build.py --docker \
  --product examples/button_companion/product.json \
  --app-dir examples/button_companion
```

示例产物位于 `upstream/arcs/build-mini-companion/impo-gadget.bin`。未传 `--app-dir` 时使用默认 companion 应用（20 条命令和 5 类可选事件，见 [能力说明](COMPANION_CAPABILITIES.md)）；传入时，目录必须包含 `CMakeLists.txt` 并实现 `impo_arcs_mini_application()`。不复制或修改 SDK 主流程。

产品说明随清单在 Impo 开发者平台配置；编译 CP 不会自动更新云端产品定义。

产品 JSON 必须设置 `schema_version: 1`、`id`、`display_name`、`model_id`、`node_prefix`。`id`、`model_id`、`node_prefix` 为 1–32 位小写字母、数字或连字符；`id` 同时决定构建目录。可选项：

| 字段 | 含义 |
| --- | --- |
| `screen_title` | 屏幕标题，默认使用名称；须在 216 像素内 |
| `product_id` | Impo 平台上已注册的产品 ID，未注册时为 null；与本地 `id` 不同 |
| `instructions` | 给 Impo 的产品说明，最多 1500 UTF-8 字节；省略时按配置生成 |
| `continuous_dialogue` | 回答后继续收音，默认 true |
| `china_relay` | 官方大陆路由及失败回退，默认 true |
| `wake` | 关闭时为 `{"enabled":false}`；启用时还须提供 `bundle` 路径 |

构建拒绝未知字段，包括 Wi-Fi 密码、账号凭据等。开发者使用自己的 Impo 账号配对，同款设备的设备身份不会从配置或示例复制。

## 语音上传与阶段查询

2026-11-26 的接入优化候选新增 `impo_arcs_mini_configure_voice_tail(keep_ms)`：
0 关闭（默认），200～1000 ms 在正常 batch 静音端点后保留对应尾部余量；会话忙、
overlap 或非法参数返回 −1。不会缩短端点，不写 KV。WS 写入合并、TTS 每请求
120 码点上限、长回复缓冲修正及完整验收方案见[云端接入优化](IMPO_CLOUD_OPTIMIZATION_20261126.md)。

优化属于公共 SDK：产品选择策略，应用不复制采集、Link、Noise、WAV/Base64 或
TTS 主流程。较长输入复测曾出现 ACK 后无完整回复及设备重启，尚未通过稳定性门槛；
`overlap` 仅作为显式实验选项。默认 `batch` 在收音结束后上传；显式选择 `overlap` 后，在确认语音时
开始同一笔上传。两种模式都保留完整 PSRAM 录音和有界取消；未知结果不自动重发。
当前官方网关的兼容检查和收益见 [实测记录](VOICE_LATENCY_VALIDATION.md)，不能
自动推广到其他网关或设备。此上传配置沿用官方 `/tts` 和音色；播放缓冲策略另见下文。

`overlap` 表示采集与文件上传重叠，不声明云端实时识别。当前未核实到设备可调用的
流式 ASR 契约；官方主线、在线说明及云端需要补充的接口和验收条件见
[云端流式输入核验](CLOUD_STREAMING_INPUT_AUDIT_20261125.md)。

```json
"voice": {"upload_mode": "overlap", "endpoint_pause_ms": 1200}
```

`voice` 可省略；缺省为 `batch` / 1200 ms。端点接受 600～2000 ms，但 800 ms
在本机句中停顿测试中截尾，因此不作为推荐配置。直接接入示例：

```sh
python3 tools/build.py --docker --arcs-sdk-version 0.1.6 \
  --product examples/voice_companion/product.json \
  --app-dir examples/voice_companion
```

任务上下文可读取快照，或在整段连续会话结束、空闲时切换配置：

```c
#include "impo_arcs_mini.h"

impo_arcs_voice_config_t config = {
    .upload_overlap = true,
    .endpoint_pause_ms = 1200,
};
int result = impo_arcs_mini_configure_voice(&config); /* 0 accepted, -1 refused */
impo_arcs_voice_snapshot_t snapshot;
impo_arcs_mini_voice_snapshot(&snapshot);
```

运行配置不写 KV，重启恢复产品 JSON。`snapshot.upload_overlap` 属于最近一轮，
不能用来确认尚未开始的新配置。`valid_stages` 的第 n 位对应
`impo_arcs_voice_stage_t` 的第 n 项；未置位时不解读该时间。
`stage_ms` 只与同一启动内的设备单调时钟相减。`queued_bytes` 包含 WAV 头，表示
进入本地队列，不是服务端已收到；`mic_dropped_bytes` 只覆盖云端收音，不能替代
唤醒输入丢弃计数。先检查 `failed` 和完整阶段，再计算延迟。

`voice_diagnostics` 可只读查看订阅就绪状态、每轮字节/行数和回复过滤计数，以及
前一启动保留的异常记录。`complete_replies` 是解析到的有关联完成事件数，须结合
拒绝计数及回合结果判断；不等同于已经播报。`subscription_ready` 是查询时状态。
连续会话的下一轮可能覆盖实时快照，基准工具使用带回合编号的摘要，不混用两轮。
缺少关联的完整事件不会提前去重，补齐关联后仍须通过 ACK 匹配。
新增字段要求应用与本 SDK 头文件重新编译；详见
[2026-11-25实施与验证](VOICE_LATENCY_PROGRESS_20261125.md)。

首 TTS 字节、首 PCM 和驱动首次写入另有带声音代次的日志；驱动写入不代表声学
首声。测试方法见 [固定语料工具](VOICE_LATENCY_BENCH.md)。普通应用不包含
`examples/voice_latency_probe` 诊断订阅。播放接收解耦、阈值起播及完整最终文字的分段预取见 [播放阶段优化](PLAYBACK_OPTIMIZATION.md)。

### 可选的增量回复播报

`impo_arcs_mini_configure_reply(true)` 可在初始化或整段会话空闲时启用实验增量模式，
返回 0 表示接受，忙时返回 −1；`false` 恢复全文模式。重启默认全文，不写 KV，
无需复制官方订阅、TTS 或播放流程。模式独立于 `sentence_tts`；后者切分已经完成的
最终文字，增量模式消费生成期间的完整句子。

首段至少 24 个 Unicode 码点并等待 150 ms 合并窗口，后续未完成段至少 48 个码点；
最终文字到达后提交精确剩余后缀。队列有界、一次一个官方 TTS，同一声音代次保留
先前 PCM。文字修订或资源不足会明确失败，不自动重播已提交的回答。需要服务端
提供稳定的可见文字增量；有限样本的前缀一致不能替代协议保证。

`reply_stream [full|incremental]` 提供空闲时切换及查询。`voice_diagnostics` 和快照中的
`reply_probe`、`reply_segments`、`reply_mismatches` 提供无原文的诊断。增量模式下阶段 6
可能早于阶段 5，时间区间有重叠；应用需随新增头文件重新编译。
最终合并策略已完成八轮语音采样，四轮增量均分段播放、无欠载；严格首声配对仅
一组，尚不足以证明稳定收益，保持 **full 默认值**。实际提前量、早期欠载和验收
边界见[增量改造记录](INCREMENTAL_REPLY_20261125.md)及
[最终镜像语音验证](INCREMENTAL_REPLY_VALIDATION_20261125.md)。

## 更换唤醒词

配置同时控制显示文本和引擎结果过滤。声学识别仍由 AP 模型完成：修改字符串不会训练模型，也不能让“你好云宝”模型识别另一个词。

1. 获取与当前 ACOMP/AP 兼容、包含目标唤醒词的单麦模型及 wrap。关键词字符串使用引擎实际输出的 `keyword`，可同时列出拼音和中文。
2. 从真实二进制生成清单，避免手填哈希：

```sh
python3 tools/wake_bundle.py --id xiaoban-v1 \
  --label '你好小伴' \
  --keyword 'ni3 hao3 xiao3 ban4' --keyword '你好小伴' \
  --model /private/resources/cae_esr_xiaoban.bin \
  --wrap /private/resources/wrap_xiaoban.json \
  --output /path/to/my-product/wake-bundle.json
```

3. 产品配置设置 `"wake":{"enabled":true,"bundle":"wake-bundle.json"}`，路径相对产品 JSON。显示名称可以独立于唤醒词设置。
4. 资源须预先存在于设备保留的 ROMFS 根目录，文件名与清单一致。资源写入由资源供应商的适配流程处理；本 SDK 的构建及 CP 刷写流程不安装模型，也不覆盖 AP、boot 或资源分区。若设备原 ROMFS 只有另一套模型，单刷 CP 无法完成更换。

清单格式见 `resources/wake/yunbao-preserved.json`，包含标签、1–8 个结果词，以及模型和 wrap 的文件名、SHA256。启动时逐字节计算资源摘要，检查模型头和单麦 wrap，随后交给官方 ACOMP 初始化；缺失、不匹配或引擎失败时不报告唤醒可用，保留按键对话。哈希验证只确认资源身份，不能证明模型准确率、AP 兼容性或实际唤醒效果；新词仍须人工验收。

清单可公开，模型的分发遵循其授权。源码包不附带原机模型和 wrap。

### 唤醒词打断回答

产品 JSON 可设置 `"wake_interrupt":true`，默认关闭，且必须启用本地唤醒。`arcs-mini2-xiaoling` 已选择该功能：设备播放或等待回答时，说配置的唤醒词，待旧语音停止并听到提示音后再说新问题。短按主功能键仍表示退出。该流程不把任意说话当作打断。

板级音频用 Mini2 的左声道麦克风和右声道硬回采，向官方 ACOMP 提供 `mic0/ref0`；产品构建启用独立声道增益，麦克风 36 dB、回采 0 dB。前台录音从同一双声道流只提取左声道，避免把回采上传为用户语音，切换时不重启音频 DMA。其他接线必须先适配回采，不能只打开配置。

打断立即清空本地待播缓冲，语音任务取消旧 `/tts`、清理排队回答，等待约 300 ms 的 DAC 尾音后提示重新收音；旧 TTS 不能继续填入已作废的播放代次。发生打断后，订阅回复须在新录音收到确认后携带匹配的父消息标识，防止旧回答串入新一轮。实物唤醒、回声误触发和打断听感单独记录在 [验收状态](VALIDATION.md)。

## 中文字体

SDK 自带默认状态字形及可打印 ASCII 子集。新增中文标题/唤醒提示若缺字，构建会明确报错，不能静默显示空白。提供原字体即可按产品生成子集：

```sh
python3 -m pip install -r requirements-tools.txt
python3 tools/build.py --docker --product /path/to/product.json \
  --font /path/to/NotoSansSC-variable.ttf
```

字体源为 Noto Sans SC，SHA256 必须为 `a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da`。取得方式及授权见 `NOTICE`；默认产品和英文示例无需 Pillow 或源字体。屏幕当前支持 BMP 字符，不支持 emoji。`--font` 使用构建宿主的 Python，不由 Docker 容器安装 Pillow。

## 产品清单、测试和打包

```sh
python3 tools/gen_manifest.py --product products/yunbao.json
python3 tools/gen_manifest.py --product examples/button_companion/product.json \
  --commands examples/button_companion/app.c:commands
sh tests/run.sh
ARCS_BASE=/path/to/arcs-sdk-0.1.6 sh tests/run_voice.sh
python3 tools/package_sdk.py --output /path/to/output
```

自定义应用须将命令/事件的静态 C 表通过 `--commands FILE:TABLE`、`--events FILE:TABLE` 加入产品清单；可重复传入多个表。工具不能推断任意 C 程序动态生成的描述。默认固件已提供的命令由官方表生成，不能重复注册同名命令。

GitHub Actions 启用时，`sdk-source` 任务也会生成同一源码包及摘要清单，下载入口位于对应提交的 Actions 运行。

打包要求所有 SDK 更改已提交，仅导出 Git 跟踪源码，附版本、提交、依赖固定及源码摘要。ListenAI SDK、模型、AP、boot、生成固件、日志和凭据均不包含。刷写仍按 [分区核验流程](FLASHING.md) 进行。

各产品和底层版本的构建、写入、国内连接及人工听测分别记录在 [验收状态](VALIDATION.md)。通用 Mini2 的 0.1.8 镜像已通过本机两轮连续对话及静音退出短测；该结果不自动转移到其他镜像。国内线路失败时的官方回退仍保持启用。
