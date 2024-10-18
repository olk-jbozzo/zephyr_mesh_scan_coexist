/*
 * Copyright (c) 2019 Tobias Svehagen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mesh_scan_coexist, LOG_LEVEL_DBG);

#include "provisioning.h"
#include "node.h"
#include "hw_config.h"

#include "peer.h"

// Change between 0 and 1 to change mesh and peer start order
// 0 - first mesh, then peer
// 1 - first peer, then mesh
#define ALTERNATIVE_SEQUENCE 0

// If your board is not a nordic DK, you can select whenever your board is a provisioner or not here
#define CONFIG_FOR_NON_DK__IS_PROVISIONER 0

static bool is_provisioner = false;

static int mesh_start(void) {
	int err = 0;
	if(is_provisioner) {
		LOG_INF("Starting as provisioner");
		err = provisining_init();
		if (err) {
			LOG_ERR("Provisioning init failed (err %d)", err);
			return err;
		}

		err = bt_mesh_init(&provisioner_prov, &provisioner_mesh_comp);
		if (err) {
			LOG_ERR("Initializing mesh failed (err %d)", err);
			return err;
		}
		
		err = provisioning_start();	
		if (err) {
			LOG_ERR("Provisioning start failed (err %d)", err);
			return err;
		}

	} else {
		LOG_INF("Starting as regular node");
		err = bt_mesh_init(&node_prov, &node_mesh_comp);
		if (err) {
			LOG_ERR("Initializing mesh failed (err %d)", err);
			return err;
		}

		err = node_start();
		if (err) {
			LOG_ERR("Node init failed (err %d)", err);
			return err;
		}
	}
	return err;
}


static void bt_ready(int err)
{
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return;
	}

	LOG_INF("Bluetooth initialized");

	#if IS_ENABLED(ALTERNATIVE_SEQUENCE) // First peer, then mesh
		err = peer_start();
		if (err) {
			LOG_ERR("Peer start failed (err %d)", err);
			return;
		}
	
		err = mesh_start();
		if (err) {
			LOG_ERR("Mesh start failed (err %d)", err);
			return;
		}
	#else // First mesh, then peer
		err = mesh_start();
		if (err) {
			LOG_ERR("Mesh start failed (err %d)", err);
			return;
		}
	
		err = peer_start();
		if (err) {
			LOG_ERR("Peer start failed (err %d)", err);
			return;
		}

	#endif
}


int main(void)
{
	int err = 0;
	LOG_INF("Initializing...");

	err = hw_init(CONFIG_FOR_NON_DK__IS_PROVISIONER);
	if (err) {
		LOG_ERR("Hardware init failed (err %d)", err);
		return err;
	}
	LOG_INF(" - Hardware initialized");

	is_provisioner = hw_provisioner_button_pressed();

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}
	LOG_INF(" - Bluetooth initialized");

	while(true) {
		k_sleep(K_MSEC(500));
	}
	return 0;
}