# Linux 串口日志采集器

## 功能

STM32 通过 USART3 (PB10) 周期性输出 `STATE=...` 状态行，经 CH340 USB 转串口模块接入 Ubuntu Linux，Python 程序自动添加时间戳并保存日志文件。

## 安装依赖

```bash
sudo apt update
sudo apt install python3-serial minicom -y
```

## 连接方式

```
STM32 PB10 (USART3_TX) → CH340 RXD
STM32 GND              → CH340 GND
```

CH340 USB 端插入电脑/虚拟机。

## 设备识别

```bash
lsusb                          # 确认 CH340 已识别
ls /dev/ttyUSB*                # 查看串口设备名
```

正常输出示例：

```
QinHeng Electronics CH340 serial converter
/dev/ttyUSB0
```

## 先验证通信

使用 minicom 确认能收到 STM32 数据：

```bash
sudo minicom -D /dev/ttyUSB0 -b 115200
```

看到 `STATE=IDLE,...` 等输出后，按 `Ctrl+A` 再按 `X` 退出。

## 运行日志采集

```bash
cd linux/serial_logger
python3 serial_logger.py
```

输出示例：

```
Listening on /dev/ttyUSB0, baud=115200
Log file: stm32_log_20260611_010702.txt
2026-06-11 01:07:02 STATE=IDLE,TARGET=3,CURRENT=0,TOTAL=0,MOTOR=OFF
2026-06-11 01:07:03 STATE=IDLE,TARGET=3,CURRENT=0,TOTAL=0,MOTOR=OFF
2026-06-11 01:07:03 KEY ADD target=3
```

按 `Ctrl+C` 停止。

## 日志文件命名规则

```
stm32_log_YYYYMMDD_HHMMSS.txt
```

每次运行生成新文件，不会覆盖旧日志。

## 常见问题

| 问题 | 排查 |
|------|------|
| 找不到 /dev/ttyUSB0 | VMware → 可移动设备 → 将 CH340 连接到虚拟机 |
| Permission denied | `sudo` 运行，或 `sudo chmod 666 /dev/ttyUSB0` |
| 日志文件权限 | `sudo chown $USER:$USER stm32_log_*.txt` |
| VMware 无法复制粘贴 | `sudo apt install open-vm-tools open-vm-tools-desktop -y` |
| KeyboardInterrupt | 这是 Ctrl+C 正常停止，不是报错 |
