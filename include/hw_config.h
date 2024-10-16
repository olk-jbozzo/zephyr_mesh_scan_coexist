#ifndef __HW_CONFIG_H__
#define __HW_CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

extern uint8_t dev_uuid[16];
extern uint64_t dev_uid64;

int hw_init(bool provisioner_button_value_if_not_dk);
bool hw_provisioner_button_pressed(void);

#endif /* __HW_CONFIG_H__ */