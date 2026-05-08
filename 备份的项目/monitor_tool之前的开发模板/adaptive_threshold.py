import time, collections
import numpy as np

class AdaptiveThreshold:
    def __init__(self, default=230, min_threshold=50, max_threshold=1000, percentile=70, alpha=0.9, timeout_seconds=3.0, history_size=100):
        self.default = default
        self.th = default
        self.min_threshold = min_threshold
        self.max_threshold = max_threshold
        self.percentile = percentile
        self.alpha = alpha
        self.timeout_seconds = timeout_seconds
        self.history = collections.deque(maxlen=history_size)
        self.last_peak_time = None

    def note_peak(self, amp, t=None):
        if t is None: t = time.monotonic()
        self.history.append(amp)
        self.last_peak_time = t
        self._update_from_history()

    def _update_from_history(self):
        if len(self.history) == 0: return
        arr = np.array(self.history)
        base = np.percentile(arr, max(10, min(50, self.percentile-20)))
        peak_est = np.percentile(arr, self.percentile)
        new_est = base + (peak_est - base)*0.5
        self.th = max(self.min_threshold, min(self.max_threshold, self.alpha*self.th + (1-self.alpha)*new_est))

    def get_threshold(self):
        if self.last_peak_time is not None and (time.monotonic()-self.last_peak_time)>self.timeout_seconds:
            self.th = max(self.min_threshold, min(self.max_threshold, 0.5*self.th+0.5*self.default))
            self.history.clear()
            self.last_peak_time = None
        return self.th
