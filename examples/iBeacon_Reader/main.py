################################################################################
# iBeacon Reader
#
# Created by Zerynth Team 2019 CC
# Author: G. Baldi
###############################################################################

import streams
#import the ESP32 BLE driver: a BLE capable VM is also needed!
from espressif.esp32ble import esp32ble as bledrv
# then import the BLE modue
from wireless import ble
from wireless import ble_beacons as bb


streams.serial()


# Let's define some callbacks


def scan_report_cb(data):
    pdata = data[3]
    rssi = data[2]
    try:
        uuid, major, minor, tx = bb.ibeacon_decode(pdata)
        print("iBeacon found with",[hex(x) for x in uuid],major,minor,tx,rssi)
    except Exception as e:
        print("::")


def scan_stop_cb(data):
    print("Scan stopped")
    #let's start it up again
    ble.start_scanning(3000)

try:
    # initialize BLE driver
    bledrv.init()

    # Set GAP name and no security
    ble.gap("Zerynth",security=(ble.SECURITY_MODE_1,ble.SECURITY_LEVEL_1))

    ble.add_callback(ble.EVT_SCAN_REPORT,scan_report_cb)
    ble.add_callback(ble.EVT_SCAN_STOPPED,scan_stop_cb)

    #set scanning parameters: every 100ms for 100ms and no duplicates
    ble.scanning(100,100,duplicates=0)

    # Start the BLE stack
    ble.start()

    # Now start scanning for 3 seconds
    ble.start_scanning(3000)

except Exception as e:
    print(e)

# loop forever
while True:
    print(".")
    sleep(10000)

