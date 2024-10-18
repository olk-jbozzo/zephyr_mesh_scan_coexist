/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/byteorder.h>
#include <bluetooth/scan.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(peer, LOG_LEVEL_DBG);

#include "hw_config.h"
#include "peer.h"

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

#define SUPPORT_PEER_CODE         0xAABBDDFF

struct adv_mfg_data {
	uint16_t company_code;
	uint32_t support_peer_code;
	uint64_t hw_id;
} __packed;

static struct adv_mfg_data mfg_data = { 0 };
struct bt_le_adv_param adv_param_conn =
	BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONNECTABLE |
			     BT_LE_ADV_OPT_NOTIFY_SCAN_REQ,
			     BT_GAP_ADV_FAST_INT_MIN_2,
			     BT_GAP_ADV_FAST_INT_MAX_2,
			     NULL);

struct bt_le_adv_param adv_param_noconn =
	BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_USE_IDENTITY |
			     BT_LE_ADV_OPT_SCANNABLE |
			     BT_LE_ADV_OPT_NOTIFY_SCAN_REQ,
			     BT_GAP_ADV_FAST_INT_MIN_2,
			     BT_GAP_ADV_FAST_INT_MAX_2,
			     NULL);


struct bt_le_adv_param *adv_param = &adv_param_conn;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, (unsigned char *)&mfg_data, sizeof(mfg_data)),
};

static struct bt_le_scan_param scan_param = {
	.type     = BT_LE_SCAN_TYPE_ACTIVE,
	.interval = BT_GAP_SCAN_FAST_INTERVAL,
	.window   = BT_GAP_SCAN_FAST_WINDOW,
	.options  = BT_LE_SCAN_OPT_NONE,
	.timeout  = 0,
};

static struct bt_scan_init_param scan_init = {
	.connect_if_match = 0,
	.scan_param = &scan_param,
	.conn_param = NULL
};

static struct bt_scan_manufacturer_data scan_mfg_data = {
	.data = (unsigned char *)&mfg_data,
	.data_len = (
		  sizeof(mfg_data.company_code)
		+ sizeof(mfg_data.support_peer_code)
	),
};

static struct bt_le_ext_adv *adv;
static void adv_work_handle(struct k_work *item);
static K_WORK_DEFINE(adv_work, adv_work_handle);

static bool data_cb(struct bt_data *data, void *user_data)
{
	struct adv_mfg_data *recv_mfg_data;
	
	switch (data->type) {
	/* Simply print the filtered device */
	case BT_DATA_MANUFACTURER_DATA:
		if (sizeof(struct adv_mfg_data) == data->data_len) {
			bt_addr_le_t* addr = user_data;
			recv_mfg_data = (struct adv_mfg_data *)data->data;
			
			char addr_str[BT_ADDR_LE_STR_LEN] = { 0 };
			bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
			uint64_t hw_id = sys_le64_to_cpu(recv_mfg_data->hw_id);
			LOG_WRN(
				"SCANNED AND FILTERED peer of addr %s and hw_id %" PRIu64,
				addr_str,
				hw_id
			);
		}
		return false;
	default:
		return true;
	}
}

static void scan_filter_match(struct bt_scan_device_info *device_info,
			      struct bt_scan_filter_match *filter_match,
			      bool connectable)
{
	bt_addr_le_t addr;
	bt_addr_le_copy(&addr, device_info->recv_info->addr);
	bt_data_parse(device_info->adv_data, data_cb, &addr);
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL, NULL, NULL);

static void adv_scanned_cb(struct bt_le_ext_adv *adv,
			struct bt_le_ext_adv_scanned_info *info)
{
	ARG_UNUSED(adv);
	ARG_UNUSED(info);

	LOG_DBG("SCANNING is working (I've been scanned)");
}

const static struct bt_le_ext_adv_cb adv_cb = {
	.scanned = adv_scanned_cb,
};

static int adv_start(void)
{
	int err;
	struct bt_le_ext_adv_start_param ext_adv_start_param = {0};

	

	if (adv) {
		err = bt_le_ext_adv_stop(adv);
		if (err) {
			LOG_ERR("Failed to stop extended advertising  (err %d)", err);
			return err;
		}
			err = bt_le_ext_adv_delete(adv);
		if (err) {
			LOG_ERR("Failed to delete advertising set  (err %d)", err);
			return err;
		}
	}

	err = bt_le_ext_adv_create(adv_param, &adv_cb, &adv);
	if (err) {
		LOG_ERR("Failed to create advertising set (err %d)", err);
		return err;
	}

	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Failed setting adv data (err %d)", err);
		return err;
	}

	err = bt_le_ext_adv_start(adv, &ext_adv_start_param);
	if (err) {
		LOG_ERR("Failed to start extended advertising  (err %d)", err);
		return err;
	}

	return err;
}

static void adv_work_handle(struct k_work *item)
{
	adv_start();
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	adv_param = &adv_param_noconn;
	k_work_submit(&adv_work);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	adv_param = &adv_param_conn;
	k_work_submit(&adv_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};


BUILD_ASSERT(CONFIG_BT_ID_MAX >= 2, "Insufficient number of available Bluetooth identities");

static int scan_start(void)
{
	int err;

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_MANUFACTURER_DATA, &scan_mfg_data);
	if (err) {
		LOG_ERR("Scanning filters cannot be set (err %d)", err);
		return err;
	}

	err = bt_scan_filter_enable(BT_SCAN_MANUFACTURER_DATA_FILTER, false);
	if (err) {
		LOG_ERR("Filters cannot be turned on (err %d)", err);
		return err;
	}

	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if(err == -EALREADY) { err = 0; } // Mesh may already started to scan
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)", err);
		return err;
	}

	return err;
}

static int prepare_identity(void)
{
	size_t id_count = 0xFF;

	/* Use different identity from Bluetooth Mesh to avoid conflicts with Mesh Provisioning
	 * Service and Mesh Proxy Service advertisements.
	 */
	(void)bt_id_get(NULL, &id_count);

	if (id_count != 1) {
		LOG_ERR("Expected only default identity so far, there currently are %d", id_count);
		return -EINVAL;
	}

	int id = bt_id_create(NULL, NULL);
	if (id < 0) {
		LOG_ERR("Failed to create identity (err %d)", id);
		return id;
	}

	adv_param_conn.id = id;
	adv_param_noconn.id = id;

	return 0;
}

int peer_start() {
	
	int err = 0;

	mfg_data.company_code = sys_cpu_to_le16(CONFIG_BT_COMPANY_ID_NORDIC);
	mfg_data.support_peer_code = sys_cpu_to_le32(SUPPORT_PEER_CODE);
	mfg_data.hw_id = dev_uid64; // From hw_config.h

	err = prepare_identity();
	if (err) {
		LOG_ERR("Failed to prepare identity (err %d)", err);
		return err;
	}
	
	err = adv_start();
	if (err) {
		LOG_ERR("Failed to start advertising (err %d)", err);
		return err;
	}

	err = scan_start();
	if (err) {
		LOG_ERR("Failed to start scanning (err %d)", err);
		return err;
	}
	
	return err;
}