'''
Created on 2013-8-16

@author: wenli
'''
import threading
#Global Variable


sensor_data_lock = threading.Lock()
temperature_data = []
pressure_data = []
parameterData = []

cmd_write = None
gSerialDev = None
TempretureResult = 0

isReadExit = True
isWriteExit = True
    
