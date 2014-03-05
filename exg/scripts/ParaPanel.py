'''
Created on 2013-8-16

@author: wenli
'''
import wx
#import global data
import gd

class ParaPanel(wx.Dialog):
    '''
    This class response gyroscope and acclerometers data.
    '''


    def __init__(self, BleMgr):
        # Instead of calling wx.Dialog.__init__ we precreate the dialog
        # so we can set an extra style that must be set before
        # creation, and then we create the GUI object using the Create
        # method.
        self.BleMgr = BleMgr
        pre = wx.PreDialog()
        pre.SetExtraStyle(wx.DIALOG_EX_CONTEXTHELP)
        ID = -1
        title = "BLE Gyroscope and Accelerometer"
        size=wx.DefaultSize
        pos=wx.DefaultPosition
        style=wx.DEFAULT_DIALOG_STYLE
        pre.Create(None, ID, title, pos, size, style)

        # This next step is the most important, it turns this Python
        # object into the real wrapper of the dialog (instead of pre)
        # as far as the wxPython extension is concerned.
        self.PostCreate(pre)
        
        self.started = False
        self.bw_cfg = 0
        self.gain_cfg = 0
        self.automatic_tempRead = 0
        self.isSavingFile = False
        self.Bind(wx.EVT_CLOSE, self.OnCloseWindow)

        self.create_main_panel()
        
    def create_main_panel(self):
        self.panel = wx.Panel(self)
        
        # parameter setting area
        height = 8
        width = 8
        self.totalReg = 49
        self.paraEdit = []
        self.paraText = range(0, self.totalReg)
        self.ParaBoxHorizontal = wx.BoxSizer(wx.HORIZONTAL)
        for i in range(0, width):
            verticalBox = wx.BoxSizer(wx.VERTICAL)
            for j in range(0, height):
                strIndex = i*height+j
                regText =  wx.TextCtrl(self.panel, -1, "00", size=(75, -1))
                regText.SetMaxLength(2)
                self.paraEdit.append(regText)
                lableStr = hex(strIndex).lstrip('0x').upper().rjust(2, '0')
                staticText = wx.StaticText(self.panel, -1, lableStr, (0,0),(20,20), wx.ALIGN_CENTER_VERTICAL)
                sTE = wx.BoxSizer(wx.HORIZONTAL)
                sTE.Add(staticText, 0, wx.FIXED_MINSIZE )
                sTE.Add(self.paraEdit[strIndex], 0, wx.EXPAND)
                verticalBox.Add(sTE, 1, wx.EXPAND)
            self.ParaBoxHorizontal.Add(verticalBox, 0,  flag=wx.LEFT | wx.TOP | wx.GROW)
        
        #Button area
        buttonSizer = wx.BoxSizer(wx.HORIZONTAL)
        b1 = wx.Button(self.panel, -1, "Get")
        self.Bind(wx.EVT_BUTTON, self.OnClickGet, b1)
        b2 = wx.Button(self.panel, -1, "Set")
        self.Bind(wx.EVT_BUTTON, self.OnClickSet, b2)
        b3 = wx.Button(self.panel, -1, "Cancel")
        self.Bind(wx.EVT_BUTTON, self.OnClickCancel, b3)
        buttonSizer.Add(b1, 0, wx.EXPAND)
        buttonSizer.Add(b2, 0, wx.EXPAND)
        buttonSizer.Add(b3, 0, wx.EXPAND)
        
        #Total Sizer         
        totalSizer = wx.BoxSizer(wx.VERTICAL)
        totalSizer.Add(self.ParaBoxHorizontal, 1, wx.EXPAND)
        totalSizer.Add(buttonSizer, 0, wx.ALIGN_RIGHT)
        self.panel.SetSizer(totalSizer)
        totalSizer.Fit(self)    
    
    def OnClickGet(self, event):
        #write something to obtain the parameter
        print "Get parameter "
        gd.cmd_write = "\x3F\x02\x33"
        wx.Sleep(1)
        readIndex = 0
        while(readIndex < self.totalReg):
            wx.Sleep(0.002)
            gd.sensor_data_lock.acquire()
            if gd.parameterData is not []:
                buf = gd.parameterData
                gd.parameterData = []
            gd.sensor_data_lock.release()
            if buf is not []:
                for i in range(0,len(buf),2):
                    strValue = hex(buf[i+1]).lstrip('0x')
                    strValue = strValue.rjust(2, '0')
                    self.paraEdit[buf[i]].SetValue(strValue)
                    readIndex = readIndex+1
                    print readIndex
            
        pass
       
    def OnClickSet(self, event):
        #write something to comm port
        print "Set parameter"
        for i in range(0, self.totalReg):
            #read text 
            regValue = self.paraEdit[i].GetValue()
            regValue = regValue.rjust(2, '0')
            strCache = "\x3F\x03\x33"
            strCache = strCache + chr(i)
            strCache = strCache + regValue.decode("hex")
            gd.cmd_write = strCache
            while gd.cmd_write is not None:
                wx.Sleep(0.002)
        dlg = wx.MessageDialog(self, 'Set Ok, Click Get to check', 'See All Register Value', wx.OK | wx.ICON_INFORMATION)
        dlg.ShowModal()
        dlg.Destroy()
    
    def OnClickCancel(self, event):
        #directly quit the dialog
        self.Destroy()
        pass
    
    def OnCloseWindow(self, event):
        print "close"
        self.Destroy()
    
