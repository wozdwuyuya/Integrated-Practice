import numpy as np

def find_header_indices(buf: bytes, header: bytes):
    idx = []
    start = 0
    while True:
        i = buf.find(header, start)
        if i == -1: break
        idx.append(i)
        start = i + 1
    return idx

def split_frames(buf: bytes, header: bytes=b'\x51\x25\x54\x16\xe3\xbe'):
    ids = find_header_indices(buf, header)
    frames = []
    for i, pos in enumerate(ids):
        end = ids[i+1] if i+1 < len(ids) else len(buf)
        frames.append(buf[pos:end])
    return frames

def parse_payload_to_samples(payload: bytes, skip_header_bytes:int=6, try_dtypes=None):
    if try_dtypes is None: try_dtypes = ['u1', '<i2', '>i2', '<u2', '>u2']
    data = payload[skip_header_bytes:]
    for dt in try_dtypes:
        try:
            arr = np.frombuffer(data, dtype=np.dtype(dt))
            if arr.size == 0: continue
            vmin, vmax = arr.min(), arr.max()
            # --- 溢出安全修正 --- 
            if int(vmax) - int(vmin) < 2: continue
            return {'dtype':dt, 'samples':arr, 'vmin':int(vmin), 'vmax':int(vmax)}
        except Exception: continue
    return {'dtype':None, 'samples':np.array([], dtype=np.int16), 'vmin':0, 'vmax':0}
