import serial
import serial.tools.list_ports
import threading
import numpy as np
import matplotlib
matplotlib.use('TkAgg')  # 设置交互式后端
import matplotlib.pyplot as plt
import re
import sys

# ================= 配置 =================
ROWS = 24
COLS = 32
BAUDRATE = 115200

row_pattern = re.compile(r"Row\s+(\d+):(.+)$")
ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')

# ================= 串口选择 =================
def select_serial_port():
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("No serial ports found.")
        sys.exit(1)

    print("Available ports:")
    for i, p in enumerate(ports):
        print(f"{i}: {p.device}")

    while True:
        try:
            user_input = input("Select port (enter index or device name): ").strip()
            if user_input.isdigit():
                idx = int(user_input)
                if 0 <= idx < len(ports):
                    return ports[idx].device
                else:
                    print("Invalid index. Try again.")
            else:
                # 检查是否是设备名
                for p in ports:
                    if p.device == user_input:
                        return user_input
                print("Device not found. Try again.")
        except KeyboardInterrupt:
            sys.exit(0)


# ================= 热图显示类 =================
class HeatmapViewer:
    def __init__(self):
        self.frame = np.zeros((ROWS, COLS), dtype=np.float32)
        self.current_rows = {}

        plt.ion()
        self.fig, self.ax = plt.subplots()
        self.im = self.ax.imshow(
            self.frame,
            cmap="inferno",
            origin="upper",
            interpolation="nearest",
        )
        self.colorbar = plt.colorbar(self.im, ax=self.ax)
        self.ax.set_title("MLX90640 Heatmap (24x32)")
        self.ax.set_xlabel("X")
        self.ax.set_ylabel("Y")

    def update_row(self, row_idx, values):
        if len(values) != COLS:
            print(f"Warning: Row {row_idx} has {len(values)} values, expected {COLS}")
            return
        self.current_rows[row_idx] = values

        # 如果收到完整 24 行 → 更新整帧
        if len(self.current_rows) == ROWS:
            for r in range(ROWS):
                self.frame[r, :] = self.current_rows[r]

            self.current_rows.clear()
            self.refresh_plot()

    def refresh_plot(self):
        vmin = np.min(self.frame)
        vmax = np.max(self.frame)
        if vmax - vmin < 0.1:
            vmax = vmin + 0.1

        self.im.set_data(self.frame)
        self.im.set_clim(vmin, vmax)
        self.colorbar.update_normal(self.im)
        self.fig.canvas.draw()
        self.fig.canvas.flush_events()


# ================= 串口线程 =================
def serial_thread(ser, viewer):
    print("Listening serial data...")
    while True:
        try:
            line = ser.readline().decode(errors="ignore").strip()
            if not line:
                continue

            # 去除ANSI转义序列
            line = ansi_escape.sub('', line)

            match = row_pattern.search(line)
            if match:
                row = int(match.group(1))
                try:
                    # 使用正则提取所有浮点数，避免非数字干扰
                    values_str = re.findall(r'\d+\.\d+', match.group(2))
                    values = list(map(float, values_str))
                    viewer.update_row(row, values)
                except ValueError as e:
                    print(f"Parse error in line: {line} - {e}")
            else:
                # 可选：打印未匹配的行用于调试
                # print(f"Unmatched line: {line}")
                pass

        except Exception as e:
            print("Serial error:", e)
            break


# ================= 主程序 =================
def main():
    port = select_serial_port()
    ser = serial.Serial(port, BAUDRATE, timeout=1)
    print(f"Connected to {port}")

    viewer = HeatmapViewer()

    t = threading.Thread(target=serial_thread, args=(ser, viewer), daemon=True)
    t.start()

    print("Waiting for BOOT button press on ESP32...")
    print("Close window to exit.")

    while plt.fignum_exists(viewer.fig.number):
        plt.pause(0.1)

    ser.close()
    print("Exited.")


if __name__ == "__main__":
    main()