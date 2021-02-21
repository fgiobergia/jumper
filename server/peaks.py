import numpy as np
from scipy.signal import find_peaks


def find_skips(readings):
    # max freq: 3 Hz. Sampling @ 25 Hz: 1 peak each 8 samples --> distance=8
    distance = 25//3
    prominence = 500

    xyz = [ v[1] for v in sorted(readings.items(), key=lambda x: x[0]) ]
    x = [ x for x,_,_ in xyz ]
    y = [ y for _,y,_ in xyz ]
    z = [ z for _,_,z in xyz ]

    peaks_x, _ = find_peaks(x, distance=distance, prominence=prominence)
    peaks_y, _ = find_peaks(y, distance=distance, prominence=prominence)
    peaks_z, _ = find_peaks(z, distance=distance, prominence=prominence)

    print(len(peaks_x), len(peaks_y), len(peaks_z))
    return int(round(np.mean([len(peaks_x), len(peaks_y), len(peaks_z)])))

# debugging function: to be deleted
def plot_peaks(x):
    # max freq: 3 Hz. Sampling @ 25 Hz: 1 peak each 8 samples --> distance=8
    distance = 25//3
    prominence=500

    xn = np.array(x)
    plt.plot(xn)
    peaks, _ = find_peaks(x, distance=distance, prominence=prominence)
    print(len(peaks))

    plt.scatter(peaks, xn[peaks], marker='x', color='r')

