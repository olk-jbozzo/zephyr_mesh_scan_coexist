#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/mesh.h>

#include "fake_kconfig.h"

#include "node.h"
#include "hw_config.h"

/* TODO: Parametrized logging */
LOG_MODULE_REGISTER(node, LOG_LEVEL_DBG);

static void prov_complete(uint16_t net_idx, uint16_t addr)
{
	LOG_INF("================");
	LOG_INF("NODE PROVISIONED");
	LOG_INF("================");
}

static void prov_reset(void)
{
	(void)bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
}

const struct bt_mesh_prov node_prov = {
	.uuid = dev_uuid,
	.complete = prov_complete,
	.reset = prov_reset,
};

static const struct bt_mesh_model sig_models[] = {
	BT_MESH_MODEL_CFG_SRV,
};

static struct bt_mesh_model vnd_models[] = { };

static const struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0, sig_models, vnd_models),
};

const struct bt_mesh_comp node_mesh_comp = {
	.cid = CONFIG_BL_COMPOSITION_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

int node_start(void)
{
	int err = 0;
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		err = settings_load();
        if (err) {
            LOG_ERR("Failed to load settings (err %d)", err);
        }
	}

	err = bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
    if (err) {
        LOG_ERR("Failed to enable provisioning (err %d)", err);
        return err;
    }

	LOG_INF("Mesh initialized");
    return err;
}
