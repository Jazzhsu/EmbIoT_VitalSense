import numpy as np
from scipy.signal import butter, sosfiltfilt

class Pipeline:
    def __init__(self, sample_per_chirp: int, chirp_per_frame: int, buffer_size: int, bin_os: int = 5,
                 prt: float = 5e-3, f_center=60.5e9):
        # 1 frame = sample_per_chirp x chirp_per_frame
        self._sample_per_chirp = sample_per_chirp
        self._chirp_per_frame = chirp_per_frame
        self._buffer_size = buffer_size  # number of frames we hold
        self._range_fft_size = sample_per_chirp
        self._bin_os = bin_os

        self._data = np.zeros((buffer_size * chirp_per_frame, sample_per_chirp), dtype=np.float32)
        self._n_frames = 0

        # Matches Matlab: 2 * blackman(samples_per_chirp)
        self._window = (2.0 * np.blackman(sample_per_chirp)).astype(np.float32)

        # Matches Matlab: 16 * 3.3 * range_fft_size / samples_per_chirp
        self._if_scale = 16.0 * 3.3 * self._range_fft_size / sample_per_chirp

        self._prt = prt
        self._fs = 1.0 / prt                             # slow-time sample rate (Hz)
        lam = 3e8 / f_center
        self._rad_to_um = lam / (4 * np.pi) * 1e6        # phase→µm displacement

        self._sos_breath = butter(4, [0.1, 0.5], btype="bandpass",
                                fs=self._fs, output="sos")
        self._sos_heart  = butter(4, [0.75, 3.0], btype="bandpass",
                                fs=self._fs, output="sos")

    def data_ready(self):
        return self._n_frames >= self._buffer_size

    def enque(self, data: list):
        assert len(data) == self._sample_per_chirp * self._chirp_per_frame, "size of the data doesn't match"
        frame = np.array(data).reshape((self._chirp_per_frame, self._sample_per_chirp))

        # shift the data up
        self._data[:-self._chirp_per_frame, :] = self._data[self._chirp_per_frame:, :]

        # enque the latest frame
        self._data[-self._chirp_per_frame:, :] = frame

        self._n_frames += 1

    def range_fft(self):
        assert self._n_frames >= self._buffer_size, "buffer not filled yet"
        data = self._data - self._data.mean(axis=1, keepdims=True)
        data *= self._if_scale
        data *= self._window
        spec = np.fft.rfft(data, n=self._range_fft_size, axis=1)

        return spec[:, :self._range_fft_size // 2]
    
    def target_bin(self, spec: np.ndarray):
        energy = np.abs(spec[:, self._bin_os:]).sum(axis=0)
        return self._bin_os + int(np.argmax(energy))
    
    def target_iq(self):
        assert self.data_ready()

        spec = self.range_fft()
        bin_idx = self.target_bin(spec)
        return spec[:, bin_idx], bin_idx
    
    def displacement(self):
        """Returns chest displacement in µm over the full slow-time window."""
        # iq, _bin = self.target_iq()
        # Poor-man's circle fit: center on the mean, skip scale normalization
        """
        iq, _ = self.target_iq()
        cx, cy, r = self._circle_fit_taubin(iq.real, iq.imag)
        iq = (iq - complex(cx, cy)) / r        # recenter + normalize to unit circle
        dphi = np.angle(iq[1:] * np.conj(iq[:-1]))   # incremental phase
        phase = np.concatenate(([0.0], np.cumsum(dphi)))
        # phase = np.unwrap(np.angle(iq))
        return phase * self._rad_to_um                   # µm
        """
    
        iq, _ = self.target_iq()
        iq = iq - iq.mean()
        dphi = np.angle(iq[1:] * np.conj(iq[:-1]))
        phase = np.concatenate(([0.0], np.cumsum(dphi)))
        return phase * self._rad_to_um

    def vitals(self):
        """Returns (breath_um, heart_um) — same length as the slow-time buffer."""
        d = self.displacement()
        d = d - d.mean()                                  # pre-filter DC strip
        breath = sosfiltfilt(self._sos_breath, d)
        heart  = sosfiltfilt(self._sos_heart,  d)
        return breath, heart
    
    def _circle_fit_taubin(self, x, y):
        mx, my = x.mean(), y.mean()
        u, v = x - mx, y - my
        z = u*u + v*v
        zmean = z.mean()
        z0 = (z - zmean) / (2.0 * np.sqrt(zmean))
        M = np.column_stack([z0, u, v])
        _, _, vt = np.linalg.svd(M, full_matrices=False)
        a0, a1, a2 = vt[-1]
        a0 = a0 / (2.0 * np.sqrt(zmean))
        a3 = -zmean * a0
        cx = -a1 / (2.0 * a0) + mx
        cy = -a2 / (2.0 * a0) + my
        r  = np.sqrt((a1*a1 + a2*a2 - 4.0*a0*a3) / (4.0*a0*a0))
        return cx, cy, r
