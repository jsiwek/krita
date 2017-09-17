import sys
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *
from krita import *


class HighpassExtension(Extension):

    def __init__(self, parent):
        super().__init__(parent)

    def setup(self):
        action = Application.createAction("high_pass_filter", "High Pass")
        action.triggered.connect(self.showDialog)

    def showDialog(self):
        doc = Application.activeDocument()
        if doc == None:
            QMessageBox.information(Application.activeWindow().qwindow(), "Highpass Filter", "There is no active image.")
            return

        self.dialog = QDialog(Application.activeWindow().qwindow())

        self.intRadius = QSpinBox()
        self.intRadius.setValue(10)
        self.intRadius.setRange(2, 200)

        self.cmbMode = QComboBox()
        self.cmbMode.addItems(["Color", "Preserve DC", "Greyscale", "Greyscale, Apply Chroma", "Redrobes"])
        self.keepOriginal = QCheckBox("Keep Original Layer")
        self.keepOriginal.setChecked(True)
        form = QFormLayout()
        form.addRow("Filter Radius", self.intRadius)
        form.addRow("Mode", self.cmbMode)
        form.addRow("", self.keepOriginal)

        self.buttonBox = QDialogButtonBox(self.dialog)
        self.buttonBox.setOrientation(QtCore.Qt.Horizontal)
        self.buttonBox.setStandardButtons(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        self.buttonBox.accepted.connect(self.dialog.accept)
        self.buttonBox.accepted.connect(self.highpass)
        self.buttonBox.rejected.connect(self.dialog.reject)

        vbox = QVBoxLayout(self.dialog)
        vbox.addLayout(form)
        vbox.addWidget(self.buttonBox)

        self.dialog.show()
        self.dialog.activateWindow()
        self.dialog.exec_()

    def highpass(self):
        # XXX: Start undo macro
        image = Application.activeDocument()
        original = image.activeNode()
        working_layer = original

        # We can only highpass on paint layers
        if self.keepOriginal.isChecked() or original.type() != "paintlayer":
            working_layer = image.createNode("working", "paintlayer")
            working_layer.setColorSpace(original.colorModel(), original.colorSpace(), original.profile())
            working_layer.writeBytes(original.readBytes(0, 0, image.width(), image.height()),
                                     0, 0, image.width(), image.height())
            original.parentNode().addChildNode(working_layer, original)  # XXX: Unimplemented

        image.setActiveNode(working_layer)
        colors_layer = None

        # if keeping colors
        if self.cmbMode.currentIndex() == 1 or self.cmbMode.currentIndex() == 3:
            colors_layer = working_layer.duplicate()  # XXX: Unimplemented
            colors_layer.setName("colors")
            original.parentNode().addChildNode(working_layer, colors_layer)  # XXX: Unimplemented

        # if greyscale, desature
        if (self.cmbMode.currentIndex() == 2 or self.cmbMode.currentIndex() == 3):
            filter = Application.filter("desaturate")
            filter.apply(working_layer, 0, 0, image.width(), image.height())

        # Duplicate on top and blur
        blur_layer = working_layer.duplicate()
        blur_layer.setName("blur")
        original.parentNode().addChildNode(blur_layer, working_layer)  # XXX: Unimplemented

        # blur
        filter = Application.filter("gaussian blur")
        filter_configuration = filter.configuration()
        filter_configuration.setProperty("horizRadius", self.intRadius.value())
        filter_configuration.setProperty("vertRadius", self.intRadius.value())
        filter_configuration.setProperty("lockAspect", true)
        filter.setConfiguration(filter_configuration)
        filter.apply(blur_layer, 0, 0, image.width(), image.height())

        if self.cmbMode.currentIndex() <= 3:
            blur_layer.setBlendingMode("grain_extract")
            working_layer = image.mergeDown(blur_layer)

            # if preserve chroma, change set the mode to value and merge down with the layer we kept earlier.
            if self.cmbMode.currentIndex() == 3:
                working_layer.setBlendingMode("value")
                working_layer = image.mergeDown(working_layer)

            # if preserve DC, change set the mode to overlay and merge down with the average colour of the layer we kept earlier.
            if self.cmbMode.currentIndex() == 1:
                # get the average color of the entire image
                # clear the colors layer to the given color
                working_layer = image.mergeDown(working_layer)

        else:  # Mode == 4, RedRobes
            image.setActiveNode(blur_layer)
            # Get the average color of the input layer
            # copy the solid colour layer
            # copy the blurred layer
        # XXX: End undo macro

Scripter.addExtension(HighpassExtension(Krita.instance()))
