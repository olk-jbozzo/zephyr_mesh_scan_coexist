#include <zephyr/bluetooth/mesh.h>
#include <zephyr/bluetooth/mesh/cfg_cli.h>

#include "fake_kconfig.h"

#include "hw_config.h"
#include "provisioning.h"

/* TODO: Parametrized logging */
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(provisioning, LOG_LEVEL_DBG);

K_SEM_DEFINE(sem_unprov_beacon, 0, 1);
K_SEM_DEFINE(sem_node_added, 0, 1);

K_THREAD_STACK_DEFINE(provisioning_stack_area, CONFIG_BL_MESH_PROVISIONING_STACK_SIZE);
struct k_work_q provisioning_queue = { 0 };
const struct k_work_queue_config cfg = {
	.name = "provisioning_queue",
	.no_yield = false,
};

static uint16_t self_addr = 1;
static uint16_t node_addr = 0;
static uint8_t node_uuid[16];
static const uint8_t prov_dev_uuid[16] = { 0xdd, 0xdd };

static const uint16_t net_idx;
static const uint16_t app_idx;

/* Models */
static struct bt_mesh_cfg_cli cfg_cli = { 0 };
static const struct bt_mesh_model sig_models[] = {
	BT_MESH_MODEL_CFG_SRV,
	BT_MESH_MODEL_CFG_CLI(&cfg_cli),
};

static struct bt_mesh_model vnd_models[] = { };

static const struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0, sig_models, vnd_models),
};

const struct bt_mesh_comp provisioner_mesh_comp = {
	.cid = CONFIG_BL_COMPOSITION_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

/* Provisioning */
static void provisining_unprovisioned_beacon(
    uint8_t uuid[16],
	bt_mesh_prov_oob_info_t oob_info,
	uint32_t *uri_hash
);
static void provisining_node_added(
    uint16_t idx,
    uint8_t uuid[16],
    uint16_t addr,
    uint8_t num_elem
);

const struct bt_mesh_prov provisioner_prov = {
	.uuid = prov_dev_uuid,
	.unprovisioned_beacon = provisining_unprovisioned_beacon,
	.node_added = provisining_node_added,
};


static void provisining_unprovisioned_beacon(
    uint8_t uuid[16],
	bt_mesh_prov_oob_info_t oob_info,
	uint32_t *uri_hash
) {
	memcpy(node_uuid, uuid, 16);
	k_sem_give(&sem_unprov_beacon);
}

static void provisining_node_added(
    uint16_t idx,
    uint8_t uuid[16],
    uint16_t addr,
    uint8_t num_elem
) {
	node_addr = addr;
	k_sem_give(&sem_node_added);
}

static void setup_cdb(void)
{
	struct bt_mesh_cdb_app_key *key;
	uint8_t app_key[16];
	int err;

	key = bt_mesh_cdb_app_key_alloc(net_idx, app_idx);
	if (key == NULL) {
		LOG_ERR("Failed to allocate app-key 0x%04x", app_idx);
		return;
	}

	bt_rand(app_key, 16);

	err = bt_mesh_cdb_app_key_import(key, 0, app_key);
	if (err) {
		LOG_ERR("Failed to import appkey into cdb. Err:%d", err);
		return;
	}

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		bt_mesh_cdb_app_key_store(key);
	}
}

/* Init */
int provisining_init()
{
	int err = 0;

	/* UUID must be set by now */
	memcpy(dev_uuid, provisioner_prov.uuid, 16);

	k_work_queue_init(&provisioning_queue);
	k_work_queue_start(
		&provisioning_queue,
		provisioning_stack_area,
		K_THREAD_STACK_SIZEOF(provisioning_stack_area),
		CONFIG_BL_MESH_PROVISIONING_PRIORITY,
		&cfg
	);

	return 0;
}

/* Configuration loop */
static void configure_node(struct bt_mesh_cdb_node *node)
{
	NET_BUF_SIMPLE_DEFINE(buf, BT_MESH_RX_SDU_MAX);
	struct bt_mesh_comp_p0_elem elem;
	struct bt_mesh_cdb_app_key *key;
	uint8_t app_key[16];
	struct bt_mesh_comp_p0 comp;
	uint8_t status;
	int err, elem_addr;

	LOG_DBG("Configuring node 0x%04x...", node->addr);

	key = bt_mesh_cdb_app_key_get(app_idx);
	if (key == NULL) {
		LOG_ERR("No app-key 0x%04x", app_idx);
		return;
	}

	err = bt_mesh_cdb_app_key_export(key, 0, app_key);
	if (err) {
		LOG_ERR("Failed to export appkey from cdb. Err:%d", err);
		return;
	}

	/* Add Application Key */
	err = bt_mesh_cfg_cli_app_key_add(net_idx, node->addr, net_idx, app_idx, app_key, &status);
	if (err || status) {
		LOG_ERR("Failed to add app-key (err %d status %d)", err, status);
		return;
	}

	/* Get the node's composition data and bind all models to the appkey */
	err = bt_mesh_cfg_cli_comp_data_get(net_idx, node->addr, 0, &status, &buf);
	if (err || status) {
		LOG_ERR("Failed to get Composition data (err %d, status: %d)",
		       err, status);
		return;
	}

	err = bt_mesh_comp_p0_get(&comp, &buf);
	if (err) {
		LOG_ERR("Unable to parse composition data (err: %d)", err);
		return;
	}

	elem_addr = node->addr;
	while (bt_mesh_comp_p0_elem_pull(&comp, &elem)) {
		LOG_DBG("Element @ 0x%04x: %u + %u models", elem_addr,
		       elem.nsig, elem.nvnd);
		for (int i = 0; i < elem.nsig; i++) {
			uint16_t id = bt_mesh_comp_p0_elem_mod(&elem, i);

			if (id == BT_MESH_MODEL_ID_CFG_CLI ||
			    id == BT_MESH_MODEL_ID_CFG_SRV) {
				continue;
			}
			LOG_DBG("Binding AppKey to model 0x%03x:%04x",
			       elem_addr, id);

			err = bt_mesh_cfg_cli_mod_app_bind(
				net_idx,
				node->addr,
				elem_addr,
				app_idx,
				id,
				&status
			);
			if (err || status) {
				LOG_ERR(
					"Failed to bind SIG model %d (err: %d, status: %d)", id, err, status
				);
			}
		}

		for (int i = 0; i < elem.nvnd; i++) {
			struct bt_mesh_mod_id_vnd id = bt_mesh_comp_p0_elem_mod_vnd(&elem, i);

			LOG_DBG("Binding AppKey to model 0x%03x:%04x:%04x",
			       elem_addr, id.company, id.id);

			err = bt_mesh_cfg_cli_mod_app_bind_vnd(net_idx, node->addr, elem_addr,
							       app_idx, id.id, id.company, &status);
			if (err || status) {
				LOG_ERR(
					"Failed to bind vnd model %d (company %d) (err: %d, status: %d)",
					id.id, id.company, err, status
				);
				continue;
			}
		}

		elem_addr++;
	}

	atomic_set_bit(node->flags, BT_MESH_CDB_NODE_CONFIGURED);

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		bt_mesh_cdb_node_store(node);
	}

	LOG_DBG("Configuration complete");
}

static void configure_self(struct bt_mesh_cdb_node *self)
{
	struct bt_mesh_cdb_app_key *key;
	uint8_t app_key[16];
	uint8_t status = 0;
	int err;

	LOG_DBG("Configuring self...");

	key = bt_mesh_cdb_app_key_get(app_idx);
	if (key == NULL) {
		LOG_ERR("No app-key 0x%04x", app_idx);
		return;
	}

	err = bt_mesh_cdb_app_key_export(key, 0, app_key);
	if (err) {
		LOG_ERR("Failed to export appkey from cdb. Err:%d", err);
		return;
	}

	#if IS_ENABLED(CONFIG_SENSIBLE_DATA)
		LOG_HEXDUMP_INF(app_key, ARRAY_SIZE(app_key), "Appkey is ");
	#endif

	/* Add Application Key */
	err = bt_mesh_cfg_cli_app_key_add(
		self->net_idx, self->addr, self->net_idx, app_idx,app_key, &status
	);
	if (err || status) {
		LOG_ERR(
			"Failed to add app-key (err %d, status %d)", err, status
		);
		return;
	}

	atomic_set_bit(self->flags, BT_MESH_CDB_NODE_CONFIGURED);

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		bt_mesh_cdb_node_store(self);
	}

	LOG_DBG("Configuration complete");
}

static uint8_t provisioning_check_unconfigured(struct bt_mesh_cdb_node *node, void *data)
{
	if (!atomic_test_bit(node->flags, BT_MESH_CDB_NODE_CONFIGURED)) {
		if (node->addr == self_addr) {
			configure_self(node);
		} else {
			configure_node(node);
		}
	}

	return BT_MESH_CDB_ITER_CONTINUE;
}

static int provisioning_step(void) {
    char uuid_hex_str[32 + 1] = { 0 };
    
	k_sem_reset(&sem_unprov_beacon);
	k_sem_reset(&sem_node_added);
	bt_mesh_cdb_node_foreach(provisioning_check_unconfigured, NULL);

	LOG_DBG("Waiting for unprovisioned beacon...");
	int err = k_sem_take(&sem_unprov_beacon, K_SECONDS(10));
	if (err < 0) {
		return err;
	}

	bin2hex(node_uuid, 16, uuid_hex_str, sizeof(uuid_hex_str));

	LOG_DBG("Provisioning %s", uuid_hex_str);
	err = bt_mesh_provision_adv(node_uuid, net_idx, 0, 0);
	if (err < 0) {
		LOG_DBG("Provisioning failed (err %d)", err);
		return err;
	}

	LOG_DBG("Waiting for node to be added...");
	err = k_sem_take(&sem_node_added, K_SECONDS(10));
	if (err < 0) {
		LOG_DBG("Timeout waiting for node to be added (err: %d)", err);
		return err;
	}

	LOG_DBG("Added node 0x%04x", node_addr);
	return 0;
}

static void provisioning_work_cb(struct k_work *item) {
	struct k_work_delayable *dwork = k_work_delayable_from_work(item);
	provisioning_step();
	int error_code = k_work_reschedule_for_queue(&provisioning_queue, dwork, K_SECONDS(5));
	if(error_code < 0) {
		LOG_ERR("Failed to reschedule provisioning work (err %d)", error_code);
	}
}

K_WORK_DELAYABLE_DEFINE(provisioning_work, provisioning_work_cb);

int provisioning_start(void) {

	uint8_t net_key[16] = {0};
    uint8_t dev_key[16] = {0};

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		LOG_INF("Loading stored settings");
		settings_load();
	}

	bt_rand(net_key, 16);

	#if IS_ENABLED(CONFIG_SENSIBLE_DATA)
		for(int i = 0; i < 16; i++) {
			net_key[i] = i;
		}
		LOG_HEXDUMP_INF(net_key, ARRAY_SIZE(net_key), "Netkey is ");
	#endif

	int err = bt_mesh_cdb_create(net_key);
	if (err == -EALREADY) {
		LOG_DBG("Using stored CDB");
	} else if (err) {
		LOG_ERR("Failed to create CDB (err %d)", err);
		return err;
	} else {
		LOG_DBG("Created CDB");
		setup_cdb();
	}

	#if IS_ENABLED(CONFIG_SENSIBLE_DATA)
		bt_mesh_cdb_iv_update(0, true);
		LOG_INF("IV index is: %d", 0);
	#endif

	bt_rand(dev_key, 16);

	err = bt_mesh_provision(
        net_key,
        BT_MESH_NET_PRIMARY,
        0,
        0,
        self_addr,
		dev_key
    );

    if (err == -EALREADY) {
		LOG_DBG("Using stored settings");
	} else if (err) {
		LOG_ERR("Provisioning failed (err %d)", err);
		return err;
	} else {
		LOG_DBG("Provisioning completed");
	}

	int error_code = k_work_schedule_for_queue(&provisioning_queue, &provisioning_work, K_NO_WAIT);
	if (error_code < 0) {
		LOG_ERR("Failed to submit provisioning work (err %d)", error_code);
		return error_code;
	}
	return 0;
}