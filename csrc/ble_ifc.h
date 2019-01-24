#ifndef __BLE_ESP32_IFC__
#define __BLE_ESP32_IFC__

#include "zerynth.h"
#include "bt.h"
#include "bta_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"


#define NUM_BLE_EVTS 16
#define BLE_GATT_EVENT 1
#define BLE_GAP_EVENT 0
#define BLE_GAP_EVT_CONNECTED 0
#define BLE_GAP_EVT_DISCONNECTED 1
#define BLE_GAP_EVT_SCAN_REPORT 2
#define BLE_GAP_EVT_SCAN_STARTED 3
#define BLE_GAP_EVT_SCAN_STOPPED 4
#define BLE_GAP_EVT_SHOW_PASSKEY 5
#define BLE_GAP_EVT_MATCH_PASSKEY 6
#define BLE_GAP_EVT_AUTH_FAILED 7
#define BLE_GAP_EVT_ADV_STARTED 8
#define BLE_GAP_EVT_ADV_STOPPED 9

#define HANDLE(x) (ble.handles[x])
#define CHANDLE() (ble.handles[ble.hh_created])
#define SERVICE(x) (ble.services[x])
#define CSERVICE() (ble.services[ble.srv_created])
#define BLE_SERVICE 0
#define BLE_DECL 1
#define BLE_VAL 2
#define BLE_CCCD 3
#define BLE_PERM_NOTIFY 16
#define BLE_PERM_INDICATE 32
#define BLE_PERM_WRITE 8
#define ESP_ERR_EXC(ee,msg,exc) do { \
    if (ee){                    \
        printf(msg);            \
        printf("%i\n",ee);      \
        return exc;             \
    }                           \
    } while(0)

#define CAP_OUT  0
#define CAP_IO  1
#define CAP_KB  2
#define CAP_NONE  3
#define CAP_KBD  4
#define AUTH_NO_BOND  1
#define AUTH_BOND  1
#define AUTH_MITM  2
#define AUTH_SC  4
#define AUTH_KEYPRESS  8
#define KEY_ENC  1
#define KEY_ID   2
#define KEY_CSR 4
#define KEY_LNK 8

typedef struct _ble_event {
    uint8_t type;
    uint8_t status;
    uint16_t service;
    uint16_t characteristic;
    void* object;
} BLEEvent;

typedef union _cool_uuid {
    uint8_t full_uuid[16];
    struct {
        uint8_t unused12[12];
        uint16_t uuid16;
        uint8_t unused2[2];
    };
} BLEUUID;

typedef struct _ble_char {
    uint16_t ccc;
    uint8_t vsize;
    uint8_t usize;
    uint8_t value[20];
    BLEUUID uuid;
    uint16_t perm;
    uint16_t hpos;
} BLECharacteristic;

typedef struct _ble_service {
    uint16_t nhandles;
    uint8_t nchar;
    uint8_t usize;
    BLEUUID uuid;
    BLECharacteristic *chars;
    esp_gatts_attr_db_t *srv;
} BLEService;

typedef struct _ble_handle {
    uint16_t handle;
    uint8_t srv;
    uint8_t chr;
    uint8_t pos;
    uint8_t type;
} BLEHandle;

typedef struct _ble_scan_res {
    uint8_t data[32];
    uint8_t addr[6];
    uint8_t datalen;
    uint8_t addrtype;
    int16_t rssi;
    uint8_t scantype;
    uint8_t unused;
} BLEScanResult;

typedef struct _ble_device {
    uint8_t* name;
    uint32_t namelen;
    uint32_t appearance;
    uint32_t level;
    uint32_t security;
    uint32_t min_conn;
    uint32_t max_conn;
    uint32_t latency;
    uint32_t conn_sup;
    uint32_t adv_mode;
    uint32_t advertising_timeout;
    uint32_t advert_data_len;
    uint32_t scanrsp_data_len;
    uint32_t nserv;
    uint32_t nchar;
    uint32_t nhandles;
    int8_t adv_data_set;
    int8_t scan_data_set;
    int8_t connected;
    int8_t srv_created;
    int16_t evt_p;
    int16_t evt_n;
    uint32_t gatts_if;
    uint32_t connid;
    esp_bd_addr_t local_bda;
    esp_bd_addr_t remote_bda;
    esp_bd_addr_t pairing_bda;
    uint8_t advert_data[31];
    int8_t scan_started;
    uint8_t scanrsp_data[31];
    int8_t hh_created;
    uint32_t capabilities;
    uint32_t scheme;
    uint32_t key_size;
    uint32_t bonding;
    uint32_t init_key;
    uint32_t resp_key;
    uint32_t oob;
    uint32_t passkey;
    BLEEvent events[NUM_BLE_EVTS];
    BLEHandle *handles;
    BLEService *services;
} BLEDevice;


#endif
