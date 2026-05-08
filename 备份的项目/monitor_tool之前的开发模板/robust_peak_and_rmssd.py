import numpy as np
import math
from typing import List

def detect_peaks(signal: np.ndarray, sample_rate: float, threshold: float, min_distance_s: float=0.55):
    min_dist_samples = max(1, int(min_distance_s * sample_rate))
    peaks = []
    last_idx = -min_dist_samples*2
    for i in range(1, len(signal)-1):
        v = signal[i]
        if v > threshold and v > signal[i-1] and v >= signal[i+1]:
            if i - last_idx >= min_dist_samples:
                peaks.append(i); last_idx = i
    return peaks

def compute_rmssd(rrs: List[float], min_rr=0.25, max_rr=2.0, min_required=3):
    rr_filtered = [r for r in rrs if min_rr <= r <= max_rr]
    if len(rr_filtered) < min_required: return float('nan'), rr_filtered
    diffs = np.diff(rr_filtered)
    if len(diffs) == 0: return float('nan'), rr_filtered
    return float(math.sqrt((diffs**2).mean())), rr_filtered
