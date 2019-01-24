################################################################################
# Eddystone Beacon
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

adv_content = 0
battery_level = 40
temperature = 23.2
pdu_count = 0
uptime = timers.timer()
uptime.start()

# create payloads to cycle through
payloads = [
    bb.eddy_encode_uid("Zerynth","Python",-69),                                 # UID Eddystone payload
    bb.eddy_encode_url("https://www.zerynth.com",-69),                          # URL Eddystone payload
    bb.eddy_encode_tlm(battery_level,temperature,pdu_count,uptime.get()/1000)  # TLM Eddystone payload
]

# this callback will be called at the end of an advertising cycle.
# it is used to switch to the next content
def adv_stop_cb(data):
    global pdu_count,adv_content
    print("Advertising stopped")
    adv_content = (adv_content+1)%3
    if adv_content == 0:
        # advertise UID
        interval = 100
        timeout = 10000
    elif adv_content == 1:
        # advertise URL
        interval = 100
        timeout = 15000
    else:
        # advertise TLM
        interval = 100
        timeout = 150
        pdu_count+=1
        payloads[2] = bb.eddy_encode_tlm(battery_level,temperature,pdu_count,uptime.get()/1000)  # TLM Eddystone payload
    
    payload = payloads[adv_content]
    ble.advertising(interval,timeout=timeout,payload=payload,mode=ble.ADV_UNCN_UND)
    ble.start_advertising()
    print("Advertising restarted with",ble.btos(payload))


try:
    # initialize BLE driver
    bledrv.init()

    # Set GAP name and no security
    ble.gap("Zerynth",security=(ble.SECURITY_MODE_1,ble.SECURITY_LEVEL_1))
    ble.add_callback(ble.EVT_ADV_STOPPED,adv_stop_cb)
    
    # set advertising options: advertise every second with custom payload in non connectable undirected mode
    # after 10 seconds, stop and change payload
    ble.advertising(100,timeout=10000,payload=payloads[adv_content],mode=ble.ADV_UNCN_UND)

    # Start the BLE stack
    ble.start()

    # Now start scanning for 30 seconds
    ble.start_advertising()

except Exception as e:
    print(e)

# loop forever
while True:
    print(".")
    sleep(10000)


