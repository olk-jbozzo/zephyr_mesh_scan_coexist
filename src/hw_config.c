#include <string.h>

#include <zephyr/sys/util_macro.h>
#if IS_ENABLED(CONFIG_HWINFO)
#include <zephyr/drivers/hwinfo.h>
#endif
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_DK_LIBRARY)
#include <dk_buttons_and_leds.h>
#endif

#include "hw_config.h"

LOG_MODULE_REGISTER(hw_config, LOG_LEVEL_DBG);

/* Provisioner button */
#if IS_ENABLED(CONFIG_DK_LIBRARY)
#define PROVISIONER_BUTTON DK_BTN1
#else
#define PROVISIONER_BUTTON 0
#endif

#define PROVISIONER_BUTTON_MSK BIT(PROVISIONER_BUTTON)

static bool provisioner_button_state = false;

#if IS_ENABLED(CONFIG_DK_LIBRARY)
static void button_handler(uint32_t button_state, uint32_t has_changed) {
	if (has_changed & PROVISIONER_BUTTON_MSK) {
		provisioner_button_state = button_state & PROVISIONER_BUTTON_MSK;
		LOG_DBG("Provisioner button state is %s", provisioner_button_state ? "pressed" : "released");
	}
}
#endif

bool hw_provisioner_button_pressed(void) {
	return provisioner_button_state;
}

/* UUID */
uint8_t dev_uuid[16] = { 0xdd, 0xdd };
uint64_t dev_uid64 = { 0 };

static int uuid_from_hw(void) {

	int err = 0;

	if(IS_ENABLED(CONFIG_HWINFO)) {
		err = hwinfo_get_device_id((uint8_t*)(&dev_uid64), sizeof(dev_uid64));
		if (err < 0) {
			LOG_ERR("Failed to get device UID64 (err %d)", err);
			return err;
		}
	} else {
		LOG_WRN("CONFIG_HWINFO is not enabled, using default UID64");
	}

    err = 0;
    if (IS_ENABLED(CONFIG_HWINFO)) {
		err = hwinfo_get_device_id(dev_uuid, sizeof(dev_uuid));
		if (err < 0) {
			LOG_ERR("Failed to get device UUID (err %d)", err);
			return err;
		}
	} else {
		LOG_WRN("CONFIG_HWINFO is not enabled, using default UUID");
	}

	if (err < 0) {
        memset(dev_uuid, 0, sizeof(dev_uuid));
		dev_uuid[0] = 0xdd;
		dev_uuid[1] = 0xdd;
	}

    return 0;
}

/* Init */
int hw_init(bool provisioner_button_value_if_not_dk) {
	int err = 0;

	#if IS_ENABLED(CONFIG_DK_LIBRARY)
	err = dk_buttons_init(button_handler);
	if (err) {
		LOG_ERR("Failed to initialize buttons (err %d)", err);
		return err;
	}

	uint32_t button_state = 0;
	uint32_t has_changed = 0;	
	dk_read_buttons(&button_state, &has_changed);
	has_changed = ~0;
	button_handler(button_state, has_changed);
	#else
	provisioner_button_state = provisioner_button_value_if_not_dk;
	#endif

	err = uuid_from_hw();
	if (err) {
		LOG_ERR("Failed to get device UUID (err %d)", err);
		return err;
	}

	return 0;
}