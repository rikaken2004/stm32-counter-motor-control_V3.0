import serial
import time

PORT = "/dev/ttyUSB0"
BAUD = 115200

log_name = time.strftime("stm32_log_%Y%m%d_%H%M%S.txt")

with serial.Serial(PORT, BAUD, timeout=1) as ser, open(log_name, "a", encoding="utf-8") as f:
    print(f"Listening on {PORT}, baud={BAUD}")
    print(f"Log file: {log_name}")

    while True:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        if not line:
            continue

        ts = time.strftime("%Y-%m-%d %H:%M:%S")
        text = f"{ts} {line}"

        print(text)
        f.write(text + "\n")
        f.flush()
