Bluetooth: Mesh and scan coexistence
####################################

Overview
********

This sample tries to make custom advertising coexist with bluetooth mesh. At this point, scan filtering and mesh provisioning are at odds with one another. File structure:

- fake_kconfig.h: Constants storage.
- hw_config.h: Gets the device UUID and gets the state of a button to start as provisioner or not. The button can be disabled and compiled into a constant (button permanently pressed or released).
- main.c: Initializes the mesh and scan features. The order of initialization can be changed. To demonstrate the issue.
- node.c: Contains the mesh relay node code.
- peer.c: Contains the advertisement and filtered scan logic.
- provisioner.c: Contains the mesh provisioning logic, it is basically the mesh_provisioner example from Zephyr.

This program is based on the following samples:

- Provisioning: Zephyr's Bluetooth mesh provisioning sample (https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/bluetooth/mesh_provisioner)
- Mesh and periperal initialization: Nordic's Bluetooth Mesh and Peripheral Coexistence (https://github.com/nrfconnect/sdk-nrf/tree/main/samples/bluetooth/mesh/ble_peripheral_lbs_coex)
- Scanning / advertising: Nordic's Distance measurement sample (https://github.com/nrfconnect/sdk-nrf/tree/main/samples/bluetooth/nrf_dm)

This sample is flashed into two devices with the same ALTERNATIVE_SEQUENCE define value (See Building and Running section). One device is expected to be inialized as provisioner and provision the other node while at the same time the both nodes advertise and scan eachother.

When the mesh feature is initialized first (ALTERNATIVE_SEQUENCE == 1)
----------------------------------------------------------------------

The scan filtering is broken. The device can scan for other devices in the generic scan callback, but it the scan filter callback never gets called.

Provisioner output:

.. code-block::

   *** Booting nRF Connect SDK 4040aa0bf581 ***
   [00:00:00.000,579] <inf> mesh_scan_coexist: Initializing...
   [00:00:00.000,732] <dbg> hw_config: button_handler: Provisioner button state is pressed
   ...
   [00:00:00.256,286] <inf> provisioning: Mesh initialized
   ...
   [00:00:00.256,683] <inf> bt_mesh_main: Primary Element: 0x0001
   [00:00:00.267,333] <dbg> provisioning: provisining_init: Provisioning completed
   [00:00:00.267,608] <dbg> provisioning: configure_self: Configuring self...
   ...
   [00:00:00.330,078] <dbg> provisioning: configure_self: Configuration complete
   [00:00:00.330,078] <dbg> provisioning: provisioning_step: Waiting for unprovisioned beacon...
   [00:00:00.722,961] <dbg> hw_config: button_handler: Provisioner button state is released
   [00:00:03.631,683] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   ...
   [00:00:15.319,824] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:15.330,200] <dbg> provisioning: provisioning_step: Waiting for unprovisioned beacon...
   [00:00:15.422,882] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   ...
   [00:00:33.528,717] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:33.558,471] <dbg> provisioning: provisioning_step: Provisioning 4326c29958c8af8a0000000000000000
   [00:00:33.558,776] <dbg> provisioning: provisioning_step: Waiting for node to be added...
   [00:00:33.946,411] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   ...
   [00:00:37.100,189] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:37.397,277] <dbg> provisioning: provisioning_step: Added node 0x0002   
   [00:00:37.654,113] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   ...
   [00:00:40.483,093] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:41.434,478] <dbg> provisioning: configure_node: Configuring node 0x0002...
   [00:00:41.752,777] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:41.767,700] <dbg> provisioning: configure_node: Element @ 0x0002: 1 + 0 models
   [00:00:41.767,730] <dbg> provisioning: configure_node: Configuration complete


Notice that:
   - The provisioner is capable of provisioning and configuring itself and the other node (``Primary Element: 0x0001`` -> ``configure_self: Configuration complete`` -> ``Added node 0x0002`` -> ``configure_node: Configuration complete``).
   - We get the message ``peer: adv_scanned_cb: SCANNING is working (I've been scanned)``. I am not sure if this comes from the scanning procedure of this program or another BLE device in my vecinity.  

Node output:

.. code-block::

   *** Booting nRF Connect SDK 4040aa0bf581 ***
   [00:00:00.000,579] <inf> mesh_scan_coexist: Initializing...
   ...
   [00:00:00.004,364] <inf> mesh_scan_coexist: Starting as regular node
   ...
   [00:00:00.260,162] <inf> node: Mesh initialized
   [00:00:00.266,510] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   ...
   [00:00:06.348,175] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:06.375,671] <inf> bt_mesh_main: Primary Element: 0x0002
   [00:00:06.383,117] <inf> node: ================
   [00:00:06.383,117] <inf> node: NODE PROVISIONED
   [00:00:06.383,148] <inf> node: ================


When the scan feature is initialized first (ALTERNATIVE_SEQUENCE == 0)
----------------------------------------------------------------------

The provisioning is broken. The device is capable of self provisioning, but it cannot provision other nodes.

Provisioner output:

.. code-block::

   *** Booting nRF Connect SDK 4040aa0bf581 ***
   [00:00:00.000,579] <inf> mesh_scan_coexist: Initializing...
   ...
   [00:00:00.005,950] <inf> mesh_scan_coexist: Starting as provisioner
   [00:00:00.262,054] <inf> provisioning: Mesh initialized
   ...
   [00:00:00.262,451] <inf> bt_mesh_main: Primary Element: 0x0001
   [00:00:00.273,162] <dbg> provisioning: provisining_init: Provisioning completed
   [00:00:00.273,223] <dbg> provisioning: configure_self: Configuring self...
   ...
   [00:00:00.321,136] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:00.335,479] <dbg> provisioning: configure_self: Configuration complete
   [00:00:00.335,479] <dbg> provisioning: provisioning_step: Waiting for unprovisioned beacon...
   [00:00:00.626,373] <wrn> peer: SCANNED AND FILTERED peer of addr C3:2D:14:D8:61:AD (random) and hw_id 9993426381020735043
   [00:00:00.759,948] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:00.859,771] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:00.960,266] <dbg> hw_config: button_handler: Provisioner button state is released
   [00:00:01.054,840] <wrn> peer: SCANNED AND FILTERED peer of addr C3:2D:14:D8:61:AD (random) and hw_id 9993426381020735043
   [00:00:01.158,599] <wrn> peer: SCANNED AND FILTERED peer of addr C3:2D:14:D8:61:AD (random) and hw_id 9993426381020735043
   ...
   // Provisioner never provisions anything else

Notice that:
   - The provisioner is only capable of provisioning itself.
   - We get ``SCANNED AND FILTERED peer of addr...`` so the scan filter is working.

Node output:

.. code-block::

   *** Booting nRF Connect SDK 4040aa0bf581 ***
   [00:00:00.000,640] <inf> mesh_scan_coexist: Initializing...
   ...
   [00:00:00.006,103] <inf> mesh_scan_coexist: Starting as regular node
   ...
   [00:00:00.265,625] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:00.268,585] <inf> node: Mesh initialized
   [00:00:00.545,745] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:00.865,142] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:01.279,998] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:01.894,622] <wrn> peer: SCANNED AND FILTERED peer of addr E4:95:4B:A3:22:1F (random) and hw_id 15307391399923646998
   [00:00:01.907,989] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:02.008,758] <dbg> peer: adv_scanned_cb: SCANNING is working (I've been scanned)
   [00:00:02.300,476] <wrn> peer: SCANNED AND FILTERED peer of addr E4:95:4B:A3:22:1F (random) and hw_id 15307391399923646998
   ...
   // Node is never provisioned


Requirements
************

* Two board with Bluetooth LE support, preferably Nordic development kits

Building and Running
********************

Create a build configuration for your board. If you are not using a DK, change CONFIG_DK_LIBRARY to n

* To change the order of initialization of the scan and mesh features, change ALTERNATIVE_SEQUENCE in main.c between 0 and 1.
* To start as mesh provisioner, hold the DK button 1 while booting for a few seconds. If you compiled with CONFIG_DK_LIBRARY=n, then change the value of CONFIG_FOR_NON_DK__IS_PROVISIONER to 1 and recompile.
