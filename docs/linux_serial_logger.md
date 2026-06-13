# Ubuntu Linux 串口日志采集

## 功能目标

STM32 通过 USART3 (PB10) 输出 `STATE=...` 状态行 → CH340 USB 转串口模块 → Ubuntu Linux → Python 程序添加时间戳 → 保存日志文件。

## 接线

```
STM32 PB10 (USART3_TX) → CH340 RXD
STM32 GND              → CH340 GND
```

CH340 USB 端插入电脑/虚拟机。只单向接收时不需要接 CH340 TXD。

## 工具安装

```bash
sudo apt update
sudo apt install python3-serial minicom -y
```

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

## minicom 验证

在写 Python 程序前，先用 minicom 确认能收到数据：

```bash
sudo minicom -D /dev/ttyUSB0 -b 115200
```

看到 `STATE=IDLE,TARGET=0,CURRENT=0,TOTAL=5,MOTOR=OFF` 等输出后，按 `Ctrl+A` 再按 `X` 退出。

## serial_logger.py

代码位置：[../linux/serial_logger/serial_logger.py](../linux/serial_logger/serial_logger.py)

核心逻辑：

```python
with serial.Serial(PORT, BAUD, timeout=1) as ser, open(log_name, "a", encoding="utf-8") as f:
    while True:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        ts = time.strftime("%Y-%m-%d %H:%M:%S")
        text = f"{ts} {line}"
        print(text)
        f.write(text + "\n")
        f.flush()
```

## 运行方式

```bash
cd linux/serial_logger
python3 serial_logger.py
```

成功输出：

```
Listening on /dev/ttyUSB0, baud=115200
Log file: stm32_log_20260611_010702.txt
2026-06-11 01:07:02 STATE=IDLE,TARGET=3,CURRENT=0,TOTAL=0,MOTOR=OFF
```

按 `Ctrl+C` 停止。

## 日志文件命名

```
stm32_log_YYYYMMDD_HHMMSS.txt
```

每次运行生成新文件，不覆盖旧日志。

## 常见问题

| 问题 | 现象 | 根因 | 解决 |
|------|------|------|------|
| Ubuntu apt update 卡住 | 软件包无法下载 | VMware NAT/DHCP 服务异常 | 重启 VMware NAT/DHCP 服务 |
| 虚拟机无法复制粘贴 | 外部文本粘不进去 | 缺少 open-vm-tools | `sudo apt install open-vm-tools open-vm-tools-desktop -y` |
| 找不到串口设备 | 没有 /dev/ttyUSB0 | CH340 未连接虚拟机 | VMware → 可移动设备 → 将 CH340 连接到虚拟机 |
| Ctrl+C 后显示异常 | KeyboardInterrupt | 正常手动停止 | 这是正常行为，不是报错 |
| 日志文件权限 | Permission denied | 文件属于 root | `sudo chown $USER:$USER stm32_log_*.txt` |

## 项目价值

从本地串口调试升级为 **Linux 上位机日志系统**。体现的能力：

- Linux 基础命令
- 串口通信概念（/dev/ttyUSB0、CH340、115200 8N1）
- Python pyserial 编程
- 时间戳日志采集
- 虚拟机设备连接
