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

static int setup(void);
static int mesh_start(void);

// Change between 0 and 1 to change mesh and peer start order
// 0 - first peer, then mesh
// 1 - first mesh, then peer
#define ALTERNATIVE_SEQUENCE 0

// If your board is not a nordic DK, you can select whenever your board is a provisioner or not here
#define CONFIG_FOR_NON_DK__IS_PROVISIONER 0

int main(void)
{
	int err = 0;
	err = setup();
	if (err) {
		LOG_ERR("Setup failed (err %d)", err);
		return err;
	}

	// First mesh, then peer
	#if IS_ENABLED(ALTERNATIVE_SEQUENCE)
		err = mesh_start();
		if (err) {
			LOG_ERR("Mesh start failed (err %d)", err);
			return err;
		}
	
		err = peer_start();
		if (err) {
			LOG_ERR("Peer start failed (err %d)", err);
			return err;
		}
	// First peer, then mesh
	#else
		err = peer_start();
		if (err) {
			LOG_ERR("Peer start failed (err %d)", err);
			return err;
		}
	
		err = mesh_start();
		if (err) {
			LOG_ERR("Mesh start failed (err %d)", err);
			return err;
		}

	#endif

	while(true) {
		k_sleep(K_MSEC(500));
	}
	return 0;
}

int setup(void) {
	LOG_INF("Initializing...");

	int err = hw_init(CONFIG_FOR_NON_DK__IS_PROVISIONER);
	if (err) {
		LOG_ERR("Hardware init failed (err %d)", err);
		return err;
	}
	LOG_INF(" - Hardware initialized");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}
	LOG_INF(" - Bluetooth initialized");

	return err;
}

int mesh_start() {
	// Inside provisioning_start or node_on_mesh_ready_init, bt_mesh_init will be executed
	int err = 0;
	if(hw_provisioner_button_pressed()) {
		LOG_INF("Starting as provisioner");
		err = provisining_init();
		if (err) {
			LOG_ERR("Provisioning init failed (err %d)", err);
			return err;
		}
		err = provisioning_start();	
		if (err) {
			LOG_ERR("Provisioning start failed (err %d)", err);
			return err;
		}
	} else {
		LOG_INF("Starting as regular node");
		err = node_on_mesh_ready_init();
		if (err) {
			LOG_ERR("Node init failed (err %d)", err);
			return err;
		}
	}
	return err;
}