import gi
import json
import os 

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk

def LoadConfigFiles() :
    dir_path = os.path.dirname(os.path.realpath(__file__))

def GetGridRows(grid):
    rows = 0
    for child in grid.get_children():
        x = grid.child_get_property(child, 'top-attach')
        height = grid.child_get_property(child, 'height')
        rows = max(rows, x+height)
    return rows

def GetGridCols(grid):
    cols = 0
    for child in grid.get_children():
        x = grid.child_get_property(child, 'left-attach')
        width = grid.child_get_property(child, 'width')
        cols = max(cols, x+width)
    return cols

class ConfigDlg(Gtk.Dialog):
    def __init__(self):
        Gtk.Dialog.__init__(self, title="Config RPC Router", flags=0)
        self.add_buttons(
            Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_OK, Gtk.ResponseType.OK
        )
        self.set_default_size(500, 660)
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)

        self.set_border_width(10)
        box = self.get_content_area()
        box.add(vbox)

        stack = Gtk.Stack()
        stack.set_transition_type(Gtk.StackTransitionType.SLIDE_LEFT_RIGHT)
        stack.set_transition_duration(1000)

        gridNet = Gtk.Grid();
        gridNet.set_row_homogeneous( True )
        gridNet.props.row_spacing = 6
        self.InitNetworkPage( gridNet, 0, 0 )

        stack.add_titled(gridNet, "GridConn", "Connection")

        label = Gtk.Label()
        label.set_text("Multihop Downstream Nodes")
        gridMisc = Gtk.Grid();
        gridMisc.add(label)
        stack.add_titled(gridMisc, "GridMisc", "Misc")

        stack_switcher = Gtk.StackSwitcher()
        stack_switcher.set_stack(stack)
        vbox.pack_start(stack_switcher, False, True, 0)
        vbox.pack_start(stack, True, True, 0)

        self.show_all()

    def InitNetworkPage( self, grid:Gtk.Grid, startCol, startRow ) :
        labelIp = Gtk.Label()
        labelIp.set_text("ip address: ")
        labelIp.set_xalign(1)
        grid.attach(labelIp, startCol, startRow, 1, 1 )

        ipEditBox = Gtk.Entry()
        ipEditBox.set_text("192.168.0.1")
        grid.attach(ipEditBox, startCol + 1, startRow + 0, 2, 1 )

        labelPort = Gtk.Label()
        labelPort.set_text("port number: ")
        labelPort.set_xalign(1)
        grid.attach(labelPort, startCol + 0, startRow + 1, 1, 1 )

        portEditBox = Gtk.Entry()
        portEditBox.set_text("4132")
        grid.attach( portEditBox, startCol + 1, startRow + 1, 1, 1)

        self.AddSSLOptions( grid, startCol + 0, startRow + 2 )
        self.AddAuthOptions( grid, startCol + 0, startRow + 5 )

        rows = GetGridRows( grid )
        labelCompress = Gtk.Label()
        labelCompress.set_text("Compress: ")
        labelCompress.set_xalign(1)
        grid.attach(labelCompress, startCol + 0, rows, 1, 1 )

        compressCheck = Gtk.CheckButton(label="")
        compressCheck.connect(
            "toggled", self.on_button_toggled, "Compress")
        grid.attach( compressCheck, startCol + 1, rows, 1, 1)

        labelWebSock = Gtk.Label()
        labelWebSock.set_text("WebSocket: ")
        labelWebSock.set_xalign(1)
        grid.attach(labelWebSock, startCol + 0, rows + 1, 1, 1 )

        webSockCheck = Gtk.CheckButton(label="")
        webSockCheck.connect(
            "toggled", self.on_button_toggled, "WebSock")
        grid.attach( webSockCheck, startCol + 1, rows + 1, 1, 1)

    def AddSSLOptions( self, grid:Gtk.Grid, startCol, startRow ) :
        labelSSL = Gtk.Label()
        labelSSL.set_text("SSL: ")
        labelSSL.set_xalign(1)
        grid.attach(labelSSL, startCol, startRow, 1, 1 )

        bActive = False
        sslCheck = Gtk.CheckButton(label="")
        sslCheck.set_active( bActive )
        sslCheck.connect(
            "toggled", self.on_button_toggled, "SSL")
        grid.attach( sslCheck, startCol + 1, startRow, 1, 1 )

        labelKey = Gtk.Label()
        labelKey.set_text("Key File: ")
        labelKey.set_xalign(.618)
        grid.attach(labelKey, startCol + 1, startRow + 1, 1, 1 )

        keyEditBox = Gtk.Entry()
        keyEditBox.set_text("./server.key")
        grid.attach(keyEditBox, startCol + 2, startRow + 1, 1, 1 )

        keyBtn = Gtk.Button.new_with_label("...")
        keyBtn.connect("clicked", self.on_choose_key_clicked)
        grid.attach(keyBtn, startCol + 3, startRow + 1, 1, 1 )

        labelCert = Gtk.Label()
        labelCert.set_text("Cert File: ")
        labelCert.set_xalign(.618)
        grid.attach(labelCert, startCol + 1, startRow + 2, 1, 1 )

        certEditBox = Gtk.Entry()
        certEditBox.set_text("./server.crt")
        grid.attach(certEditBox, startCol + 2, startRow + 2, 1, 1 )

        certBtn = Gtk.Button.new_with_label("...")
        certBtn.connect("clicked", self.on_choose_cert_clicked)
        grid.attach(certBtn, startCol + 3, startRow + 2, 1, 1 )

        keyEditBox.set_sensitive( bActive )
        certEditBox.set_sensitive( bActive )
        keyBtn.set_sensitive( bActive )
        certBtn.set_sensitive( bActive )

        self.certEdit = certEditBox
        self.keyEdit = keyEditBox
        sslCheck.keyBtn = keyBtn
        sslCheck.certBtn = certBtn
        keyBtn.editBox = keyEditBox
        certBtn.editBox = certEditBox


    def on_button_toggled( self, button, name ):
        print( name )
        if name == 'SSL' :
            self.certEdit.set_sensitive( button.props.active )
            self.keyEdit.set_sensitive( button.props.active )
            button.certBtn.set_sensitive( button.props.active )
            button.keyBtn.set_sensitive( button.props.active )
        elif name == 'Auth' :
            self.svcEdit.set_sensitive( button.props.active )
            self.realmEdit.set_sensitive( button.props.active )
            self.signCombo.set_sensitive( button.props.active )
            self.kdcEdit.set_sensitive( button.props.active )

    def on_choose_key_clicked( self, button ) :
        self.on_choose_file_clicked( button, True )

    def on_choose_cert_clicked( self, button ) :
        self.on_choose_file_clicked( button, False )

    def on_choose_file_clicked( self, button, bKey:bool ) :
        dialog = Gtk.FileChooserDialog(
            title="Please choose a file", parent=self, action=Gtk.FileChooserAction.OPEN
        )
        dialog.add_buttons(
            Gtk.STOCK_CANCEL,
            Gtk.ResponseType.CANCEL,
            Gtk.STOCK_OPEN,
            Gtk.ResponseType.OK,
        )

        self.add_filters(dialog, bKey)

        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            button.editBox.set_text( dialog.get_filename() )

        dialog.destroy()
        
    def add_filters(self, dialog, bKey ):
        filter_text = Gtk.FileFilter()
        if bKey :
            strFile = "key files"
        else :
            strFile = "cert files"
        filter_text.set_name( strFile )
        filter_text.add_mime_type("text/plain")
        dialog.add_filter(filter_text)

        filter_any = Gtk.FileFilter()
        filter_any.set_name("Any files")
        filter_any.add_pattern("*")
        dialog.add_filter(filter_any)

    def AddAuthOptions( self, grid:Gtk.Grid, startCol, startRow ) :
        labelAuth = Gtk.Label()
        labelAuth.set_text("Auth: ")
        labelAuth.set_xalign(1)
        grid.attach(labelAuth, startCol, startRow, 1, 1 )

        bActive = False
        authCheck = Gtk.CheckButton(label="")
        authCheck.set_active( bActive )
        authCheck.connect(
            "toggled", self.on_button_toggled, "Auth")
        grid.attach( authCheck, startCol + 1, startRow, 1, 1 )

        labelMech = Gtk.Label()
        labelMech.set_text("Auth Mech: ")
        labelMech.set_xalign(.618)
        grid.attach(labelMech, startCol + 1, startRow + 2, 1, 1 )

        mechList = Gtk.ListStore()
        mechList = Gtk.ListStore(int, str)
        mechList.append([1, "Kerberos"])
    
        mechCombo = Gtk.ComboBox.new_with_model_and_entry(mechList)
        mechCombo.set_entry_text_column(1)
        mechCombo.set_active( 0 )
        mechCombo.set_sensitive( False )
        grid.attach(mechCombo, startCol + 2, startRow + 2, 1, 1 )

        labelSvc = Gtk.Label()
        labelSvc.set_text("Service Name: ")
        labelSvc.set_xalign(.618)
        grid.attach(labelSvc, startCol + 1, startRow + 3, 1, 1 )

        svcEditBox = Gtk.Entry()
        svcEditBox.set_text("")
        grid.attach(svcEditBox, startCol + 2, startRow + 3, 2, 1 )

        labelRealm = Gtk.Label()
        labelRealm.set_text("Realm: ")
        labelRealm.set_xalign(.618)
        grid.attach(labelRealm, startCol + 1, startRow + 4, 1, 1 )

        realmEditBox = Gtk.Entry()
        realmEditBox.set_text("")
        grid.attach(realmEditBox, startCol + 2, startRow + 4, 2, 1 )

        labelSign = Gtk.Label()
        labelSign.set_text("Sign/Encrypt: ")
        labelSign.set_xalign(.618)
        grid.attach(labelSign, startCol + 1, startRow + 5, 1, 1 )

        signMethod = Gtk.ListStore()
        signMethod = Gtk.ListStore(int, str)
        signMethod.append([1, "Encrypt Messages"])
        signMethod.append([2, "Sign Messages"])

        signCombo = Gtk.ComboBox.new_with_model_and_entry(signMethod)
        signCombo.connect("changed", self.on_sign_msg_changed)
        signCombo.set_entry_text_column(1)
        signCombo.set_active(1)

        grid.attach( signCombo, startCol + 2, startRow + 5, 1, 1 )

        labelKdc = Gtk.Label()
        labelKdc.set_text("KDC IP address: ")
        labelKdc.set_xalign(.618)
        grid.attach(labelKdc, startCol + 1, startRow + 6, 1, 1 )

        kdcEditBox = Gtk.Entry()
        kdcEditBox.set_text("")
        grid.attach(kdcEditBox, startCol + 2, startRow + 6, 2, 1 )

        self.svcEdit = svcEditBox
        self.realmEdit = realmEditBox
        self.signCombo = signCombo
        self.kdcEdit = kdcEditBox

        svcEditBox.set_sensitive( authCheck.props.active )
        realmEditBox.set_sensitive( authCheck.props.active )
        signCombo.set_sensitive( authCheck.props.active )

    def on_sign_msg_changed(self, combo) :
        tree_iter = combo.get_active_iter()
        if tree_iter is not None:
            model = combo.get_model()
            row_id, name = model[tree_iter][:2]
            print("Selected: ID=%d, name=%s" % (row_id, name))
        else:
            entry = combo.get_child()
            print("Entered: %s" % entry.get_text())


win = ConfigDlg()
win.connect("close", Gtk.main_quit)
response = win.run()
if response == Gtk.ResponseType.OK:
    print("The OK button was clicked")
win.destroy()