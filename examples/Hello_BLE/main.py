def show_passkey_cb(data):
    print("PIN CODE TO INSERT:",data)

def match_passkey_cb(data):
    print("PIN CODE TO CHECK:",data)
    # in order to continue with authentication, the pin must be confirmed by the user.
    # It can be done by calling ble.confirm_passkey(1) for Yes and ble.confirm_passkey(0) for No.
    # Let's confirm here without user interaction (don't do it at home)
    ble.confirm_passkey(1)
def value_cb(status,val):
    print("Value changed to",val)

def connection_cb(address):
    print("Connected to",ble.btos(address))

def disconnection_cb(data):
    print("Disconnected from",ble.btos(address))
    # let's start advertising again
    ble.start_advertising()


