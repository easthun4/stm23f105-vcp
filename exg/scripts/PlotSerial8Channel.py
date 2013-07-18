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

ecg_data_lock = threading.Lock()
ecg_data = []

# The recommended way to use wx with mpl is with the WXAgg
# backend. 
#
import matplotlib
matplotlib.use('WXAgg')
from matplotlib.figure import Figure
from matplotlib.backends.backend_wxagg import \
    FigureCanvasWxAgg as FigCanvas, \
    NavigationToolbar2WxAgg as NavigationToolbar
import numpy as np
import pylab


class DataGen(object):
    """ A silly class that generates pseudo-random data for
        display in the plot.
    """
    def __init__(self, graph, init=50):
        self.data = self.init = init
        self.graph = graph
        self.in_sync = 0
        self.count = 0
        self.high_val = 0
        self.low_val = 0
        self.f = []
        self.do_record = 0
        for idx in range(0, 8):
            self.f.append(open(str(idx) + ".log", 'w'))
        os.path
        
    def __next(self):
        global ecg_data
        global ecg_data_lock
        idx = 0
        try_next = 0
        data_buf = [[] for x in xrange(8)]

        # Get next bunch of data
        ecg_data_lock.acquire()
        if len(ecg_data) > 0:
            buf = ecg_data[0]
            del ecg_data[0]
            try_next = 1
        else:
            ecg_data_lock.release()
            return (try_next, data_buf)
        ecg_data_lock.release()

        # print "got data from serial, length: " + str(len(buf))
        if self.in_sync == 0:
            idx = 0;
            while True:
                if ord(buf[idx]) == 0 and ord(buf[idx + 1]) == 0 and (ord(buf[idx + 2]) & (1 << 7)) == 0x80:
                    break
                idx = idx + 1;
                if idx > (1024 - 3):
                    print "BUG at next"
            self.in_sync = 1;
            self.count = 0;    
            print "Got synced"

        # Handle the data bytes
        while idx < 1024:        
            if self.count >= 2 and self.count % 2 == 0:
                if self.count == 2 and (ord(buf[idx]) & 0x80) == 0:
                    print "start resync"
                    self.in_sync = 0;
                    return (try_next, data_buf)
                #elif self.count != 2 and (ord(buf[idx]) & 0x80) != 0:
                #    print "BUG"
                self.high_val = ord(buf[idx]) & 0x7F;   
                if self.count == 2:
                    self.do_record = (self.do_record + 1) % 2;
            elif self.count >= 2 and self.count % 2 == 1:
                self.low_val = ord(buf[idx])
                channel = (self.count - 2 - 1) / 2
                data_buf[channel].append((self.high_val) << 8 | self.low_val)
                if self.do_record == 1:
                    value_str = str((self.high_val) << 8 | self.low_val) + "\n"
                    self.f[channel].write(value_str)

            idx = idx + 1;
            self.count = (self.count + 1) % 18;
        return (try_next, data_buf)

    def next(self):
        (try_next, data_buf) = self.__next()
        while try_next != 0:
            (try_next, buf) = self.__next()
            for i in range(0, 8):
                data_buf[i] = data_buf[i] + buf[i]
        return data_buf

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

        # find our Serial device
        #self.dev = serial.Serial(17)
        # was it found?
        #if self.dev is None:
        #    raise ValueError('Device not found')

        self.create_status_bar()
        self.total_channel = 8
        self.datagen = DataGen(self)
        self.data = [[] for x in xrange(self.total_channel)]
        self.started = False
        self.bw_cfg = 0
        self.gain_cfg = 0
        
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

        self.init_plot()
        self.canvas = []
        for i in range(0, self.total_channel):
            self.canvas.append(FigCanvas(self.panel, -1, self.fig[i]))

        self.hbox = wx.BoxSizer(wx.VERTICAL)
        self.hbox.Add(self.canvas[0], 1, flag=wx.LEFT | wx.TOP | wx.GROW)        
        self.hbox.Add(self.canvas[1], 1, flag=wx.LEFT | wx.TOP | wx.GROW)        
        self.hbox.Add(self.canvas[2], 1, flag=wx.LEFT | wx.TOP | wx.GROW)        
        self.hbox.Add(self.canvas[3], 1, flag=wx.LEFT | wx.TOP | wx.GROW)        
        self.hbox.Add(self.canvas[4], 1, flag=wx.LEFT | wx.TOP | wx.GROW)        
        self.hbox.Add(self.canvas[5], 1, flag=wx.LEFT | wx.TOP | wx.GROW)        
        self.hbox.Add(self.canvas[6], 1, flag=wx.LEFT | wx.TOP | wx.GROW)        
        self.hbox.Add(self.canvas[7], 1, flag=wx.LEFT | wx.TOP | wx.GROW)        

        self.panel.SetSizer(self.hbox)
        self.hbox.Fit(self)
    
    def create_status_bar(self):
        self.statusbar = self.CreateStatusBar()

    def init_plot(self):
        self.dpi = 100
        self.fig = []
        self.axes = []
        self.plot_data = []
        for idx in range(0, self.total_channel):
            self.fig.append(Figure((10.0, 2.0), dpi=self.dpi))

            self.axes.append(self.fig[idx].add_subplot(111))
            self.axes[idx].set_axis_bgcolor('black')
            self.axes[idx].set_title('ECG Realtime Data Plot ' + str(idx), size=10)
        
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

    def draw_plot(self):
        for idx in range(0, self.total_channel):
            """ Redraws the plot
            """
            # when xmin is on auto, it "follows" xmax to produce a 
            # sliding window effect. therefore, xmin is assigned after
            # xmax.
            #
            xmax = len(self.data[idx]) if len(self.data[idx]) > 5000 else 5000
            xmin = xmax - 5000

            # for ymin and ymax, find the minimal and maximal values
            # in the data set and add a mininal margin.
            # 
            # note that it's easy to change this scheme to the 
            # minimal/maximal value in the current display, and not
            # the whole data set.
            # 
            ymin = round(min(self.data[idx][-5000:-1]), 0)
            ymax = round(max(self.data[idx][-5000:-1]), 0)

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
            pylab.setp(self.axes[idx].get_xticklabels(), 
                visible=True)
            
            self.plot_data[idx].set_xdata(np.arange(len(self.data[idx])))
            self.plot_data[idx].set_ydata(np.array(self.data[idx]))
            
            self.canvas[idx].draw()
    
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
        new_data = self.datagen.next()
        for i in range(0, self.total_channel):
            self.data[i] = self.data[i] + new_data[i]
        self.draw_plot()
    
    def on_exit(self, event):
        self.Destroy()
    
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

class MyThread(threading.Thread):  
    def __init__(self):  
        threading.Thread.__init__(self)  
        print "threading initialized"
        self.dev = serial.Serial(44, 115200)
        if self.dev is None:
            raise ValueError('Device not found')
          
    def run(self):  
        global ecg_data
        global ecg_data_lock
        i = 0
        while True:
            buf = self.dev.read(1024)
            if i < 100:
                i = i + 1
            else:
                i = 0
            ecg_data_lock.acquire()
            ecg_data.append(buf)
            ecg_data_lock.release()


if __name__ == '__main__':
    MyThread().start();#start the 1st thread  
    app = wx.PySimpleApp()
    app.frame = GraphFrame()
    app.frame.Show()
    app.MainLoop()
