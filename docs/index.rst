.. _esp32ble:

*******************
Espressif ESP32 BLE
*******************

This module implements the BLE driver for ESP32. It includes support for secure pairing and scanning.

The module implements only the peripheral role functionalities, while the central role ones are not included. 
The module is also compatible with the Wifi and Ethernet drivers and the radio coexistence is entirely managed by the VM in a balanced mode.

Many examples are available going from a simple scanner, to beacons to a more advanced GATT server.
To test the examples it is suggested to use a BLE app like the `nRF Connect <https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp&hl=en>`_ and a Beacon scanner like `Beacon Scanner <https://play.google.com/store/apps/details?id=com.bridou_n.beaconscanner&hl=en_US>`_ 

Some screenshots related to the examples:

.. figure:: /custom/img/esp32blebeacon.jpg
   :align: center
   :figwidth: 70%
   :alt: Esp32 Eddystone beacon

   Eddystone beacons

.. figure:: /custom/img/esp32blealert1.jpg
   :align: center
   :figwidth: 70%
   :alt: Esp32 Notifications

   Notifications GATT server

.. figure:: /custom/img/esp32blealert2.jpg
   :align: center
   :figwidth: 70%
   :alt: Esp32 Notifications Control

   Controlling GATT server with nRF Connect


.. include:: __toc.rst

