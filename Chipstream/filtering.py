from PyQt4 import QtCore, QtGui
import matplotlib
from matplotlib.backends.backend_qt4agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.backends.backend_qt4agg import NavigationToolbar2QT as NavigationToolbar
from matplotlib.figure import Figure
import numpy as np
import scipy.signal as signal

class FilterPlotWindow(QtGui.QWidget):
    def __init__(self, filt, cutoffs):
        super(FilterPlotWindow, self).__init__(None)

        freqs = (np.pi / 1.5e4) * np.logspace(np.log10(cutoffs[0])-0.5, np.log10(1.5e4))
        freqs, h = signal.freqz(*filt, worN=freqs)
        # convert frequencies from rad/s to Hz
        freqs = (1.5e4 / np.pi) * freqs
        # get magnitude in dB and phase in degrees
        mag = 20 * np.log10(abs(h))
        phase = np.rad2deg(np.angle(h))
        
        self.fig = Figure()
        self.canvas = FigureCanvas(self.fig)
        self.canvas.setParent(self)

        self.mag_axes = self.fig.add_subplot(211)
        self.mag_axes.set_title('Gain (dB) vs. Frequency (Hz)')
        self.mag_axes.set_xscale('log')
        self.mag_axes.grid(True, which='both', axis='both')
        self.mag_axes.axis([min(freqs), max(freqs), -60, 6])
        self.mag_axes.plot(freqs, mag)
        self.phase_axes = self.fig.add_subplot(212)
        self.phase_axes.set_title('Phase (degrees) vs. Frequency (Hz)')
        self.phase_axes.set_xscale('log')
        self.phase_axes.axis([min(freqs), max(freqs), -200, 200])
        self.phase_axes.grid(True, which='both', axis='both')
        self.phase_axes.plot(freqs, phase)

        for cutoff in cutoffs:
            self.mag_axes.axvline(cutoff, color='green')
            self.phase_axes.axvline(cutoff, color='green')

        self.mpl_toolbar = NavigationToolbar(self.canvas, self)

        self.mplLayout = QtGui.QVBoxLayout()
        self.mplLayout.addWidget(self.canvas)
        self.mplLayout.addWidget(self.mpl_toolbar)
        #self.mplWindow = QtGui.QWidget()
        #self.mplWindow.setLayout(self.mplLayout)
        self.setLayout(self.mplLayout)

        self.setWindowTitle('Data playback transfer function')

        self.canvas.draw()

