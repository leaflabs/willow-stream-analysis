from PyQt4 import QtCore, QtGui

class StreamDialog(QtGui.QDialog):

    def __init__(self, parent=None):
        super(StreamDialog, self).__init__(parent)

        self.chipNumberLine = QtGui.QLineEdit('0')
        self.chipNumberLine.setValidator(QtGui.QIntValidator(0,31,self))

        self.xrangeLine = QtGui.QLineEdit('1000')
        self.xrangeLine.setValidator(QtGui.QDoubleValidator(1,100000,1,self))

        self.refreshRateLine = QtGui.QLineEdit('20')
        self.refreshRateLine.setValidator(QtGui.QIntValidator(2,60,self))

        self.dialogButtons = QtGui.QDialogButtonBox(
            QtGui.QDialogButtonBox.Ok | QtGui.QDialogButtonBox.Cancel, QtCore.Qt.Horizontal, self)
        self.dialogButtons.accepted.connect(self.accept)
        self.dialogButtons.rejected.connect(self.reject)

        layout = QtGui.QGridLayout()
        layout.addWidget(QtGui.QLabel('Chip Number:'), 0,0, 1,1)
        layout.addWidget(self.chipNumberLine, 0,1, 1,2)
        layout.addWidget(QtGui.QLabel('Maximal displayed x-range (ms):'), 2,0, 1,1)
        layout.addWidget(self.xrangeLine, 2,1, 1,1)
        layout.addWidget(QtGui.QLabel('Refresh Rate (Hz):'), 3,0, 1,1)
        layout.addWidget(self.refreshRateLine, 3,1, 1,2)
        layout.addWidget(self.dialogButtons, 4,2, 1,2)

        self.setLayout(layout)
        self.setWindowTitle('Playback Window Parameters')
        self.setWindowIcon(QtGui.QIcon('../img/round_logo_60x60.png'))
        self.resize(500,100)


    def getParams(self):
        params = {}
        params['chip'] = int(self.chipNumberLine.text())
        params['xrange'] = int(self.xrangeLine.text())
        params['refreshRate'] = int(self.refreshRateLine.text())
        return params

