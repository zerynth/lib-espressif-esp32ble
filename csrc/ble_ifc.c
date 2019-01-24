// #define ZERYNTH_PRINTF
#include "ble_ifc.h"

VSemaphore bleevt=NULL;
VSysTimer _ble_adv_tm=NULL;

//BLE constants
uint16_t _ble_srv_primary = 0x2800;
uint16_t _ble_char_decl = 0x2803;
uint16_t _ble_cccd = 0x2902;

//some status structs
BLEDevice ble;

esp_ble_adv_params_t _ble_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

esp_ble_adv_data_t _ble_adv_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data =  NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 32,
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

esp_ble_scan_params_t _ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x10,
    .scan_window            = 0x10,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};


int _retrieve_from_handle(uint16_t handle){
    int i;

    for(i=0;i<ble.nhandles;i++){
        if (HANDLE(i).handle==handle) return i;
    }
    return -1;
}

int _ble_get_evt(BLEEvent *blee){
    int filled = 0;
    vosSysLock();
    if(ble.evt_n<=0) {
        //empty ring, nothing to do...
    } else {
        int pos = (NUM_BLE_EVTS+ble.evt_p-ble.evt_n)%NUM_BLE_EVTS;
        memcpy(blee,&ble.events[pos],sizeof(BLEEvent));
        ble.evt_n--;
        filled=1;
    }
    vosSysUnlock();
    return filled;
}

int _ble_put_evt(BLEEvent *blee){
    int new_in =0 ;
    vosSysLock();
    ble.evt_n++;
    if(ble.evt_n>NUM_BLE_EVTS) {
        //can't add new event without losing oldest one
        //not adding!
        ble.evt_n=NUM_BLE_EVTS;
    } else {
        new_in=1;
        memcpy(&ble.events[ble.evt_p],blee,sizeof(BLEEvent));
        ble.evt_p=(ble.evt_p+1)%NUM_BLE_EVTS;
    }
    vosSysUnlock();
    if (new_in) vosSemSignal(bleevt);
    return new_in;
}

/********************* callbacks */

void _ble_adv_stopper(void*args){
    (void)args;
    esp_ble_gap_stop_advertising();
}
// void show_bonded_devices(void)
// {
//     int dev_num = esp_ble_get_bond_device_num();

//     esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)gc_malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
//     esp_ble_get_bond_device_list(&dev_num, dev_list);
//     // ESP_LOGI(GATTS_TABLE_TAG, "Bonded devices number : %d\n", dev_num);

//     // ESP_LOGI(GATTS_TABLE_TAG, "Bonded devices list : %d\n", dev_num);
//     for (int i = 0; i < dev_num; i++) {
//         printf("bonded %i\n",i);
//         // esp_log_buffer_hex(GATTS_TABLE_TAG, (void *)dev_list[i].bd_addr, sizeof(esp_bd_addr_t));
//     }

//     gc_free(dev_list);
// }
void _ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    printf("GAP HANDLER %i\n",event);
    switch (event) {
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        ble.adv_data_set++;
        break;
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        ble.scan_data_set++;
        break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: {
            if(param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                // ESP_LOGE(DEMO_TAG,"Scan start failed: %s", esp_err_to_name(err));
                ble.scan_started=-1;
                // printf("scan failed\n");
            }
            else {
                // ESP_LOGI(DEMO_TAG,"Start scanning...");
                // printf("scan started\n");
                ble.scan_started=1;
                BLEEvent blee = {BLE_GAP_EVENT,BLE_GAP_EVT_SCAN_STARTED,0,0};
                _ble_put_evt(&blee);
            }
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t* scan_result = (esp_ble_gap_cb_param_t*)param;
        switch(scan_result->scan_rst.search_evt)
        {
            case ESP_GAP_SEARCH_INQ_RES_EVT: {
                BLEScanResult *scr = gc_malloc(sizeof(BLEScanResult));
                if (!scr) break;
                memcpy(scr->data,scan_result->scan_rst.ble_adv,scan_result->scan_rst.adv_data_len);
                scr->datalen = scan_result->scan_rst.adv_data_len;
                scr->rssi = scan_result->scan_rst.rssi;
                memcpy(scr->addr,scan_result->scan_rst.bda,6);
                scr->addrtype = scan_result->scan_rst.ble_addr_type;
                scr->scantype = scan_result->scan_rst.ble_evt_type;

                // printf("Packet received\n");
                // printf(":: %i bytes\n",scr->datalen);
                // printf(":: %x:%x:%x:%x:%x:%x | %i %i\n",scr->addr[0],scr->addr[1],scr->addr[2],scr->addr[3],scr->addr[4],scr->addr[5],scr->addrtype,scr->scantype);
                // printf(":: %i rssi\n",scr->rssi);

                BLEEvent blee = {BLE_GAP_EVENT,BLE_GAP_EVT_SCAN_REPORT,0,0,scr};
                if(!_ble_put_evt(&blee)){
                    //lost packet!
                    gc_free(scr);
                }
                break;
            }
            case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            case ESP_GAP_SEARCH_DISC_CMPL_EVT:
            case ESP_GAP_SEARCH_DI_DISC_CMPL_EVT:
            case ESP_GAP_SEARCH_SEARCH_CANCEL_CMPL_EVT:{
                BLEEvent blee = {BLE_GAP_EVENT,BLE_GAP_EVT_SCAN_STOPPED,0,0};
                _ble_put_evt(&blee);
            }
            break;
            default:
                break;
        }
        break;
    }
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:{
        if(param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            // ESP_LOGE(DEMO_TAG,"Scan stop failed: %s", esp_err_to_name(err));
            // printf("scan stop failed\n");
        }
        else {
            // printf("scan stopped\n");
            ble.scan_started=0;
            BLEEvent blee = {BLE_GAP_EVENT,BLE_GAP_EVT_SCAN_STOPPED,0,0};
            _ble_put_evt(&blee);
        }
        break;
    }
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            // printf("Adv failed\n");
        } else {
            // printf("Adv started\n");
            if (ble.advertising_timeout) {
                //setup timer for advertising termination
                vosSysLock();
                vosTimerOneShot(_ble_adv_tm,TIME_U(ble.advertising_timeout,MILLIS),_ble_adv_stopper,NULL);
                vosSysUnlock();
                // printf("Timer armed\n");
                BLEEvent blee = {BLE_GAP_EVENT,BLE_GAP_EVT_ADV_STARTED,0,0};
                _ble_put_evt(&blee);
            }
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            // printf("Adv stop failed\n");
            // ESP_LOGE(GATTS_TAG, "Advertising stop failed\n");
        } else {
            // printf("Adv stop success\n");
            // ESP_LOGI(GATTS_TAG, "Stop adv successfully\n");
            BLEEvent blee = {BLE_GAP_EVENT,BLE_GAP_EVT_ADV_STOPPED,0,0};
            _ble_put_evt(&blee);
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        // printf("Update conn params\n");
         // ESP_LOGI(GATTS_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
         //          param->update_conn_params.status,
         //          param->update_conn_params.min_int,
         //          param->update_conn_params.max_int,
         //          param->update_conn_params.conn_int,
         //          param->update_conn_params.latency,
         //          param->update_conn_params.timeout);
        break;
// SECURITY FLOW
     case ESP_GAP_BLE_PASSKEY_REQ_EVT:                           /* passkey request event */
        // ESP_LOGI(GATTS_TABLE_TAG, "ESP_GAP_BLE_PASSKEY_REQ_EVT");
        // printf("PASSKEY request\n");
        //esp_ble_passkey_reply(heart_rate_profile_tab[HEART_PROFILE_APP_IDX].remote_bda, true, 0x00);
        break;
    case ESP_GAP_BLE_OOB_REQ_EVT:                                /* OOB request event */
        // ESP_LOGI(GATTS_TABLE_TAG, "ESP_GAP_BLE_OOB_REQ_EVT");
        // printf("OOB request\n");
        break;
    case ESP_GAP_BLE_LOCAL_IR_EVT:                               /* BLE local IR event */
        // ESP_LOGI(GATTS_TABLE_TAG, "ESP_GAP_BLE_LOCAL_IR_EVT");
        // printf("local IR\n");
        break;
    case ESP_GAP_BLE_LOCAL_ER_EVT:                               /* BLE local ER event */
        // printf("local ER\n");
        break;
    case ESP_GAP_BLE_NC_REQ_EVT:{
        /* The app will receive this evt when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
        show the passkey number to the user to confirm it with the number displayed by peer deivce. */
        // ESP_LOGI(GATTS_TABLE_TAG, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%d", param->ble_security.key_notif.passkey);
        // printf("YesNo passkey\n");
        memcpy(ble.pairing_bda,param->ble_security.auth_cmpl.bd_addr,6);
        BLEEvent blee = {BLE_GAP_EVENT,BLE_GAP_EVT_MATCH_PASSKEY,0,0,PSMALLINT_NEW(param->ble_security.key_notif.passkey)};
        _ble_put_evt(&blee);
        }
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        /* send the positive(true) security response to the peer device to accept the security request.
        If not accept the security request, should sent the security response with negative(false) accept value*/
        // printf("SEC REQ\n");
        // esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:{
            ///the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
            ///show the passkey number to the user to input it in the peer deivce.
            // ESP_LOGI(GATTS_TABLE_TAG, "The passkey Notify number:%06d", param->ble_security.key_notif.passkey);
            // printf("PASSKEY SHOW\n");
            BLEEvent blee = {BLE_GAP_EVENT,BLE_GAP_EVT_SHOW_PASSKEY,0,0};
            _ble_put_evt(&blee);
        }
        break;
    case ESP_GAP_BLE_KEY_EVT:
        //shows the ble key info share with peer device to the user.
        // ESP_LOGI(GATTS_TABLE_TAG, "key type = %s", esp_key_type_to_str(param->ble_security.ble_key.key_type));
        // printf("kEY EVT\n");
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        if(!param->ble_security.auth_cmpl.success) {
            // printf("auth fail %i\n",param->ble_security.auth_cmpl.fail_reason);
            BLEEvent blee = {BLE_GAP_EVENT,BLE_GAP_EVT_AUTH_FAILED,0,0};
            _ble_put_evt(&blee);
        } else {
            // printf("auth ok\n");
        }
        // show_bonded_devices();
        break;
    }
    case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT: {
        // printf("Bond removed\n");
        break;
    }
    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:
        // printf("local privacy\n");
        if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS){
            // printf("local privacy failed\n");
            break;
        }
    break;
    default:
        break;
    }
}


void _ble_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param){
    printf("GATT %i\n",event);

    esp_err_t ee = ESP_OK;
    ble.gatts_if = gatts_if;
    switch (event) {
        case ESP_GATTS_REG_EVT:
            // printf("REG EVT\n");
            esp_ble_gap_config_local_privacy(true);
            ee = esp_ble_gatts_create_attr_tab(CSERVICE().srv, gatts_if, CSERVICE().nhandles, 0);
        break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
            if (param->add_attr_tab.status != ESP_GATT_OK){
                // printf("Failed attr tab %x\n",param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != CSERVICE().nhandles){
                // printf("Failed attr tab2 %x\n",param->add_attr_tab.num_handle);
            }
            else {
                // printf("tab ok %i\n",param->add_attr_tab.num_handle);
                int i;
                for(i=0;i<CSERVICE().nhandles;i++){
                    CHANDLE().handle = param->add_attr_tab.handles[i];
                    ble.hh_created++;
                }
                esp_ble_gatts_start_service(param->add_attr_tab.handles[0]);
            }
            ble.srv_created++;
            if (ble.srv_created<ble.nserv) {
                ee = esp_ble_gatts_create_attr_tab(CSERVICE().srv, gatts_if, CSERVICE().nhandles, 0);
            }
            break;
        }
        case ESP_GATTS_START_EVT:
            // printf("Service started %i %i\n",param->start.status, param->start.service_handle);
        break;
        case ESP_GATTS_CONNECT_EVT:{
                printf("Connect\n");
                if(ble.level>1) {
                    //TODO: set correct encryption
                    esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
                }
                ble.connid = param->connect.conn_id;
                esp_ble_conn_update_params_t conn_params = {0};
                memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
                memcpy(ble.remote_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
                /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
                conn_params.latency = (int)(ble.latency/1.25f);
                conn_params.max_int = _ble_adv_data.min_interval;    // max_int = 0x20*1.25ms = 40ms
                conn_params.min_int = _ble_adv_data.max_interval;    // min_int = 0x10*1.25ms = 20ms
                conn_params.timeout = (int)(ble.conn_sup/1.25f);    // timeout = 400*10ms = 4000ms
                //start sent the update connection parameters to the peer device.
                esp_ble_gap_update_conn_params(&conn_params);
                ble.connected = 1;
                BLEEvent blee = {BLE_GAP_EVENT,BLE_GAP_EVT_CONNECTED,0,0};
                _ble_put_evt(&blee);
            }
            break;
        case ESP_GATTS_DISCONNECT_EVT:{
                printf("Disconnect\n");
                esp_ble_gap_disconnect(ble.remote_bda);
                //remember to restart advertising after disconnect! 
                // esp_ble_gap_start_advertising(&_ble_adv_params);
                ble.connected=0;
                BLEEvent blee = {BLE_GAP_EVENT,BLE_GAP_EVT_DISCONNECTED,0};
                _ble_put_evt(&blee);
            }
            break;
        case ESP_GATTS_WRITE_EVT:
            if (!param->write.is_prep){
                // the data length of gattc write  must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
                // printf("NOT WRITE_PREP\n");
                int hp = _retrieve_from_handle(param->write.handle);
                if(hp<0) {
                    // printf("Unknown handle\n");
                    break;
                }
                BLEHandle *bl = &HANDLE(hp);
                BLEService *s = &SERVICE(bl->srv);
                // printf("Found srv %i nch %i ndb %i is_cccd %i for handle %x\n",bl->srv,bl->chr,bl->pos,bl->type,param->write.handle);
                if (bl->type==BLE_CCCD){
                    //writing to a cccd
                    BLECharacteristic *c = &s->chars[bl->chr];
                    if(param->write.len!=2 || !(c->perm&48)){
                        // printf("wrong cccd op %i %x\n",param->write.len,c->perm);
                        return;
                    }
                    uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
                    if (descr_value == 0x0001){
                        //notify enable
                        //the size of notify_data[] need less than MTU size
                        c->ccc = 1;
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, HANDLE(c->hpos).handle,c->vsize, c->value, false);
                    }else if (descr_value == 0x0002){
                        //indicate enable
                        //the size of indicate_data[] need less than MTU size
                        c->ccc = 2;
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, HANDLE(c->hpos).handle,c->vsize, c->value, true);
                    }
                    else if (descr_value == 0x0000){
                        // printf("notify/indicate disable\n");
                        c->ccc = 0;
                    }else{
                        // printf("unknown descr\n");
                    }
                } else if(bl->type==BLE_VAL){
                    //writing to a value
                    BLECharacteristic *c = &s->chars[bl->chr];
                    // printf("writing value to char %x %x\n",s->uuid.uuid16,c->uuid.uuid16);
                    if(param->write.len>c->vsize || !(c->perm&8)){
                        // printf("wrong value op %i %x\n",param->write.len,c->perm);
                        return;
                    }
                    memcpy(c->value,param->write.value,param->write.len);
                    BLEEvent blee = {BLE_GATT_EVENT,8,s->uuid.uuid16,c->uuid.uuid16};
                    _ble_put_evt(&blee);
                }
                /* send response when param->write.need_rsp is true*/
                if (param->write.need_rsp){
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
            }else{
                /* handle prepare write */
                // example_prepare_write_event_env(gatts_if, &prepare_write_env, param);
            }
      	    break;
        case ESP_GATTS_EXEC_WRITE_EVT: 
            // the length of gattc prapare write data must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX. 
            // ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
            // example_exec_write_event_env(&prepare_write_env, param);
            break;
        case ESP_GATTS_MTU_EVT:
            // ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            // printf("MTU set to %i\n",param->mtu.mtu);
            break;
        case ESP_GATTS_CONF_EVT:
            // ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
        break;    
    }
}

/********************* natives */

C_NATIVE(_ble_init) {
    C_NATIVE_UNWARN();
    uint32_t err_code;
    esp_err_t ee = ESP_OK;

    //zero out status
    memset(&ble,0,sizeof(BLEDevice));

    ee = nvs_flash_init();
    if (ee) {
        ee = nvs_flash_erase();
        ee = nvs_flash_init();
    }

    ee = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    ESP_ERR_EXC(ee,"controller mem fail\n",ERR_RUNTIME_EXC);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ee = esp_bt_controller_init(&bt_cfg);
    ESP_ERR_EXC(ee,"initialize controller failed\n",ERR_RUNTIME_EXC);

    ee = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    ESP_ERR_EXC(ee,"enable controller failed\n",ERR_RUNTIME_EXC);

    ee = esp_bluedroid_init();
    ESP_ERR_EXC(ee,"init bluedroid failed\n",ERR_RUNTIME_EXC);

    ee = esp_bluedroid_enable();
    ESP_ERR_EXC(ee,"enable bluedroid failed\n",ERR_RUNTIME_EXC);
    
    ee = esp_ble_gatts_register_callback(_ble_gatts_event_handler);
    ESP_ERR_EXC(ee,"gatts register callback failed\n",ERR_RUNTIME_EXC);

    ee = esp_ble_gap_register_callback(_ble_gap_event_handler);
    ESP_ERR_EXC(ee,"gap register callback failed\n",ERR_RUNTIME_EXC);
    
    ee = esp_ble_gatts_register_callback(_ble_gatts_event_handler);
    ESP_ERR_EXC(ee,"gap register callback failed\n",ERR_RUNTIME_EXC);
   
    // ee = esp_ble_gatts_app_register(0);
    // ESP_ERR_EXC(ee,"app register failed\n",ERR_RUNTIME_EXC);

    // esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    // ESP_ERR_EXC(local_mtu_ret,"mtu failed\n",ERR_RUNTIME_EXC);

    return ERR_OK;
}




C_NATIVE(_ble_set_gap) {
    C_NATIVE_UNWARN();
    esp_err_t ee = ESP_OK;
    *res = MAKE_NONE();
    if (parse_py_args("siiiiiii", nargs, args, 
                &ble.name, 
                &ble.namelen,
                &ble.security,
                &ble.level,
                &ble.appearance,
                &ble.min_conn,
                &ble.max_conn,
                &ble.latency,
                &ble.conn_sup) != 8)
        return ERR_TYPE_EXC;

    //make a copy of name
    uint8_t *bname = gc_malloc(ble.namelen+1);
    memcpy(bname,ble.name,ble.namelen);
    ble.name=bname;
    // printf("%s\n",ble.name);
    esp_ble_gap_set_device_name(ble.name);
    _ble_adv_data.include_name = ble.namelen>0;
    _ble_adv_data.appearance = ble.appearance;
    _ble_adv_data.min_interval = (int)(ble.min_conn/1.25f);
    _ble_adv_data.max_interval = (int)(ble.max_conn/1.25f);

    ee = esp_ble_gap_set_prefer_conn_params(
            esp_bt_dev_get_address(),
            _ble_adv_data.min_interval,
            _ble_adv_data.max_interval,
            (int)(ble.latency/1.25f),
            (int)(ble.conn_sup/1.25f));
    ESP_ERR_EXC(ee,"gap conn data\n",ERR_RUNTIME_EXC);
    return ERR_OK;
}


C_NATIVE(_ble_set_services) {
    C_NATIVE_UNWARN();

    int i,j,h,c,e=0;
    PList *slist = args[0];
    ble.nchar = 0;
    ble.nserv = PSEQUENCE_ELEMENTS(slist);
    ble.nhandles=0;
    *res  = MAKE_NONE();

    if (!ble.nserv) {
        //return and keep db at null
        return ERR_OK;
    }

    ble.services = gc_malloc(sizeof(BLEService)*ble.nserv);

    //let's count handles
    for(i=0;i<ble.nserv;i++){
        PTuple *psrv = (PTuple*)PLIST_ITEM(slist,i); //tuple of uuid, characteristics
        PList *chs = PTUPLE_ITEM(psrv,1); //get characteristics
        int nchs = PSEQUENCE_ELEMENTS(chs);
        ble.nhandles++;
        c=0;
        ble.nhandles+=nchs*2;
        c+=nchs*2;
        ble.nchar++;
        for(j=0;j<nchs;j++){
            PTuple *ch = PLIST_ITEM(chs,j); //take jth char
            int permission = (uint16_t)PSMALLINT_VALUE(PTUPLE_ITEM(ch,2)); //permission
            // printf("permission %i\n",permission);
            if (permission&16 || permission&32) {
                //it's a notify, add a cccd
                ble.nchar++;
                ble.nhandles++;
                c++;
            }
        }
        SERVICE(i).nchar = nchs;
        SERVICE(i).nhandles = c+1;
        SERVICE(i).chars = gc_malloc(sizeof(BLECharacteristic)*nchs);
        SERVICE(i).srv = gc_malloc(sizeof(esp_gatts_attr_db_t)*(c+1));
        // printf("service %i has char %i and handles %i\n",i,nchs,SERVICE(i).nhandles);
    }

    ble.handles = gc_malloc(sizeof(BLEHandle)*ble.nhandles);
    // printf("Nservices %i, nhandles %i, nchars %i\n",ble.nserv,ble.nhandles,ble.nchar);

    //fill table
    BLEHandle *bl=ble.handles;
    for(i=0;i<ble.nserv;i++){
        h=0;
        BLEService *s = &SERVICE(i);
        PTuple *psrv = (PTuple*)PLIST_ITEM(slist,i); //tuple of uuid, characteristics
        PList *chs = PTUPLE_ITEM(psrv,1); //get characteristics
        int nchs = PSEQUENCE_ELEMENTS(chs);
        PObject *base_uuid = PTUPLE_ITEM(psrv,2);

        //fill entry for service
        s->srv[h].attr_control.auto_rsp = ESP_GATT_AUTO_RSP;
        s->srv[h].att_desc.uuid_length = ESP_UUID_LEN_16; 
        s->srv[h].att_desc.uuid_p = &_ble_srv_primary;
        s->srv[h].att_desc.perm = ESP_GATT_PERM_READ;
        bl->srv = i;
        bl->type = BLE_SERVICE;
        bl->pos = h;

        if(base_uuid!=*res){
            //extended uuid!
            int ulen = PSEQUENCE_ELEMENTS(base_uuid);
            if (ulen>16) {
                e=1;
                break;
            }
            memcpy(s->uuid.full_uuid,PSEQUENCE_BYTES(base_uuid),ulen);
            s->srv[h].att_desc.max_length = ulen;
            s->srv[h].att_desc.length = ulen;
            s->usize = ulen;
            s->srv[h].att_desc.value = (uint8_t*)s->uuid.full_uuid;
        } else {
            int uuid = PSMALLINT_VALUE( PTUPLE_ITEM(psrv,0));
            s->srv[h].att_desc.max_length = 2;
            s->srv[h].att_desc.length = 2;
            memcpy(&s->uuid.uuid16,&uuid,2);
            s->usize = 2;
            s->srv[h].att_desc.value = (uint8_t*)(&s->uuid.uuid16);
        }

        //start filling chars
        h++;
        bl++;
        for(j=0;j<nchs;j++){
            esp_gatts_attr_db_t *sch = &s->srv[h];
            BLECharacteristic *ca = &s->chars[j];

            PTuple *ch = PLIST_ITEM(chs,j); //take jth char
            //TODO: add long uuid support in ble.py 
            int uuid = (uint16_t)PSMALLINT_VALUE(PTUPLE_ITEM(ch,0)); //uuid
            int permission = (uint16_t)PSMALLINT_VALUE(PTUPLE_ITEM(ch,2)); //permission
            int ilen = (uint16_t)PSMALLINT_VALUE(PTUPLE_ITEM(ch,3)); //value size

            //fill db with 3 entries for each char
            //1st is declaration
            // printf("adding ch decl for ch %i srv %i at %i\n",j,i,h);
            ca->perm = permission;
            sch->attr_control.auto_rsp = ESP_GATT_AUTO_RSP;
            sch->att_desc.uuid_length = ESP_UUID_LEN_16; //set default
            sch->att_desc.uuid_p = &_ble_char_decl;
            sch->att_desc.perm = ESP_GATT_PERM_READ;
            sch->att_desc.max_length = 1;
            sch->att_desc.length = 1;
            sch->att_desc.value = &ca->perm;
            bl->srv=i;
            bl->chr=j;
            bl->pos=h;
            bl->type=BLE_DECL;

            //2nd is value
            h++;
            bl++;
            // printf("adding ch value for ch %i srv %i at %i\n",j,i,h);
            sch = &s->srv[h];
            ca->vsize = ilen;
            ca->hpos = bl-ble.handles;  //pos of c in handles
            ca->usize = 2;
            ca->uuid.uuid16 = uuid;
            sch->att_desc.uuid_p = &ca->uuid.uuid16;
            sch->attr_control.auto_rsp = ESP_GATT_AUTO_RSP;
            sch->att_desc.uuid_length = ESP_UUID_LEN_16; //set default
            sch->att_desc.perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
            sch->att_desc.max_length = ilen;
            sch->att_desc.length = ilen;
            sch->att_desc.value = (uint8_t*)ca->value;
            bl->srv=i;
            bl->chr=j;
            bl->pos=h;
            bl->type=BLE_VAL;

            h++;
            bl++;
            if (permission&(BLE_PERM_NOTIFY | BLE_PERM_INDICATE)) {
                //it's a notify
                //3rd is cccd
                // printf("adding ch cccd for ch %i srv %i at %i\n",j,i,h);
                sch = &s->srv[h];
                ca->ccc = 0;
                sch->attr_control.auto_rsp = ESP_GATT_AUTO_RSP;
                sch->att_desc.uuid_length = ESP_UUID_LEN_16; //set default
                sch->att_desc.uuid_p = &_ble_cccd;
                sch->att_desc.perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
                sch->att_desc.max_length = 2;
                sch->att_desc.length = 2;
                sch->att_desc.value = (uint8_t*)&ca->ccc;
                bl->srv=i;
                bl->chr=j;
                bl->pos=h;
                bl->type=BLE_CCCD;
                h++;
                bl++;
            } else {
                ca->ccc = 0xff;
            }
        }
        //add service

    }

    //TODO: check errors
    esp_err_t ee = esp_ble_gatts_app_register(0);
    ESP_ERR_EXC(ee, "failed app register\n", ERR_RUNTIME_EXC);


    while(ble.srv_created<ble.nserv){
        // printf("wait creation\n");
        vosThSleep(TIME_U(10,MILLIS));
    }

    return ERR_OK;
}


C_NATIVE(_ble_get_event) {
    C_NATIVE_UNWARN();
    BLEEvent blee;
    int pos;
    RELEASE_GIL();
    vosSemWait(bleevt);
    _ble_get_evt(&blee);
    PTuple *tpl = ptuple_new(5,NULL);
    PTUPLE_SET_ITEM(tpl,0,PSMALLINT_NEW(blee.type));
    PTUPLE_SET_ITEM(tpl,1,PSMALLINT_NEW(blee.type));
    PTUPLE_SET_ITEM(tpl,2,PSMALLINT_NEW(blee.type));
    PTUPLE_SET_ITEM(tpl,3,PSMALLINT_NEW(blee.status));
    PTUPLE_SET_ITEM(tpl,4,MAKE_NONE());

    if (blee.type==BLE_GATT_EVENT){
        //GATT EVENT
        PTUPLE_SET_ITEM(tpl,1,PSMALLINT_NEW(blee.service));
        PTUPLE_SET_ITEM(tpl,2,PSMALLINT_NEW(blee.characteristic));
    } else {
        //GAP EVENT
        if(blee.status==BLE_GAP_EVT_SCAN_REPORT) {
            //create Python report
            BLEScanResult *scr = blee.object;
            PTuple *tpp = ptuple_new(5,NULL);
            PTUPLE_SET_ITEM(tpp,0,PSMALLINT_NEW(scr->scantype));
            PTUPLE_SET_ITEM(tpp,1,PSMALLINT_NEW(scr->addrtype));
            PTUPLE_SET_ITEM(tpp,2,PSMALLINT_NEW(scr->rssi));
            PBytes *pdata = pbytes_new(scr->datalen,scr->data);
            PBytes *paddr = pbytes_new(6,scr->addr);
            PTUPLE_SET_ITEM(tpp,3,pdata);
            PTUPLE_SET_ITEM(tpp,4,paddr);
            //We can now free scr data
            gc_free(scr);
            PTUPLE_SET_ITEM(tpl,4,tpp);
        } else if(blee.status==BLE_GAP_EVT_CONNECTED || blee.status==BLE_GAP_EVT_DISCONNECTED) {
            //add remote address
            PBytes *paddr = pbytes_new(6,ble.remote_bda);
            PTUPLE_SET_ITEM(tpl,4,paddr);
        } else if(blee.status==BLE_GAP_EVT_SHOW_PASSKEY){
            PTUPLE_SET_ITEM(tpl,4,PSMALLINT_NEW(ble.passkey));
        } else if(blee.status==BLE_GAP_EVT_MATCH_PASSKEY){
            PTUPLE_SET_ITEM(tpl,4,blee.object);
        }
    }
    *res = tpl;
    ACQUIRE_GIL();
    return ERR_OK;
}

C_NATIVE(_ble_get_value) {
    C_NATIVE_UNWARN();
    uint32_t serv_uuid;
    uint32_t uuid;
    uint8_t* value;
    uint32_t valuelen;
    int i,j;
    *res = MAKE_NONE();
    if (parse_py_args("ii", nargs, args, &serv_uuid, &uuid) != 2)
        return ERR_TYPE_EXC;

    for(i=0;i<ble.nserv;i++){
        BLEService *s = &SERVICE(i);
        //TODO: fix this for non 16-bit uuid
        // printf("checking service %i with uuid %x vs %x\n",i,s->uuid.uuid16,serv_uuid);
        if(s->uuid.uuid16==serv_uuid) {
            //found
            for(j=0;j<s->nchar;j++){
                BLECharacteristic *c = &s->chars[j];
                // printf("checking char %i/%i/%x with uuid %x vs %x and len %i vs %i\n",j,c->hpos,c->ccc,c->uuid.uuid16,uuid,c->vsize,valuelen);
                if (c->uuid.uuid16==uuid) {
                    PBytes *pb =pbytes_new(c->vsize,NULL);
                    memcpy(PSEQUENCE_BYTES(pb),c->value,c->vsize);
                    *res = pb;
                    return ERR_OK;
                }
            }
        }
    }
    return ERR_OK;
}

C_NATIVE(_ble_set_value) {
    C_NATIVE_UNWARN();
    uint32_t serv_uuid;
    uint32_t uuid;
    uint8_t* value;
    uint32_t valuelen;
    int i,j;
    *res = MAKE_NONE();
    if (parse_py_args("iis", nargs, args, &serv_uuid, &uuid,&value, &valuelen) != 3)
        return ERR_TYPE_EXC;

    for(i=0;i<ble.nserv;i++){
        BLEService *s = &SERVICE(i);
        //TODO: fix this for non 16-bit uuid
        // printf("checking service %i with uuid %x vs %x\n",i,s->uuid.uuid16,serv_uuid);
        if(s->uuid.uuid16==serv_uuid) {
            //found
            for(j=0;j<s->nchar;j++){
                BLECharacteristic *c = &s->chars[j];
                // printf("checking char %i/%i/%x with uuid %x vs %x and len %i vs %i\n",j,c->hpos,c->ccc,c->uuid.uuid16,uuid,c->vsize,valuelen);
                if (c->uuid.uuid16==uuid) {
                    //found
                    if(valuelen==c->vsize){
                        //size is ok
                        if (memcmp(value,c->value,c->vsize)!=0) {
                            memcpy(c->value,value,c->vsize);
                            if (ble.connected) {
                                // printf("ch %i ccc %x\n",j,c->ccc);
                                if (c->ccc==2){
                                    esp_ble_gatts_send_indicate(ble.gatts_if, ble.connid, HANDLE(c->hpos).handle, c->vsize, c->value, true);
                                } else if (c->ccc==1){
                                    esp_ble_gatts_send_indicate(ble.gatts_if, ble.connid, HANDLE(c->hpos).handle, c->vsize, c->value, false);
                                }
                            } else {
                                // printf("not connected, value not notified\n");
                            }
                        }
                        return ERR_OK;
                    }
                }
            }
    
        }
    }

    return ERR_INDEX_EXC;
}



C_NATIVE(_ble_set_advertising) {
    C_NATIVE_UNWARN();
    *res = MAKE_NONE();
    uint8_t *payload;
    uint32_t payloadlen;
    uint8_t *scanrsp;
    uint32_t advertising_interval;
    uint32_t scanrsplen;
    // printf("TYPE %i\n",PTYPE(args[2]));
    if (parse_py_args("iisis", nargs, args, &advertising_interval,&ble.advertising_timeout,&payload,&payloadlen,&ble.adv_mode,&scanrsp,&scanrsplen) != 5)
        return ERR_TYPE_EXC;
    ble.advert_data_len=payloadlen;
    if (payloadlen>31) ble.advert_data_len=31;
    memcpy(ble.advert_data,payload,ble.advert_data_len);
    ble.scanrsp_data_len=scanrsplen;
    if (scanrsplen>31) ble.scanrsp_data_len=31;
    memcpy(ble.scanrsp_data,scanrsp,ble.scanrsp_data_len);
    _ble_adv_params.adv_int_min = (int)(advertising_interval/0.625f);
    _ble_adv_params.adv_int_max = (int)((advertising_interval+10)/0.625f);
    if (ble.adv_mode==0) _ble_adv_params.adv_type = ADV_TYPE_IND;
    else if (ble.adv_mode==3) _ble_adv_params.adv_type = ADV_TYPE_NONCONN_IND;
    else if (ble.adv_mode==2) _ble_adv_params.adv_type = ADV_TYPE_SCAN_IND;
    else return ERR_UNSUPPORTED_EXC;

    return ERR_OK;
}


C_NATIVE(_ble_start_advertising) {
    C_NATIVE_UNWARN();
    *res=MAKE_NONE();
    esp_err_t ee=ESP_OK;
    int exp_adv=0;
    if (ble.advert_data_len) {
        ee = esp_ble_gap_config_adv_data_raw(ble.advert_data,ble.advert_data_len);
        ESP_ERR_EXC(ee,"gap adv raw data\n",ERR_RUNTIME_EXC);
        exp_adv++;
    } else {
        ee = esp_ble_gap_config_adv_data(&_ble_adv_data); 
        ESP_ERR_EXC(ee,"gap adv data\n",ERR_RUNTIME_EXC);
    }
    if (ble.scanrsp_data_len) {
        ee = esp_ble_gap_config_scan_rsp_data_raw(ble.scanrsp_data,ble.scanrsp_data_len);
        ESP_ERR_EXC(ee,"gap scan rsp raw data\n",ERR_RUNTIME_EXC);
        exp_adv++;
    } 
    // else {
    //     //TODO: add a custom scan rsp data for non raw?
    //     ee = esp_ble_gap_config_adv_data(&_ble_adv_data); 
    //     ESP_ERR_EXC(ee,"gap scan rsp data\n",ERR_RUNTIME_EXC);
    // }
    while(ble.adv_data_set<exp_adv){
        vosThSleep(TIME_U(100,MILLIS));
        // printf("wait\n");
    }
    ble.adv_data_set=0;
    ee = esp_ble_gap_start_advertising(&_ble_adv_params);
    ESP_ERR_EXC(ee, "failed start\n", ERR_RUNTIME_EXC);
    return ERR_OK;
}

C_NATIVE(_ble_stop_advertising) {
    C_NATIVE_UNWARN();
    *res=MAKE_NONE();
    esp_err_t ee=ESP_OK;
    ee = esp_ble_gap_stop_advertising();
    ESP_ERR_EXC(ee, "failed stop\n", ERR_RUNTIME_EXC);
    return ERR_OK;
}

C_NATIVE(_ble_set_scanning) {
    C_NATIVE_UNWARN();
    int interval,window,duplicates,filter,addr,active;
    if (parse_py_args("iiiiii", nargs, args, &interval,&window,&duplicates,&filter,&addr,&active) != 6)
        return ERR_TYPE_EXC;
    _ble_scan_params.scan_interval = (int)(interval/0.625);
    _ble_scan_params.scan_window = (int)(window/0.625);
    _ble_scan_params.scan_duplicate = (!duplicates) ? BLE_SCAN_DUPLICATE_ENABLE:BLE_SCAN_DUPLICATE_DISABLE;
    _ble_scan_params.scan_type = (!active) ?  BLE_SCAN_TYPE_PASSIVE:BLE_SCAN_TYPE_ACTIVE;
    //TODO: add other scan parameters
    return ERR_OK;
}

C_NATIVE(_ble_start_scanning) {
    C_NATIVE_UNWARN();
    int duration;
    esp_err_t ee=ESP_OK;
    if (parse_py_args("i", nargs, args, &duration) != 1)
        return ERR_TYPE_EXC;

    ble.scan_data_set = 0;
    ee = esp_ble_gap_set_scan_params(&_ble_scan_params);
    ESP_ERR_EXC(ee,"failed scan params\n",ERR_RUNTIME_EXC);
    while(!ble.scan_data_set) vosThSleep(TIME_U(10,MILLIS));
    ble.scan_started=0;
    esp_ble_gap_start_scanning(duration/1000);
    while(!ble.scan_started) vosThSleep(TIME_U(10,MILLIS));
    if(ble.scan_started<0) {
        //error!
        return ERR_RUNTIME_EXC;
    }

    return ERR_OK;
}

C_NATIVE(_ble_set_security){
    C_NATIVE_UNWARN();
    if (parse_py_args("iiiiiiii", nargs, args, &ble.capabilities,&ble.bonding,&ble.scheme,&ble.key_size,&ble.init_key,&ble.resp_key,&ble.oob,&ble.passkey) != 8)
        return ERR_TYPE_EXC;


    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = 0;
    if(ble.bonding) auth_req|=ESP_LE_AUTH_BOND;
    if(ble.scheme & AUTH_MITM) auth_req|=ESP_LE_AUTH_REQ_MITM;
    if(ble.scheme & AUTH_SC) auth_req|=ESP_LE_AUTH_REQ_SC_ONLY;

    esp_ble_io_cap_t iocap = ble.capabilities;

    ble.key_size = (ble.key_size>16) ? 16:((ble.key_size<7) ? 7:ble.key_size);      //the key size should be 7~16 bytes
    int init_key = 0, resp_key = 0;
    if (ble.init_key & KEY_ENC) init_key |= ESP_BLE_ENC_KEY_MASK;
    if (ble.init_key & KEY_ID) init_key |= ESP_BLE_ID_KEY_MASK;
    if (ble.init_key & KEY_CSR) init_key |= ESP_BLE_CSR_KEY_MASK;
    if (ble.init_key & KEY_LNK) init_key |= ESP_BLE_LINK_KEY_MASK;
    if (ble.resp_key & KEY_ENC) resp_key |= ESP_BLE_ENC_KEY_MASK;
    if (ble.resp_key & KEY_ID) resp_key |= ESP_BLE_ID_KEY_MASK;
    if (ble.resp_key & KEY_CSR) resp_key |= ESP_BLE_CSR_KEY_MASK;
    if (ble.resp_key & KEY_LNK) resp_key |= ESP_BLE_LINK_KEY_MASK;


    //set static passkey
    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_ENABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &ble.passkey, sizeof(uint32_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &ble.key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribut to you,
    and the response key means which key you can distribut to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribut to you,
    and the init key means which key you can distribut to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &resp_key, sizeof(uint8_t));
    return ERR_OK;
}

C_NATIVE(_ble_stop_scanning) {
    C_NATIVE_UNWARN();
    esp_ble_gap_stop_scanning();
    return ERR_OK;
}

C_NATIVE(_ble_del_bonded) {
    C_NATIVE_UNWARN();
    uint8_t *addr;
    uint32_t addrlen;
    if (parse_py_args("s", nargs, args, &addr,&addrlen) != 1)
        return ERR_TYPE_EXC;

    if(addrlen!=6) return ERR_TYPE_EXC;

    esp_ble_remove_bond_device(addr);
    return ERR_OK;
}

C_NATIVE(_ble_get_bonded) {
    C_NATIVE_UNWARN();
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)gc_malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    // ESP_LOGI(GATTS_TABLE_TAG, "Bonded devices number : %d\n", dev_num);

    // ESP_LOGI(GATTS_TABLE_TAG, "Bonded devices list : %d\n", dev_num);
    PTuple *tpl = ptuple_new(dev_num,NULL);
    for (int i = 0; i < dev_num; i++) {
        PBytes *pb = pbytes_new(6,dev_list[i].bd_addr);
        PTuple *tpp = ptuple_new(2,NULL);
        PTUPLE_SET_ITEM(tpp,0,pb);
        PTUPLE_SET_ITEM(tpp,1,PSMALLINT_NEW(dev_list[i].bond_key.key_mask));
        PTUPLE_SET_ITEM(tpl,i,tpp);
        // printf("bonded %i\n",i);
        // esp_log_buffer_hex(GATTS_TABLE_TAG, (void *)dev_list[i].bd_addr, sizeof(esp_bd_addr_t));
    }

    gc_free(dev_list);
    *res=tpl;
    return ERR_OK;
}

C_NATIVE(_ble_confirm_passkey) {
    C_NATIVE_UNWARN();
    *res = MAKE_NONE();
    uint32_t confirm;
    if (parse_py_args("i", nargs, args, &confirm) != 1)
        return ERR_TYPE_EXC;
    esp_ble_confirm_reply(ble.pairing_bda,confirm);
    return ERR_OK;
}

C_NATIVE(_ble_start) {
    C_NATIVE_UNWARN();
    *res = MAKE_NONE();
    bleevt = vosSemCreate(0);
    _ble_adv_tm = vosTimerCreate();
   return ERR_OK;
}


