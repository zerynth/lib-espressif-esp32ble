################################################################################
# BLE Wifi
#
# Created by Zerynth Team 2019 CC
# Author: G. Baldi
###############################################################################

import streams
#import the ESP32 BLE driver: a BLE capable VM is also needed!
from espressif.esp32ble import esp32ble as bledrv
# then import the BLE modue
from wireless import ble
#  import wifi modules 
from espressif.esp32net import esp32wifi as wifi_driver
from wireless import wifi
import socket
import gc

streams.serial()


# Let's define some callbacks and constants

# How long to scan for in milliseconds
scan_time=30000
# tcp socket
ss=None

def scan_report_cb(data):
    try:
        print("Detected packet from",ble.btos(data[4]),"containing",ble.btos(data[3]))
        print("         packet is of type",data[0],"while address is of type",data[1])
        print("         remote device has RSSI of",data[2])
        # send to socket
        ss.sendall(ble.btos(data[3]))
        ss.sendall("\n")
    except Exception as e:
        print("send",e)
    
def scan_start_cb(data):
    print("Scan started")

def scan_stop_cb(data):
    print("Scan stopped")
    #let's start it up again
    ble.start_scanning(scan_time)

try:
    # initialize wifi
    wifi_driver.auto_init()

    for i in range(10):
        try:
            print('connecting to wifi...')
            # place here your wifi configuration
            wifi.link("SSID",wifi.WIFI_WPA2,"password")
            break
        except Exception as e:
            print(e)
    
        
    
    # let's open a socket to forward the BLE packets
    print("Opening socket...")
    ss=socket.socket()
    # you can run "nc -l -p 8082" on your machine
    # and change the ip below
    ss.connect(("192.168.71.52",8082))
    

    print("Starting BLE...")
    # initialize BLE driver
    bledrv.init()

    # Set GAP name and no security
    ble.gap("Zerynth",security=(ble.SECURITY_MODE_1,ble.SECURITY_LEVEL_1))

    ble.add_callback(ble.EVT_SCAN_REPORT,scan_report_cb)
    ble.add_callback(ble.EVT_SCAN_STARTED,scan_start_cb)
    ble.add_callback(ble.EVT_SCAN_STOPPED,scan_stop_cb)

    #set scanning parameters: every 100ms for 50ms and no duplicates
    ble.scanning(100,50,duplicates=0)

    # Start the BLE stack
    ble.start()

    # Now start scanning for 30 seconds
    ble.start_scanning(scan_time)

except Exception as e:
    print(e)

# loop forever
while True:
    print(".",gc.info())
    sleep(10000)
    ss.sendall("::\n")
