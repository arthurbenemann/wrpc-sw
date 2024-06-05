import sys
from PyQt6.QtWidgets import QApplication, QPushButton, QLabel, QGridLayout, QLineEdit, QMainWindow
from PyQt6.QtGui import QPixmap
from backend import SnmpHandler

coreOid = ((1, 3, 6, 1, 4, 1, 96, 101, 1))
mibDir = '~/.pysnmp/mibs'
wrpcMibName = 'WR-WRPC-MIB'

class MyWindow(QMainWindow):

    def __init__(self, targetIp):
        super().__init__()
        # setup window
        self.setGeometry(10, 100, 2000, 800)
        self.setWindowTitle('Virgo Demonstrator Box')
        # init snmp_handler
        self.snmp_handler = SnmpHandler(targetIp, wrpcMibName, mibDir, coreOid)
        self.edit_button_list = []
        # Set up the background image
        pixmap = QPixmap('Virgo_WR_Front_Panel/FrontPanel.jpg')
        self.background_label = QLabel(self)
        self.background_label.setPixmap(pixmap)
        self.background_label.setGeometry(0, 0, self.width(), self.height())
        self.grid = QGridLayout()

        self.init_ui()

    def configure_ui(self, button, qedit, id):
        base = 10
        offset = 110
        button.setGeometry(base + offset*id, 10, 100, 30)
        button.clicked.connect(lambda: self.toggle(id))
        qedit.setReadOnly(True)
        qedit.setGeometry(base + offset*id, 50, 100, 30)

    def init_ui(self):
        for i in range(3):
            edit = QLineEdit(self)
            if i < 2:
                button = QPushButton(f'SelGroup {i}', self)
            elif i == 2:
                button = QPushButton(f'IRIG-B', self)
            self.configure_ui(button, edit, i)
            # Append the tuple to the list
            self.edit_button_list.append((edit, button))

    def update_timing_status(self, id, status):
        self.edit_button_list[id][0].setText(status)

    def toggle(self, id):
        
        try:
            if id < 2:
                SelGroup = "wrpcSelGroup" + str(id)
                varBinds = self.snmp_handler.get(SelGroup)

                for varBind in varBinds:
                    _, value = varBind
                    current_value = value.prettyPrint()
                if current_value == "10":
                    self.snmp_handler.set(SelGroup, 100)
                    self.update_timing_status(id, "100MHz")
                elif current_value == "100":
                    self.snmp_handler.set(SelGroup, 10)
                    self.update_timing_status(id, "10MHz")
            elif id == 2:
                SelGroup = "wrpcIRIGB"
                varBinds = self.snmp_handler.get(SelGroup)

                for varBind in varBinds:
                    _, value = varBind
                    current_value = value.prettyPrint()
                if current_value == "enabled":
                    self.snmp_handler.set(SelGroup, "disable")
                    self.update_timing_status(id, "disabled")
                elif current_value == "disabled":
                    self.snmp_handler.set(SelGroup, "enable")
                    self.update_timing_status(id, "enabled")

        except Exception as e:
                print(f"Connection issue? Wrong target IP? : {e}")

if __name__ == '__main__':
    # get target's IP address
    if len(sys.argv) != 2:
        print("Usage: python script.py <IP_ADDRESS>")
        sys.exit(1)
    targetIp = sys.argv[1]

    app = QApplication([])
    window = MyWindow(targetIp)
    window.show()

    sys.exit(app.exec())