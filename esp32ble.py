"""
.. module:: esp32ble

*********
ESP32 BLE
*********



This module implements the ESP32 ble driver for peripheral roles. 
The driver is based on ESP32 IDF 3.2 and can work on Virtual Machines compiled with support for ESP32 BLE.

To use the module expand on the following example: ::

    from espressif.esp32ble import esp32ble as bledrv
    from wireless import ble

    bledrv.init()

    ble.gap("Zerynth")
    ble.start()

    while True:
        sleep(1000)
        # do things here

    """


@native_c("_ble_init",["csrc/ble_ifc.c"],["VHAL_BLE"])
def _sd_init():
    pass


def init():
    __builtins__.__default_net["ble"] = __module__
    _sd_init()


@native_c("_ble_start",["csrc/ble_ifc.c"],[])
def _start():
    pass


@native_c("_ble_get_event",["csrc/ble_ifc.c"],[])
def _get_event():
    pass



def _event_loop(services,callbacks):
    try:
        while True:
            evt_type, service, char, status, data = _get_event()
            if evt_type == 0: # GAP EVENT
                if status in callbacks and callbacks[status]:
                    try:
                        callbacks[status](data)
                    except:
                        pass
            elif evt_type == 1: # GATT EVENT
                if service in services:
                    srv = services[service]
                    if char in srv.chs:
                        ch = srv.chs[char]
                        if ch.fn is not None:
                            if status==8: # ble.WRITE
                                value = get_value(service,char)
                                val =ch.convert(value)
                            else:
                                val=None
                            try:
                                ch.fn(status,val)
                            except Exception as e:
                                print(e)
                                pass
    except Exception as e:
        pass

def start(services,callbacks):
    _start()
    thread(_event_loop,services,callbacks)


@native_c("_ble_set_gap",["csrc/ble_ifc.c"],[])
def gap(name,security,level,appearance,min_conn,max_conn,latency,conn_sup):
    pass


@native_c("_ble_set_services",["csrc/ble_ifc.c"],[])
def services(servlist):
    pass

@native_c("_ble_set_security",["csrc/ble_ifc.c"],[])
def security(capabilities,bonding,scheme,key_size,initiator,responder,oob,passkey):
    pass

@native_c("_ble_get_bonded",["csrc/ble_ifc.c"],[])
def bonded():
    pass

@native_c("_ble_del_bonded",["csrc/ble_ifc.c"],[])
def remove_bonded(addr):
    pass

@native_c("_ble_confirm_passkey",["csrc/ble_ifc.c"],[])
def confirm_passkey(confirmed):
    pass

@native_c("_ble_set_advertising",["csrc/ble_ifc.c"],[])
def advertising(interval,timeout,payload,mode,scanrsp):
    pass


@native_c("_ble_start_advertising",["csrc/ble_ifc.c"],[])
def start_advertising():
    pass


@native_c("_ble_stop_advertising",["csrc/ble_ifc.c"],[])
def stop_advertising():
    pass

@native_c("_ble_set_scanning",["csrc/ble_ifc.c"],[])
def scanning(interval,window,duplicate,filter,addr,active):
    pass


@native_c("_ble_start_scanning",["csrc/ble_ifc.c"],[])
def start_scanning(duration):
    pass


@native_c("_ble_stop_scanning",["csrc/ble_ifc.c"],[])
def stop_scanning():
    pass


@native_c("_ble_set_value",["csrc/ble_ifc.c"],[])
def set_value(service, char, value):
    pass

@native_c("_ble_get_value",["csrc/ble_ifc.c"],[])
def get_value(service,char):
    pass

