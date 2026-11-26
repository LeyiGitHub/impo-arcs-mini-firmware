# 内置摄像头与拍照接口

云宝、小周、小聆使用本仓库的 ARCS Mini 板级适配。摄像头启用官方
LISA Camera、DVP 和 GC0328 驱动，控制总线为 I2C0（PB6/PB7），DVP 为
PA10–PA20/PA26；PA0 保留 Mini 的原引脚复用，不将其当作 Mini3 的 PWDN。
引脚依据 [ListenAI ARCS Mini 板卡说明](https://docs2.listenai.com/arcs-sdk/latest/zh/html/boards/arcs_mini/README.html)。

摄像头通过本地 ADB 交互式 shell 使用：

```text
camera status
camera test
```

`status` 分别报告驱动、I2C、DVP 是否就绪和传感器是否已配置。
`test` 首次配置传感器，采集三帧原生 640×480 RGB565，每帧 614,400 字节，
报告尺寸、字节数、摘要和像素字节范围，然后停止采集。之后可以再次测试。
图像不上传；摘要不能代替画面内容、方向与颜色的人工验收。

没有后台采集。两帧约 1.17 MiB 的 PSRAM 缓冲在首次测试时分配，停止后由驱动
保留以供复用，重启释放。配置失败后本次启动不重复尝试，避免旧 SDK 部分分配
失败时反复占用内存。采集超时和帧格式错误均停止采集。

DVP 使用 GPDMA4，与当前音频的 1/2/3 通道分开；屏幕的 CPDMA 是另一控制器。
编译时检查音频通道冲突。保留原生 VGA，避开 0.1.6 `set_crop` 忽略传感器写入
与重新分配失败返回值的行为。

ListenAI 0.1.6 的 DVP 总线停止流程原本通过 `camera_bus` 而非所属的 `camera`
归还帧。`gadget/camera_driver.cmake` 在构建目录生成修正版，并替换该编译单元；
原 SDK 文件不变。0.1.8 保留其已经修正的实现。

SDK 0.2.3 增加官方 `camera.capture`，无参数，30 秒超时。具有拍照回调的板卡
才上报 `camera=true` 并注册该命令。返回标准
`{"ok":true,"payload":{"format":"jpeg-base64","data_base64":"...","width":320,"height":240}}`。
先丢弃三帧供曝光稳定，再将第四帧按 RGB565 高字节在前取样成 320×240。
使用匹配 ListenAI SDK 自带的 IJG 软件 JPEG 编码器，质量 60，JPEG 输出最多
48 KiB；不占用硬件 JPEG/DMA 通道。超出上限明确报错，不发送半张图片。

云端拍照在独立任务中执行，不阻塞 Noise 会话。请求参数深拷贝，原 request_id
和 session_generation 跟随结果；官方 Noise 队列负责丢弃旧会话回执。
同一时刻只有一个云端拍照任务，本地测试与云端采集另由板级锁互斥。
控制台仅输出拍照结果尺寸和编码长度，省略图像内容；正式云端回传不省略。
`display.show_image` 仍用于显示下载图片，和拍照是两个接口。

验证命令（`ARCS_BASE` 指向匹配的 SDK）：

```sh
sh tests/run_camera_control.sh
sh tests/run_camera_cloud.sh
# 以下回归需要本机 CMake；以 0.1.6 SDK 的实际总线源码验证帧归属：
python3 tests/test_camera_driver_compat.py
```

完整 host 回归继续使用 `ARCS_BASE=... sh tests/run.sh`。构建、烧录独立读回、
传感器识别、采集成功和人工画面验收分别记录。

## 2026-11-26 云宝实机记录

- 当前实物通过 Recovery 核验为 `BOOT-f17c0a1a033ed4ad`，原固件 0.1.0，
  AP 0.1.6、唤醒“你好云宝”。两份 16 MiB 备份一致，Boot/AP/资源与此前
  已验收的云宝备份一致，原 CP 与国内中继版本的独立读回一致。
- 仅发送一次 CP 写入：SDK 0.2.2，2,500,164 字节，SHA256
  `83270747ce20e36373ac45d93333d5cf5d86d7c22e73b401f50d037c105aa990`。
  独立 CP 读回逐字节一致，CP 之外全部闪存字节不变。
- 实机 `camera status` 确认 camera/I2C0/DVP0 均 ready。GC0328 的传感器
  PID 为 `0x009D`、I2C 地址 `0x21`；两轮 `camera test` 各得到三帧
  640×480 RGB565、每帧 614,400 字节，帧数据非均一，六帧签名均不同，
  每轮停止均返回 0。
- 两轮前后及后续健康检查的 `boot_id` 均为 `ba0973a420f8333a`，没有
  堆断言或异常标记；`paired=1`、Link online、唤醒监听/待机正常。
  这次短测不能证明此前 TLSF 间歇性堆故障已修复，也没有验收画面内容与方向。
- 完整 host 回归、真实 SDK 0.1.6/0.1.8 头文件下的采集生命周期测试，以及
  使用 0.1.6 实际 DVP 总线源码的普通/Ping-Pong 帧归属回归通过；两种 AP
  配套固件交叉构建与镜像检查通过。小周、小聆尚未刷入本次摄像头版本。

初次构建期间，并行 host 测试准备重新生成了公共云宝产品头，实际镜像的
`product_id` 为 null，与当时构建描述中的开发产品 UUID 不一致。随后串行按
公共云宝产品重新构建，所得 CP 与已安装镜像逐字节一致，构建描述已校正，
无需再次写入。`tools/build.py` 现在核对构建前后的产品配置摘要，发生此类
覆盖时停止生成成功描述。云端产品绑定仍单独验收，不将本地采集记作云端拍照。

私有证据位于工作区外层 `outputs/camera-enable-20261126/`，包括双备份、
独立读回、安装账本、`runtime-result.private.json`、
`post-camera-check.private.json`、完整 host 和两版交叉构建日志。

标准 Mini 引脚依据也可查阅[聆思板卡文档](https://docs2.listenai.com/arcs-sdk/latest/zh/html/boards/arcs_mini/README.html)。

## 2026-11-26 云宝云端拍照验证

- 保留 ListenAI 0.1.6、原唤醒资源与配对，仅一次写入 CP。SDK 0.2.3 镜像
  2,570,364 字节，SHA256
  `f9d263b9d9965a436125ecc595ec9928ee75a50b2d855ab5a45b3f79e9d436fd`。
  两份 16 MiB 备份一致，独立 CP 读回完全一致，所有非 CP 字节不变。
- 完整 host 回归、默认云宝和 button_companion 的 ListenAI 0.1.6 交叉构建通过；
  摄像头生命周期、真实 IJG 编码和独立 stb 解码、异步参数归属测试在两版 SDK
  头文件/依赖下通过。本轮没有把 0.1.8 新固件刷到小周或小聆。
- Impo 云宝设备页实际显示 0.2.3 和 camera。通过云宝原对话提交一次自然语言
  拍照请求，收到“云宝拍的照片.jpg”，实际打开预览成功，云端描述与图中浅色
  横带、暗部和细线相符。这证明采集、JPEG、回传与模型看图链路可用；图像
  大部分是近处物体与暗部，不能据此判断办公室全景，也未做人工方向/颜色标定。
- 额外一次本地 camera.capture 完成约 730 ms（不含云端或语音），base64 长度
  5,392，GC0328 PID 009D / I2C 21；其后 90 秒启动编号未变，停止后再次读取
  驱动状态为 configured=1、setup_result=0。独立的 125 秒空闲对照也未重启。
- 此前观察到启动编号从 cda91f81a481f23d 到 ae22f8355c4c9136，再到
  d6ce497ed34d2e1a，期间 ADB 流断开，读取的 reset_record valid=0。
  用户已确认没有拔插或重启，应按异常重启处理。未取得有效异常现场，尚未
  证明拍照导致重启，也未证明重启问题已修复。当前转串口未接设备，待接到云宝
  UART0 TX（PA3）和 GND 后，以 921600 只接收日志，保留重启前现场。功能通过不等于长期稳定性通过，语音说话到拍照的物理验收
  也不能由本轮 App 文本请求代替。

私有镜像、双备份、写入/读回账本、原始日志和对照结果保存于工作区外层
`outputs/camera-cloud-20261126/`。没有提交、推送或发布生产产品清单。
