"""
This demo demonstrates how to draw a dynamic mpl (matplotlib) 
plot in a wxPython application.

It allows "live" plotting as well as manual zooming to specific
regions.

Both X and Y axes allow "auto" or "manual" settings. For Y, auto
mode sets the scaling of the graph to see all the data points.
For X, auto mode makes the graph "follow" the data. Set it X min
to manual 0 to always see the whole data from the beginning.

Note: press Enter in the 'manual' text box to make a new value 
affect the plot.

Eli Bendersky (eliben@gmail.com)
License: this code is in the public domain
Last modified: 31.07.2008
"""
import os
import pprint
import random
import sys
import wx
import serial
import time
import array
import threading
import string
import operator

sensor_data_lock = threading.Lock()
temperature_data = []
pressure_data = []

cmd_write = None
gSerialDev = None
TempretureResult = 0

# The recommended way to use wx with mpl is with the WXAgg
# backend. 
#
import matplotlib
matplotlib.use('WXAgg')
from matplotlib.figure import Figure
from matplotlib.backends.backend_wxagg import FigureCanvasWxAgg as FigCanvas, NavigationToolbar2WxAgg as NavigationToolbar
import numpy as np
import pylab


class DataGen(object):
    """ A silly class that generates pseudo-random data for
        display in the plot.
    """
    
    def __init__(self, graph, init=50):
        self.graph = graph
        
    def next_pressure(self):
        global sensor_data_lock
        global pressure_data
        sensor_data_lock.acquire()
        buf = pressure_data
        pressure_data = []
        sensor_data_lock.release()
        return buf

    def next_temperature(self):
        global sensor_data_lock
        global temperature_data
        sensor_data_lock.acquire()
        buf = temperature_data
        temperature_data = []
        sensor_data_lock.release()
        return buf

class BoundControlBox(wx.Panel):
    """ A static box with a couple of radio buttons and a text
        box. Allows to switch between an automatic mode and a 
        manual mode with an associated value.
    """
    def __init__(self, parent, ID, label, initval):
        wx.Panel.__init__(self, parent, ID)
        
        self.value = initval
        
        box = wx.StaticBox(self, -1, label)
        sizer = wx.StaticBoxSizer(box, wx.VERTICAL)
        
        self.radio_auto = wx.RadioButton(self, -1, 
            label="Auto", style=wx.RB_GROUP)
        self.radio_manual = wx.RadioButton(self, -1,
            label="Manual")
        self.manual_text = wx.TextCtrl(self, -1, 
            size=(35,-1),
            value=str(initval),
            style=wx.TE_PROCESS_ENTER)
        
        self.Bind(wx.EVT_UPDATE_UI, self.on_update_manual_text, self.manual_text)
        self.Bind(wx.EVT_TEXT_ENTER, self.on_text_enter, self.manual_text)
        
        manual_box = wx.BoxSizer(wx.HORIZONTAL)
        manual_box.Add(self.radio_manual, flag=wx.ALIGN_CENTER_VERTICAL)
        manual_box.Add(self.manual_text, flag=wx.ALIGN_CENTER_VERTICAL)
        
        sizer.Add(self.radio_auto, 0, wx.ALL, 10)
        sizer.Add(manual_box, 0, wx.ALL, 10)
        
        self.SetSizer(sizer)
        sizer.Fit(self)
    
    def on_update_manual_text(self, event):
        self.manual_text.Enable(self.radio_manual.GetValue())
    
    def on_text_enter(self, event):
        self.value = self.manual_text.GetValue()
    
    def is_auto(self):
        return self.radio_auto.GetValue()
        
    def manual_value(self):
        return self.value


class GraphFrame(wx.Frame):
    """ The main frame of the application
    """
    title = 'Demo: ECG realtime data plot'
    
    def __init__(self):
        wx.Frame.__init__(self, None, -1, self.title)
        self.create_status_bar()
        self.datagen = DataGen(self)
        self.channel_count = 2
        self.data = [[] for x in xrange(self.channel_count)]
        self.started = False
        self.bw_cfg = 0
        self.gain_cfg = 0
        self.automatic_tempRead = 0
        
        #self.create_menu()
        self.create_main_panel()
        
        self.redraw_timer = wx.Timer(self)
        self.Bind(wx.EVT_TIMER, self.on_redraw_timer, self.redraw_timer)        
        self.redraw_timer.Start(1)
  

    def create_menu(self):
        self.menubar = wx.MenuBar()
        
        menu_file = wx.Menu()
        m_expt = menu_file.Append(-1, "&Save plot\tCtrl-S", "Save plot to file")
        self.Bind(wx.EVT_MENU, self.on_save_plot, m_expt)
        menu_file.AppendSeparator()
        m_exit = menu_file.Append(-1, "E&xit\tCtrl-X", "Exit")
        self.Bind(wx.EVT_MENU, self.on_exit, m_exit)
                
        self.menubar.Append(menu_file, "&File")
        self.SetMenuBar(self.menubar)

    def create_main_panel(self):
        self.panel = wx.Panel(self)

        self.dpi = 100
        self.fig = []
        self.axes = []
        self.plot_data = []
        self.canvas = []
        for idx in range(0, self.channel_count):
            self.fig.append(Figure((3.0, 3.0), dpi=self.dpi))

            self.axes.append(self.fig[idx].add_subplot(111))
            self.axes[idx].set_axis_bgcolor('black')
            if idx == 0:
                self.axes[idx].set_title('Temperature', size=10)
            else:
                self.axes[idx].set_title('Pressure', size=10)
        
            pylab.setp(self.axes[idx].get_xticklabels(), fontsize=8)
            pylab.setp(self.axes[idx].get_yticklabels(), fontsize=8)

            # plot the data as a line series, and save the reference 
            # to the plotted line series
            #
            self.plot_data.append(self.axes[idx].plot(
                self.data[idx], 
                linewidth=1,
                color=(1, 1, 0),
                )[0])
            self.canvas.append(FigCanvas(self.panel, -1, self.fig[idx]))

        button = wx.Button(self.panel, -1, "Close")
        self.Bind(wx.EVT_BUTTON, self.OnClickClose, button)
        setParaButton = wx.Button(self.panel, -1, "Set")
        self.Bind(wx.EVT_BUTTON, self.OnClickSetPara, setParaButton)
        self.Bind(wx.EVT_CLOSE, self.OnCloseWindow)  
        
        #Parameter control text 
        self.GainText = wx.TextCtrl(self.panel, -1, "0", size=(125, -1))
        self.ChannelText = wx.TextCtrl(self.panel, -1, "1", size=(125, -1))
        self.RateText = wx.TextCtrl(self.panel, -1, "0", size=(125, -1))
        
        #Sensor value display text
        self.PressureText =  wx.TextCtrl(self.panel, -1, "0", size=(125, -1))
        self.TempretureText =  wx.TextCtrl(self.panel, -1, "0", size=(125, -1))
            
        self.vbox = wx.BoxSizer(wx.VERTICAL)
        self.FigureHbox = wx.BoxSizer(wx.HORIZONTAL)
        self.ParameterHbox = wx.BoxSizer(wx.HORIZONTAL)
        self.SensorHbox = wx.BoxSizer(wx.HORIZONTAL)
        self.buttonHbox = wx.BoxSizer(wx.HORIZONTAL)
        self.vbox.Add(self.FigureHbox, 0, flag=wx.LEFT | wx.TOP | wx.GROW)
        self.vbox.Add(self.ParameterHbox, 0, flag=wx.LEFT | wx.TOP | wx.GROW)
        self.vbox.Add(self.SensorHbox, 0, flag=wx.LEFT | wx.TOP | wx.GROW)
        self.vbox.Add(self.buttonHbox, 0, flag=wx.LEFT | wx.TOP | wx.GROW)
        
        for idx in range(0, self.channel_count):
            self.FigureHbox.Add(self.canvas[idx], 1, wx.EXPAND)

        #Add parameter control
        self.ParameterHbox.Add(wx.StaticText(self.panel, -1, "Gain",(0,0),(10,10),wx.ALIGN_RIGHT ), 1, wx.EXPAND) 
        self.ParameterHbox.Add(self.GainText, 1, wx.EXPAND)       
        self.ParameterHbox.Add(wx.StaticText(self.panel, -1, "Channel",(0,0),(10,10),wx.ALIGN_RIGHT), 1, wx.EXPAND)
        self.ParameterHbox.Add(self.ChannelText, 1, wx.EXPAND) 
        self.ParameterHbox.Add(wx.StaticText(self.panel, -1, "Rate",(0,0),(10,10),wx.ALIGN_RIGHT), 1, wx.EXPAND)
        self.ParameterHbox.Add(self.RateText, 1, wx.EXPAND) 
        
        #Add pressure and tempreture display
        self.SensorHbox.Add(wx.StaticText(self.panel, -1, "Pressure",(0,0),(10,10),wx.ALIGN_CENTER ), 1, wx.EXPAND) 
        self.SensorHbox.Add(self.PressureText, 1, wx.EXPAND)       
        self.SensorHbox.Add(wx.StaticText(self.panel, -1, "Tempreture",(0,0),(10,10),wx.ALIGN_CENTER), 1, wx.EXPAND)
        self.SensorHbox.Add(self.TempretureText, 1, wx.EXPAND) 
       
        self.buttonHbox.Add(setParaButton, 1, wx.EXPAND)
        self.buttonHbox.Add(button , 1, wx.EXPAND)

        self.panel.SetSizer(self.vbox)
        self.vbox.Fit(self)
        
        
    def create_status_bar(self):
        self.statusbar = self.CreateStatusBar()

    def draw_plot(self):
        dataLenIdx = [0,0,0,0]
        for idx in range(0, self.channel_count):
            #Redraws the plot
            # when xmin is on auto, it "follows" xmax to produce a 
            # sliding window effect. therefore, xmin is assigned after
            # xmax.
            #
            xmax = len(self.data[idx]) if len(self.data[idx]) > 500 else 500
            xmin = xmax - 500

            # for ymin and ymax, find the minimal and maximal values
            # in the data set and add a mininal margin.
            # 
            # note that it's easy to change this scheme to the 
            # minimal/maximal value in the current display, and not
            # the whole data set.
            # 
            dataLenIdx[idx] = len(self.data[idx])
            try:
                ymin = round(min(self.data[idx][dataLenIdx[idx]-500:]), 0) - 100
            except :
                ymin = 0
            try:
                ymax = round(max(self.data[idx]), 0) + 100
            except :
                ymax = 100
            

            self.axes[idx].set_xbound(lower=xmin, upper=xmax)
            self.axes[idx].set_ybound(lower=ymin, upper=ymax)
            
            # anecdote: axes.grid assumes b=True if any other flag is
            # given even if b is set to False.
            # so just passing the flag into the first statement won't
            # work.
            #
            #if self.cb_grid.IsChecked():
            #    self.axes.grid(True, color='gray')
            #else:
            #    self.axes.grid(False)
            self.axes[idx].grid(True, color='gray')

            # Using setp here is convenient, because get_xticklabels
            # returns a list over which one needs to explicitly 
            # iterate, and setp already handles this.
            #  
            #pylab.setp(self.axes.get_xticklabels(), 
            #    visible=self.cb_xlab.IsChecked())
            pylab.setp(self.axes[idx].get_xticklabels(), visible=True)
            
            self.plot_data[idx].set_xdata(np.arange(len(self.data[idx])))
            self.plot_data[idx].set_ydata(np.array(self.data[idx]))
            
            try:
                self.canvas[idx].draw()
            finally:
                pass
            
    def on_save_plot(self, event):
        file_choices = "PNG (*.png)|*.png"
        
        dlg = wx.FileDialog(
            self, 
            message="Save plot as...",
            defaultDir=os.getcwd(),
            defaultFile="plot.png",
            wildcard=file_choices,
            style=wx.SAVE)
        
        if dlg.ShowModal() == wx.ID_OK:
            path = dlg.GetPath()
            self.canvas.print_figure(path, dpi=self.dpi)
            self.flash_status_message("Saved to %s" % path)
    
    def on_redraw_timer(self, event):
        data0Len = len(self.data[0])
        data1Len = len(self.data[1])
        avgData = 0
        if data0Len > 5000:
            self.data[0] = []
        if data1Len > 5000:
            self.data[1] = []
        self.data[0] = self.data[0] + self.datagen.next_temperature()
        self.data[1] = self.data[1] + self.datagen.next_pressure()
        
        if operator.mod(data0Len,72)==0 and data0Len>0:
            avgData = sum(self.data[0][data0Len-72:data0Len])/72
            self.TempretureText.SetValue(str(avgData))
        if operator.mod(data1Len,72)==0 and data1Len>0:
            avgData = sum(self.data[1][data1Len-72:data1Len])/72
            self.PressureText.SetValue(str(avgData))
        self.draw_plot()
    
    def flash_status_message(self, msg, flash_len_ms=1500):
        self.statusbar.SetStatusText(msg)
        self.timeroff = wx.Timer(self)
        self.Bind(
            wx.EVT_TIMER, 
            self.on_flash_status_off, 
            self.timeroff)
        self.timeroff.Start(flash_len_ms, oneShot=True)
    
    def on_flash_status_off(self, event):
        self.statusbar.SetStatusText('')
    
    def OnClickSetPara(self, event):
        global cmd_write
        gain_max = 7
        channel_max = 3
        rate_max = 8
        
        #Write to and read back parameter from the host.
        gain = self.GainText.GetValue()
        channel = self.ChannelText.GetValue()
        rate = self.RateText.GetValue()
        if string.atoi(gain)>gain_max:
            gain=gain_max
            self.GainText.SetValue(str(gain_max))
        if string.atoi(channel)>channel_max:
            channel = channel_max
            self.ChannelText.SetValue(str(channel_max))
        if string.atoi(rate)>rate_max:
            rate = rate_max
            self.RateText.SetValue(str(rate_max))
        while cmd_write!=None:
            #wait until previous send finished.
            pass
        cmd_write = 'FC' + str(gain)+str(channel)+str(rate)
        
        #if current channel is tempreture channel
#        if string.atoi(channel) == 3:
#            if string.atoi(rate) == 1  :
#                self.tempretureReadtimer.Start(100)
#            elif string.atoi(rate) == 2 :
#                self.tempretureReadtimer.Start(500)
#            else:
#                self.tempretureReadtimer.Start(1000)
#        else:
#            self.tempretureReadtimer.Stop();
#        pass

    def OnClickClose(self, event):
        self.Close(True)
    
    def OnCloseWindow(self, event):
        self.Destroy()
    
        
    def __del__(self):
        self.redraw_timer.Stop()

        

class ReadThread(threading.Thread):  
    def __init__(self,port,baud):  
        threading.Thread.__init__(self)  
        global gSerialDev
        print "threading initialized"
        gSerialDev = serial.Serial(port-1, baud)
        self.sync = 0
        self.sync_last_byte = chr(0)
        if gSerialDev is None:
            raise ValueError('Read Device not found')
          
    def process_data(self, buf, start_idx):
        global sensor_data_lock
        global teamperature_data
        global pressure_data
        if ord(buf[start_idx]) & 0xF0 == 0x10:
            # data for temperature
            sensor_data_lock.acquire()
            for idx in range(0, 9):
                temperature_data.append((ord(buf[start_idx+1+idx*2])<<8) | (ord(buf[start_idx+1+idx*2+1])))
            sensor_data_lock.release()
        elif ord(buf[start_idx]) & 0xF0 == 0x20:
            # data for pressure
            sensor_data_lock.acquire()
            for idx in range(0, 6):
                pressure_data.append(int((ord(buf[start_idx+1+idx*3])<<16) | (ord(buf[start_idx+1+idx*3+1])<<8) | (ord(buf[start_idx+1+idx*3+2]))))
            sensor_data_lock.release()
        else:
            print "data format error, unknown type: " + hex(ord(buf[0]))
        
    def run(self): 
        global gSerialDev 
        global TempretureResult
        while True:
            if self.sync == 0:
                # Not synchronized yet
                buf = gSerialDev.read(1)
                if ord(self.sync_last_byte) == 0x85 and ord(buf[0]) == 0x75:
                    print "data synchronized"
                    self.sync = 1
                    buf = gSerialDev.read(19)
                    self.process_data(buf, 0)
                    continue
                else:
                    self.sync_last_byte = buf[0]
                    continue
            else:
                # Data synchronized
                buf = gSerialDev.read(21)
                if ord(buf[0]) != 0x85 or ord(buf[1]) != 0x75:
                    print "data format error, lost sync"
                    self.sync = 0
                    self.sync_last_byte = chr(0)
                    continue
                self.process_data(buf, 2)

class WriteThread(threading.Thread):  
    def __init__(self,port,baud):  
        threading.Thread.__init__(self)  
        global gSerialDev
        print "threading initialized"
        if gSerialDev is None:
            raise ValueError('Write Device not found')
          
    def run(self):
        global cmd_write  
        global gSerialDev
       
        while True:  
            if cmd_write != None:
                gSerialDev.write(cmd_write)
                cmd_write = None
                print "write success\n"
                pass
            
if __name__ == '__main__':
    port = 38
    baud  = 115200
    #Start read thread first and then start write thread
    ReadThread(port, baud).start();
    WriteThread(port, baud).start(); 
    app = wx.PySimpleApp()
    app.frame = GraphFrame()
    app.frame.Show(True)
    app.MainLoop()
