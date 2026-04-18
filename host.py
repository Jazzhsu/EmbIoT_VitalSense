import serial
import struct
from serial.tools import list_ports
import matplotlib.pyplot as plt
import numpy as np

from pipeline import Pipeline

SAMPLE_PER_CHIRP = 128
CHIRP_PER_FRAME = 256
MAGIC = 0xFFDDFFDD
COMS = 'COM6'

vital_sense = Pipeline(SAMPLE_PER_CHIRP, CHIRP_PER_FRAME, 8)

def sync_buffer(s: serial.Serial):
    buf = b""
    while True:
        buf = (buf + s.read(1))[-4:]
        if (len(buf) == 4) and struct.unpack('<I', buf)[0] == MAGIC:
            return


s = serial.Serial(COMS)

plt.ion()
fig, (ax_mean, ax_var, ax_breath, ax_heart) = plt.subplots(
    4, 1,
    figsize=(8, 12),
    gridspec_kw={"height_ratios": [1, 1, 3, 3]},
)
bins = np.arange(vital_sense._range_fft_size // 2)
(line_mean,) = ax_mean.plot(bins, np.zeros_like(bins, dtype=float))
(line_var,)  = ax_var .plot(bins, np.zeros_like(bins, dtype=float))

n_slow = vital_sense._buffer_size * vital_sense._chirp_per_frame
t = np.arange(n_slow) / vital_sense._fs                     # seconds
(line_breath,) = ax_breath.plot(t, np.zeros(n_slow))
(line_heart,)  = ax_heart .plot(t, np.zeros(n_slow))

ax_breath.set_ylim(-3000, 3000)
ax_heart .set_ylim(-1500, 1500)

ax_mean.set_title("mean |spec|"); ax_mean.set_xlabel("range bin")
ax_var .set_title("variance |spec|")
ax_breath.set_title("breath (µm)");       ax_breath.set_xlabel("time (s)")
ax_heart .set_title("heart (µm)");        ax_heart .set_xlabel("time (s)")
fig.tight_layout()

sync_buffer(s)
while True:
    nbytes = struct.unpack('<I', s.read(4))[0]
    data = s.read(nbytes)
    # print(len(data))
    
    # unpack the data as unsigned short (uint16_t)
    data = [ d[0] for d in struct.iter_unpack('<H', data) ]
    # print(len(data), data[0], data[-1])

    vital_sense.enque(data)
    if vital_sense.data_ready():
        spec = vital_sense.range_fft()
        mag  = np.abs(spec)

        line_mean.set_ydata(mag.mean(axis=0))
        line_var .set_ydata(mag.var(axis=0, dtype=np.float64))

        breath, heart = vital_sense.vitals()
        disp = vital_sense.displacement()       # pre-filter, µm
        # line_breath.set_ydata(disp)             # temporarily overwrite
        line_breath.set_ydata(breath)
        line_heart .set_ydata(heart)

        for ax in (ax_mean, ax_var):
            ax.relim(); ax.autoscale_view()     # rescale Y as signal changes

        fig.canvas.draw_idle()
        fig.canvas.flush_events()               # pump the GUI so the window repaints

        iq, bin_idx = vital_sense.target_iq()
        print(bin_idx * 7.5)

    peek = s.read(4)
    if struct.unpack('<I', peek)[0] != MAGIC:
        sync_buffer(s)
        continue