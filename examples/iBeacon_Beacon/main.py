################################################################################
# iBeacon
#
# Created by Zerynth Team 2019 CC
# Author: G. Baldi
###############################################################################

import streams
import timers
#import the ESP32 BLE driver: a BLE capable VM is also needed!
from espressif.esp32ble import esp32ble as bledrv
# then import the BLE module and beacons
from wireless import ble
from wireless import ble_beacons as bb

streams.serial()
try:
    # initialize BLE driver
    bledrv.init()

    # Set GAP name and no security
    ble.gap("Zerynth",security=(ble.SECURITY_MODE_1,ble.SECURITY_LEVEL_1))
    
    # set advertising options: advertise every second with custom payload in non connectable undirected mode
    ble.advertising(20,payload=bb.ibeacon_encode("fb0b57a2-8228-44cd-913a-94a122ba1206",10,3,-69),mode=ble.ADV_UNCN_UND)

    # Start the BLE stack
    ble.start()

    # Now start advertising
    ble.start_advertising()

except Exception as e:
    print(e)

# loop forever
while True:
    print(".")
    sleep(10000)


