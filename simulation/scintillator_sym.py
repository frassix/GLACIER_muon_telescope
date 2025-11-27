"""
Real-time triple-coincidence scintillator simulator
Landau pulse shape, 1 MSPS, 8-bit serial output to Arduino (confirm that it works please)
"""
import math
import argparse
import random
import time
import struct
import serial
import numpy as np
from matplotlib import pyplot as plt

# ---------- user knobs ----------
SINGLES_RATE = 200          # Hz per paddle
TRIPLE_RATE  =  0.1         # Hz   (sovrastimato)
JITTER_NS    =  2           # RMS ns between paddles
SERIAL_PORT  = "/dev/ttyUSB0"      # COM3 on windows !!!!!!!
# ---------------------------------
SAMPLE_NS   = 1e9           # 1 GHz
TAU_RISE    =  5e-9         # 5 ns
TAU_FALL    = 10e-9         # 10 ns
PULSE_LEN   = 100e-9        # 100 ns total
MAX_AMPL    = 220           # 8-bit headroom

# --------- BiExponential shape ---------
"""
Sinals behave like a biexponential with two characteristic times: rise and fall.
The amplitude is set to 150 mV. All other possible signals would be higher. 
Setting any THR level for the smallest 1p.e. signal assures the correct functioning.
"""

t = np.arange(int(round(PULSE_LEN * SAMPLE_NS,0))) * 1e-9
pulse_shape = np.exp(-t / TAU_FALL) * (1 - np.exp(-t / TAU_RISE))
pulse_shape = pulse_shape / pulse_shape.max() * 150         # normalise to 150 mV

# --------- Functions ---------
def biexp_pulse(ampl):
    """return 8-bit array"""
    return np.clip(ampl * pulse_shape, 0, 255).astype(np.uint8)

def next_event_time(rate):
    """return ns (float)"""
    return -math.log(1.0 - random.random()) / rate * 1e9

def main():
    """Module for the complete real-time signal generator"""
    parser = argparse.ArgumentParser()
    parser.add_argument("-p","--port", default=SERIAL_PORT)
    parser.add_argument("-s","--singles", type=float, default=SINGLES_RATE, help='Noise rate')
    parser.add_argument("-t","--triple",  type=float, default=TRIPLE_RATE,  help='Triplets rate')
    parser.add_argument("-j","--jitter",  type=float, default=JITTER_NS,    help='Triplets jitter')
    parser.add_argument("-v", "--verbose", type=bool, default=False,        help='True for verbose')
    args = parser.parse_args()

    if args.verbose:
        plt.figure()
        plt.title('Signal shape', fontweight='bold', fontsize=24)
        plt.plot(t,pulse_shape)
        plt.ylabel(r'$\Delta V\, [mV]$', fontweight='bold', fontsize=20)
        plt.xlabel(r'$\Delta t\, [s]$', fontweight='bold', fontsize=20)
        plt.grid(which='both')
        plt.show()

    ser = serial.Serial(args.port, 2_000_000, timeout=0) # MODIFY HERE
    print(f"Streaming to {args.port} …  Ctrl-C to stop")

    queues = [[], [], []]          # per-channel event queues
    t0 = time.perf_counter_ns()
    sample_t = 0

    next_single = [next_event_time(args.singles) for _ in range(3)]
    next_triple = next_event_time(args.triple)

    try:
        while True:
            # 1. schedule singles
            for ch in range(3):
                if sample_t >= next_single[ch]:
                    amp = random.randint(80, MAX_AMPL)
                    queues[ch].append((sample_t, biexp_pulse(amp)))
                    next_single[ch] = sample_t + next_event_time(args.singles)

            # 2. schedule triple coincidence
            if sample_t >= next_triple:
                base_amp = random.randint(100, MAX_AMPL)
                base_time = sample_t
                for ch in range(3):
                    jitter = random.gauss(0, args.jitter)
                    t_evt = base_time + int(jitter)
                    queues[ch].append((t_evt, biexp_pulse(base_amp)))
                next_triple = sample_t + next_event_time(args.triple)

            # 3. build current sample
            out = [0, 0, 0]
            for ch in range(3):
                remove = []
                for evt_t, pulse in queues[ch]:
                    idx = int((sample_t - evt_t) / SAMPLE_NS)
                    if 0 <= idx < len(pulse):
                        out[ch] += pulse[idx]
                    elif idx >= len(pulse):
                        remove.append((evt_t, pulse))
                for r in remove:
                    queues[ch].remove(r)
                out[ch] = min(255, out[ch])

            # 4. send packet
            ser.write(b'\xff' + struct.pack('BBB', out[0], out[1], out[2]))

            # 5. advance simulation clock
            sample_t += SAMPLE_NS
            while time.perf_counter_ns() - t0 < sample_t:
                pass

    except KeyboardInterrupt:
        print("\nStopped.")
        ser.close()

if __name__ == "__main__":
    main()
