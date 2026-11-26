# 小聆堆损坏调查（2026-11-26）

故障尚未修复。此文记录诊断证据与候选固件，不能用作稳定版本或三设备升级完成的证明。

## 目标与边界

- 小聆 Recovery 唯一编号 `BOOT-F17C0A1A5628D2AD`，节点 `arcsmini2-d22856`，保留 ListenAI AP 0.1.8、原“小聆小聆”单麦唤醒资源和配对。
- 运行态 USB 固定串号 `FFBBCCDDEE001122` 不作为唯一身份。每次写入前重新核验 Recovery、当前 CP 与两份全片备份；只写 `0x600000..0xA00000`。
- 原始日志、全片备份、精确 ELF、镜像及操作账本在工作区外层 `outputs/companion-capabilities-20261125/`，均为私有证据，不提交。
- 保留既有语音延迟及 companion 修改；没有提交、推送或 production 发布。

## 已确认的故障

1. 原 0.1.0 与小周 0.2.1 的健康查询也出现过重启，不能先归因新增图片。
2. 异常回调中的 `xTaskGetTickCountFromISR` 误用已由 patch 0028 修正，仅恢复原始故障记录，不修复堆损坏。
3. 小聆的 `display_status` / cJSON 分配发现 TLSF `block must be free` 断言；当时命令任务栈余量充足。发现点不是破坏点。
4. comprehensive poisoning 版本在 6 次图片后、连续健康查询第 39 次期间重启。现场为 `shell_task_flush_tx` → `exram_free` → `verify_allocated_region`；头 canary 正确，分配长度却变成堆内指针，导致诊断按错误地址读取尾 canary。
5. 私有 head-first 检查版先通过 50 次健康、6 次图片、200 次健康；随后 PNG/JPEG 与健康/显示状态交替的混合测试在第 8 次图片期间重启，前 7 次为 ready。仍为 TLSF 空闲链表断言，证明此前长健康批次不能代替混合稳定性验收。

分配长度被堆内指针覆盖，与 TLSF 更新空闲链表指针时可能写入的位置吻合；这是待验证线索，不是根因结论。没有证据证明图片解码器、USB DMA、newlib 竞态或 SPI 警告是破坏者。

## 元数据检查诊断版

在私有单文件挂载的 `multi_heap.c` 中，对 malloc/free/realloc/aligned allocation 的 TLSF 操作前后检查：

- 物理块大小、边界、相邻块空闲状态和回指；
- 空闲链表指针范围、反向链接、尺寸索引、位图及块计数；
- 有界遍历，避免诊断继续追随损坏指针；
- 最近 32 条操作的阶段、堆、指针、长度、调用地址和任务指针。记录放 PSRAM，只在检测到错误时通过底层 printk 输出。

检查器不验证图片内容，也不把常规 comprehensive poisoning 中尚未写好 canary 的中间分配误判为坏块。原 ListenAI SDK 源文件与常规产品配置未修改。

主机检查通过 30,000 次随机分配/释放/重分配及人工损坏长度、链表指针、位图和块状态测试，使用 ASan/UBSan。主机为 64 位，因上游 TLSF 固定 4 字节对齐而关闭 alignment sanitizer；这不代替 32 位目标机验证。

构建首次出现生成归档 `libipclsf.a` 为 0 字节；仅删除该生成产物后增量构建通过。镜像检查覆盖归档、设备注册、唤醒/对话、PSRAM 声明和 CP 分区大小。

候选：`xiaoling-022-metadiag-candidate/`，SDK 0.2.2 / ListenAI 0.1.8，2,509,972 字节，SHA256 `148b6a8e41364448b2d1f0bc86d81997a9d25e0a4871ead05cd80b95f473d995`。SRAM 153,248 字节，与上一诊断版一致。

本次写入前两份 16 MiB 备份一致，当前 CP 与 headguard 候选一致，Boot/AP/唤醒资源与已核验的小聆备份一致。仅发送一次 CP 写入；独立 CP 读回完全一致，所有非 CP 字节不变。

全堆扫描版本启动日志到网络和唤醒初始化后停止，随后返回 Recovery。Recovery 实际读回 `reason: 5, handshake_timeout: 0`，即 Boot watchdog timeout；没有获得新的 HEAPDIAG 或堆断言。再次独立读回的 CP 与该候选完全一致。这属于诊断候选的启动失败，不能归并为原 TLSF 故障复现，也不能仅凭现象确定是扫描开销或报告重入。初版诊断源已按构建指纹归档。

因此另建 `xiaoling-022-linkdiag-candidate/`：默认只在每次空闲链表 insert/remove 写指针前检查当前节点、邻居的边界/空闲状态/回指和索引，保留操作与 ISR 标志。全堆扫描通过私有 ADB shell 的 `heapdiag full` 在启动后按需开启；`heapdiag light` 恢复局部检查。错误报告加入重入保护。该命令仅用于诊断，不是产品云端能力。

局部检查钩子也通过 30,000 次随机操作和注入坏元数据的 ASan/UBSan 主机验证；交叉构建与镜像检查通过。候选 2,511,580 字节，SHA256 `0d8194f0979250c44d1b5520fedf06a4f96ac06ec8c395c75ee8705dbdb7c12f`。在 Recovery 再次核验唯一编号、当前 metadiag CP、AP/唤醒资源并完成两份一致的全片备份，仅发送一次 CP 写入，独立读回一致，所有非 CP 字节不变。

首次普通 reboot 仍只进入原 Boot 1.1.9 的 Recovery，UART 只有 `boot reason: boot wdt timeout`，第二版 CP 当时尚未实际启动。查阅官方 Recovery 实现后，通过其整机 POR 复位路径清除暂存的启动状态；该操作不写闪存分区。随后实际启动成功，UART/ADB 核对 SDK 0.2.2、ListenAI 0.1.8 与原唤醒词；显示状态返回同一次启动编号。启动采集没有 HEAPDIAG、堆断言或异常。混合测试结果待本轮采集后补充。

局部检查运行时，首轮 8 次图片 ready，第 9 次遇到日志 JSON 解析错误停止；第二轮另有 6 次 ready，第 7 次停止于显示状态回复采集超时。UART 没有堆诊断/异常。两轮之后都单独查询到同一 `boot_id`，故这两次脚本停止不计为设备重启。图片请求不自动重发；只读查询解析改为分别处理 UART/ADB 的完整 JSON 对象，并记录有界只读查询重试。

在已运行的同一固件上启用 `heapdiag full`，第一次只读查询停止后成功恢复 light 模式，设备未重启；第二次启用后，在第一张 PNG 下载期间再次出现 Boot watchdog timeout，仍无 HEAPDIAG/原始堆断言。此时所有分配都在临界区内做全堆扫描，会改变执行时间与 RTOS tick/调度时序；结合两次 Boot watchdog 现象，后续停用这种高频全堆扫描，不能将其结果并入原故障复现。局部链表检查继续保留。

## 2026-11-26 续接烧录与基本检查

已安装 `xiaoling-022-tracediag-candidate/`：SDK 0.2.2 / ListenAI 0.1.8，2,510,444 字节，SHA256 `034f047d4da19af60a4aa48d2e2f080206e6bdca343a0acbf86f6946e796808c`。该私有诊断版保留局部链表检查、最近 32 条分配操作及异常时的有界输出和重入保护，停用高频全堆扫描。镜像、精确 ELF、诊断源指纹和现有构建完整性检查重新核对通过；这不是堆故障修复版。

续接时设备仍为 `BOOT-F17C0A1A5628D2AD`。重新读取的两份完整 16 MiB 备份一致，并与上一会话的两份备份完全相同；当前 CP 为已记录的 linkdiag 候选，AP/原唤醒资源保持一致。只发送一次 CP 写入，独立 CP 读回逐字节一致，完整闪存读回确认全部非 CP 字节不变。

普通 `reboot` 后实际启动成功，接收专用 UART 为 `/dev/cu.usbserial-AF67X27H`、921600，DTR/RTS 预置关闭。ADB 确认 SDK 0.2.2、ListenAI 0.1.8 和“小聆小聆”；启动日志没有堆诊断错误、断言或异常。日志中的 `Boot AP watchdog handoff` 是正常交接记录，不计为 watchdog 故障。

随后各发送一次 PNG/JPEG 请求，分别达到 ready（100×100 / 239×178）。下载期间交替进行共 10 次健康查询及显示状态查询，全部正常；启动编号一直为 `d3e8784b99b1ce80`，UART 没有故障标记。该结果仅证明本轮短时基本检查通过，不能替代混合压力回归或面板实物验收。本轮 `link` 控制台查询未获得可解析的连接状态，不据此宣称正式 Impo 云端复验通过。

本地证据为 `xiaoling-022-tracediag-resume-preflight.private.json`、`-install.private.json`、`-boot-result.private.json` 和 `xiaoling-022-tracediag-smoke-1-result.private.json` 及对应原始日志。采集进程完成后关闭 UART 和 ADB shell。

## 后续验收

首次坏状态若出现在 TLSF 操作前，应继续追查操作之间的写入；若操作前正常、操作后异常，应围绕该次分配器操作及中断并发缩小范围。仍需区分首次发现与实际破坏。

修复后必须回归混合场景，并恢复合适的正常固件。正式 Impo 图片调用、产品/事件绑定、面板方向和完整性、声音听感分别记录。当前只确认用户曾看到狼图；方向和完整性未确认。云宝未完成本轮刷写，三台升级未全部完成。
