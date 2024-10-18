/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef __PEER_H_
#define __PEER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/addr.h>
#include <math.h>

struct peer_entry {
	sys_snode_t node;
	bt_addr_le_t bt_addr;
	uint64_t hw_id;
	uint16_t timeout_ms;
};

int peer_start();


#ifdef __cplusplus
}
#endif

#endif /* __PEER_H_ */
