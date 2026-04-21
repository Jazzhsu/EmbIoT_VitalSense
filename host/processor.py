import serial
import struct
import threading
from serial.tools import list_ports
import matplotlib.pyplot as plt
import numpy as np

from pipeline import Pipeline

SAMPLE_PER_CHIRP = 128
CHIRP_PER_FRAME = 256
MAGIC = 0xFFDDFFDD
COMS = 'COM6'

# Read data from serial port
class Processor:
    def __init__(self, result: dict, lock: threading.Lock):
        self._result = result
        self._lock = lock
        self._pipe = Pipeline(SAMPLE_PER_CHIRP, buffer_size=CHIRP_PER_FRAME * 32, window_size=CHIRP_PER_FRAME * 8, stride=64)

    def worker(self):
        s = serial.Serial(COMS)
        self._sync_buffer(s)

        while True:
            nbytes = struct.unpack('<I', s.read(4))[0]
            data = s.read(nbytes) 

            data = [ d[0] for d in struct.iter_unpack('<H', data) ]
            # print(len(data), data[0], data[-1])
            self._pipe.enque(data)
            if self._pipe.data_ready():
                self._process()

            peek = s.read(4)
            if struct.unpack('<I', peek)[0] != MAGIC:
                self._sync_buffer(s)
                continue

    def _process(self):
        mag = np.abs(self._pipe.range_fft()).mean(axis=0)
        breath, heart = self._pipe.vitals()
        with self._lock:
            self._result["mag"] = mag
            self._result["breath"] = breath
            self._result["heart"] = heart


    def _sync_buffer(self, s: serial.Serial):
        buf = b""
        while True:
            buf = (buf + s.read(1))[-4:]
            if (len(buf) == 4) and struct.unpack('<I', buf)[0] == MAGIC:
                return

# s = serial.Serial(COMS)

# sync_buffer(s)
# while True:
#     nbytes = struct.unpack('<I', s.read(4))[0]
#     data = s.read(nbytes)
#     # print(len(data))
    
#     # unpack the data as unsigned short (uint16_t)
#     data = [ d[0] for d in struct.iter_unpack('<H', data) ]
#     # print(len(data), data[0], data[-1])

#     vital_sense.enque(data)
#     if vital_sense.data_ready():
#         spec = vital_sense.range_fft()
#         mag  = np.abs(spec)

#         line_mean.set_ydata(mag.mean(axis=0))
#         line_var .set_ydata(mag.var(axis=0, dtype=np.float64))

#         breath, heart = vital_sense.vitals()
#         disp = vital_sense.displacement()       # pre-filter, µm
#         # line_breath.set_ydata(disp)             # temporarily overwrite
#         line_breath.set_ydata(breath)
#         line_heart .set_ydata(heart)

#         for ax in (ax_mean, ax_var):
#             ax.relim(); ax.autoscale_view()     # rescale Y as signal changes

#         fig.canvas.draw_idle()
#         fig.canvas.flush_events()               # pump the GUI so the window repaints

#         iq, bin_idx = vital_sense.target_iq()
#         print(bin_idx * 7.5)

#     peek = s.read(4)
#     if struct.unpack('<I', peek)[0] != MAGIC:
#         sync_buffer(s)
#         continue