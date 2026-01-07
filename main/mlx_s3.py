import serial
import serial.tools.list_ports
import threading
import numpy as np
import tkinter as tk
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import matplotlib.pyplot as plt

WIDTH, HEIGHT = 32, 24

class HeatmapApp:
    def __init__(self, root, ser):
        self.ser = ser
        self.root = root

        self.fig, self.ax = plt.subplots(figsize=(4, 3))
        self.data = np.zeros((HEIGHT, WIDTH))

        self.img = self.ax.imshow(
            self.data,
            cmap="inferno",
            vmin=20,
            vmax=40,
            interpolation="nearest"
        )
        self.ax.set_title("Waiting for BOOT key...")
        self.colorbar = self.fig.colorbar(self.img)

        self.canvas = FigureCanvasTkAgg(self.fig, master=root)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        threading.Thread(target=self.serial_thread, daemon=True).start()

    def serial_thread(self):
        while True:
            line = self.ser.readline().decode(errors="ignore").strip()
            if line != "FRAME_BEGIN":
                continue

            values = []
            while True:
                line = self.ser.readline().decode(errors="ignore").strip()
                if line == "FRAME_END":
                    break
                values.extend(line.split(","))

            if len(values) != 768:
                print("Invalid frame length:", len(values))
                continue

            frame = np.array(values, dtype=np.float32).reshape(HEIGHT, WIDTH)
            self.update_image(frame)

    def update_image(self, frame):
        vmin = np.min(frame)
        vmax = np.max(frame)

        self.img.set_data(frame)
        self.img.set_clim(vmin, vmax)
        self.ax.set_title(f"{vmin:.1f}C ~ {vmax:.1f}C")
        self.canvas.draw_idle()

def select_port():
    ports = list(serial.tools.list_ports.comports())
    for i, p in enumerate(ports):
        print(f"{i}: {p.device}")
    idx = int(input("Select port: "))
    return ports[idx].device

def main():
    port = select_port()
    ser = serial.Serial(port, 115200, timeout=1)
    print("Connected to", port)

    root = tk.Tk()
    root.title("MLX90640 Heatmap")
    root.geometry("320x240")

    HeatmapApp(root, ser)
    root.mainloop()

if __name__ == "__main__":
    main()
