#ifndef __MESH_PROVISIONING_H__
#define __MESH_PROVISIONING_H__

extern const struct bt_mesh_comp provisioner_mesh_comp;
extern const struct bt_mesh_prov provisioner_prov;

int provisining_init(void);
int provisioning_start(void);

#endif /* __MESH_PROVISIONING_H__ */