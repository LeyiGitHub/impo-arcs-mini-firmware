# ARCS Mini companion 能力与验收

SDK 0.2.3 的默认应用复用官方 BLE、Wi-Fi、Noise、命令和语音路径，按产品 JSON 保留各设备的名称、唤醒资源与聆思 AP 版本。内置摄像头通过标准 camera.capture 按需拍照并回传 JPEG。

## 21 条命令

| 命令 | 行为与限制 |
| --- | --- |
| `device.health` / `device.set_name` | 健康查询、设备名称；不把未校准的电池曲线当作电量 |
| `speaker.say` / `speaker.play_url` / `speaker.beep` | 朗读、公开 HTTPS 音频、短鸣；返回 `boot_id` 与 `playback_id` |
| `speaker.set_volume` / `speaker.stop` / `speaker.status` | 保留音量设置；指定双编号可只停止那次仍在进行的播放；`idle` 不能证明完成 |
| `speaker.playback_status` | 按双编号查询 fetching / playing / completed / interrupted / failed；最近 8 次声音，重启丢失 |
| `speaker.play_effect` | correct / wrong / chime / applause / bird，420–700 ms 本地合成声音；忙碌时拒绝，不延后补播 |
| `display.show_animation` | 清除表情和文字，恢复自动状态画面 |
| `display.set_expression` | happy / sad / surprised / sleepy / curious / wink，以及 auto；持续 1–300 秒 |
| `display.show_text` | 三行、每行 216 像素；约 27 个中文或 39 个 ASCII 字符，最多 192 UTF-8 字节；空文字清除；拒绝超出字库或排版边界的内容 |
| `display.show_image` | 公开 HTTPS 的静态 JPEG/PNG，文件不超过 256 KiB、源图不超过 512×512；等比例居中适配 240×240，透明区域与留边为黑色；1–300 秒，默认 30 秒，从解码就绪起计时 |
| `camera.capture` | 按需采集一张照片，独立任务回传 320×240 JPEG，最长 30 秒；需打开图片后再描述画面 |
| `display.status` | 返回请求中的显示内容，不是面板物理读回；语音、离线、警告画面优先 |
| `timer.start` / `timer.status` / `timer.cancel` | 一个本地倒计时，1 秒至 24 小时；断网继续、重启丢失；取消必须匹配 `boot_id` 和 `timer_id` |
| `button.configure` / `button.status` | 临时 event 答题模式 1–600 秒；短按发送累计计数，不启动语音；到期/重启恢复 voice；长按电源策略继续有效 |

`completed` 的依据是软件输出排空后再等 200 ms DMA 余量。它不证明功放、扬声器或人耳已经听到；物理听测单独记录。终态不可被之后的 stop 改写。旧启动或已淘汰的编号只能判为未知。

计时到点只在扬声器及语音空闲时尝试一次本地 chime，忙碌则记 `skipped_busy`，不打断对话也不稍后补响。`alarm=submitted` 只表示已提交；应继续查询对应播放回执。不会自动朗读计时标签。

## 图片显示

```json
{"url":"https://httpbin.org/image/png","duration_seconds":30}
```

`display.show_image` 异步返回 `boot_id`、`image_id` 与 `state: accepted`，只表示请求已接收。随后读取 `display.status.image`，核对同一 `boot_id` / `image_id` 的 `fetching` / `ready` / `failed`；就绪后返回源图尺寸与剩余秒数。`ready` 表示已解码，不是面板物理读回。到期为 `expired`，屏幕恢复自动状态；重启丢失图片。文字、表情以及 `display.show_animation` 会清除图片并作废尚未完成的旧请求；底层下载完成前会拒绝另一个图片下载。

只有屏幕任务写面板。图片仅在空闲画面显示，语音、断网与警告优先。下载沿用现有校验证书的 HTTPS 客户端，最多三次 HTTPS 重定向，不支持鉴权 URL、GIF、WebP 或动画。解码器固定到 `sdk/third_party/README.md` 记录的 stb_image 提交，仅启用 JPEG/PNG，限制尺寸和活跃分配量；大缓冲进入 PSRAM。下载失败不会自动重试或延后补发。

## 5 类事件与防循环

事件：`playback_completed`、`playback_interrupted`、`playback_failed`、`timer_fired`、`button_pressed`。

0.2.1 中，`speaker.say/play_url/beep/play_effect` 与 `timer.start` 的 `notify_event` **默认 false**。回执始终保留，可直接查询；只有正在运行的接力编排或明确需要通知的游戏才显式传 true。倒计时的本地提示音不发送播放事件。答题按键仅在用户启用的临时 event 模式发送事件。

这些事件是尽力通知，可能延迟、合并或因有界队列而丢失。用状态查询和累计计数核验，不能按收到的事件条数判断按键次数或播放次数。产品说明明确禁止收到事件后再次自动朗读。测试发现 0.2.0 在产品清单尚未生效时，播放事件会触发云端空回复朗读，形成循环；0.2.1 通过显式订阅避免普通播放进入该循环。

设备只报告本机播放被中断，不能直接取消别的会话安排的后续故事。编排者必须作废自己尚未派发的句子；没有实际取消证据时不能宣称全局停止。

## 产品清单与场景

`products/companion-instructions.txt` 与 `products/companion-scenarios.json` 定义通用说明和五个场景：三机故事接力、文字/按键答题、刷牙/学习倒计时、表情/短提醒、猜声音/鼓励音效。场景不增加固件能力，也不代表已做跨会话的全局调度服务。

`manifests/companion.manifest.json` 是待审核的共享清单；产品 UUID 在各自产品配置中，公共产品示例仍默认 `product_id: null`，其他使用者须替换为自己账户的产品 ID。设备名称、唤醒词不写死在共享清单中。生成：

```sh
python3 tools/gen_manifest.py --instructions products/companion-instructions.txt
```

自定义 `--app-dir` 替换默认 companion，不会自动拥有这 10 条扩展命令。生成这类清单时使用 `--no-companion --commands <file>:<table>`，并按需提供自己的事件表。

编译只生成固件，不会发布云端清单。清单只写 dev；production 发布需独立确认。必须从 Impo 实际目录验证产品、命令与事件，不能用 API 保存成功或固件声明产品 ID 代替运行时绑定证据。

## 2026-11-25 验收记录

- 完整 host 回归在 ListenAI 0.1.8 依赖下通过，覆盖离线计时、忙碌跳过、编号校验、终态不变、默认事件静默及显式订阅、按键归属/到期、文字字库/布局和真实音频工作线程排空余量；之前 0.1.6 下完整回归也通过。
- 0.2.1 默认云宝 ListenAI 0.1.6、小周 ListenAI 0.1.8、自定义 button_companion ListenAI 0.1.6 均交叉编译并通过镜像完整性检查；小聆 ListenAI 0.1.8 也已编译。
- 小周 0.2.1 已仅写 CP，独立读回逐字节一致，CP 之外所有字节不变。原 AP、唤醒词和配对保留。实际测试通过中文卡片、表情、计时到期/取消、合成音效、精确完成/中断回执、按键模式自动恢复、音量 20% 保持。实物按键及听感单独等待人工确认。
- 0.2.0 测试期间出现用户未碰 USB/电源的重启，没有有效保留异常记录。0.2.1 的功能测试通过，但只读健康查询稳定性观察也重启。回退原始 0.1.0 做对照，连续健康查询成功 4 次后同样断开并重启，因此不能归因于新增倒计时。固定 12 KB 命令任务仍在 7 次健康查询后断开，栈余量约 10 KB，该猜测被排除，相关任务改写未保留。版本信息连续 30 次、扬声器小型状态连续 40 次通过；精简控制台日志、仍执行健康查询及 JSON 序列化时也断开。故障位置尚未确认，等待小周 UART 诊断，不能声称稳定性通过。
- 云端已分别保存最低固件 0.2.0、0.2.1 的 dev 清单，均为 19 命令/5 事件/5 场景。0.2.1 清单已独立 GET 逐字段核对。小周目录确认过 0.2.0 的 19 条命令，但当时产品为 null、事件为空；0.2.1 实际产品绑定仍待核验。生产清单未发布。
- 小周为 0.2.1，含仅用于定位故障的控制台 `gadget_output compact|full` 开关，重启默认 full，不改变正式云端命令和结果。云宝尚未写入本轮新固件；小聆的 0.2.2 更新见下文。三台升级尚未全部完成。人工未留意此前听测，听感仍记为未确认。
- UART 接入后两次被动采集均读到小周芯片 `f17c0a1a322ed2ad` 的 boot 1.1.9 启动日志，停在屏幕初始化约 260 ms；当时 USB 未重新枚举。尚未发送 UART 命令，也未再次触发健康查询。当前先核实供电和三线连接，不能将此启动状态认定为原健康查询故障的原因。
- 后续只读 UART 版本查询无响应；断开转接器 TX、再完全断开串口后，人工仍报告黑屏，但能进入同芯片 ADB 更新模式。该模式下完整 16 MB 闪存读回与上次刷写后备份逐字节一致，CP 和所有非 CP 区域均未变化，恢复原因 2、握手超时 0。没有再次写入固件；从更新模式执行普通 `reboot` 后恢复 0.2.1 应用，唤醒及扬声器状态正常。冷启动黑屏原因仍未确定，与健康查询故障分别保留。

## 小聆 0.2.2 图片能力更新

- USB Recovery 唯一编号 `BOOT-F17C0A1A5628D2AD`，原应用 0.1.0、ListenAI 0.1.8、唤醒“小聆小聆”。两份完整 16 MiB 备份一致；从本次 ROMFS 实际提取模型与 wrap 的摘要，与小聆构建描述逐项吻合。没有用历史整片备份代替本次设备核验。
- 完整 host 回归及图像 AddressSanitizer/UndefinedBehaviorSanitizer 测试通过，覆盖真实 JPEG/PNG、透明黑底、等比例留边、512 像素边界、超限拒绝、过期、取消后旧结果失效、重定向限制、网络与任务分配失败。0.1.8 小聆默认应用、0.1.6 云宝默认应用、0.1.6 button_companion 均构建及镜像检查通过。
- 仅发送一次 CP 写入；2,507,556 字节镜像独立读回一致，CP 以外所有字节不变。实际启动 0.2.2，保持原节点 `arcsmini2-d22856`、配对、唤醒词与 20% 音量，网络在线，屏幕驱动 ready、error=0。
- PNG 实机下载解码为 ready（100×100），并读到正常 expired；JPEG 单独下载也到 ready（239×178），屏幕驱动无错误。此前 JPEG 下载中轮询状态时出现一次 USB 断开并重启；尚未定位，不能声称图片功能稳定性验收通过。人工屏幕确认及串口故障日志仍待补充。
- 0.2.2 的 dev 清单 revision 1（20 命令、5 事件、5 场景）已独立 GET 逐字段核对。首次 PUT 响应不完整，后续 GET 证实已保存，因此没有重发。Impo 桌面端正式目录读取确认小聆在线、版本 0.2.2、20 命令且包含 `display.show_image`；当时 product 为 null、events 为空，未返回 manifest channel/revision。生产清单未发布。
- 通过 Impo 正式 Gadget 通道执行一次 `display.show_image`，收到本次 `accepted` 回执；之后单独执行一次 `display.status`，同一图片编号返回 `ready`、100×100、剩余 217 秒、download_busy=false。没有用 ADB 结果代替这两次云端调用。仍等待实物屏幕确认，且一次重启的诊断未完成。
- 已追加并补刷 `display.status.image.boot_id`，用于与图片回执的启动编号同时核对，防止重启后复用 image_id 造成误判。补刷前再次完成两份一致的全片备份；2,507,652 字节镜像独立读回一致，所有非 CP 字节不变。PNG 实机返回同一 boot_id/image_id 的 accepted 与 ready。dev revision 2 已独立 GET 逐字段核对，生产清单仍未发布。
- 用户接好仅 RX/GND 的 UART 后，921600 接收日志有效。一次 JPEG 加状态查询完成，启动编号不变；10 次健康查询返回正常。随后 PNG/JPEG/PNG 连续切换，前两次 ready，第三次下载中重启，UART 留下 TLSF `block must be free` 断言，后续又发生异常处理器嵌套断言。已确认异常记录回调误用了仅适用于中断的 `xTaskGetTickCountFromISR`；修正回调以保留后续原始现场，尚不能把该诊断修正称为堆故障修复。
- 用户确认曾看到一只狼的图片，与 JPEG 测试图一致；尚未确认画面完整性及方向。UART 同时出现 SPI 未发完数据警告，驱动返回 error=0 不能代替实物显示验收。
- 异常回调修正已完成 host 回归、三种交叉构建及小聆 CP 独立读回，非 CP 不变。再次复现时不再出现嵌套异常；符号化现场为 `display_status` → `cJSON_AddObjectToObject` → 24 字节分配 → TLSF 空闲链表断言，`gadget_sh` 栈最高使用约 27%。这是发现损坏的位置，不等于实际破坏内存的位置。实机相同 PNG/JPEG 在主机交替解码 200 次，ASan/UBSan 未检出异常；另建 comprehensive heap poisoning 诊断构建以继续定位。
- comprehensive heap poisoning 私有诊断版已仅写 CP 并独立读回，非 CP 不变。6 次 PNG/JPEG 交替切换均在同一启动编号下到 ready；随后健康查询成功 38 次，第 39 次期间重启。完整现场为 `shell_task_flush_tx` → `exram_free` → `heap_caps_free` → `verify_allocated_region`，读取已损坏的分配长度后发生总线访问异常；命令行任务栈最高使用约 30%。尚不能确认最初损坏来自 ADB、图片或其他并发任务。
- 为避免诊断代码按损坏的长度访问尾部，另建私有 head-first/bounded-size 检查版，通过单文件只读挂载构建，原 ListenAI SDK 源码及常规产品配置未修改。小聆再次双备份一致后，仅写 2,507,988 字节 CP（SHA256 `6088338050697ed6ef2a748f28012be8d30c608544939cf83975ebd94e800bae`），独立读回一致且所有非 CP 字节不变。首轮 50 次健康查询及随后 6 次图片切换通过，继续复现观察；该变更仍属于诊断，不能称为堆故障修复。

日志、开发者密钥、设备备份、镜像和逐台刷写账本仅保存在仓库外私有目录，不随源码提交。以上是分项证据，不把构建、读回、云端登记、软件完成回执和人工听测合并成同一个“已验收”。

开发者流程依据 [Impo 官方 Gadget developer skill](https://raw.githubusercontent.com/impoai/impo-developer-skills/main/skills/impo-gadget-developer/SKILL.md)。
