#include <stdio.h>
#include "vlan.h"

#define VLAN_SETTINGS "[learning on|off] [fdb auto|N] [stg N]"
#define PORT_LIST     "PORT [PORT [...]]"

static int vlan_add(char *arg)
{
	if (!arg || *arg == 0)
		return -1;

	DBG("%s", arg);

	return 0;
}

static int vlan_del(char *arg)
{
	DBG();
	return 0;
}

static int vlan_set(char *arg)
{
	DBG();
	return 0;
}

static int vlan_attach(char *arg)
{
	DBG();
	return 0;
}

static int vlan_detach(char *arg)
{
	DBG();
	return 0;
}

struct cmd vlan_cmds[] = {
	{ NULL, "add",    0, "[protocol N] " VLAN_SETTINGS, "Add VLAN to bridge",        vlan_add    },
	{ NULL, "del",    0, NULL,                          "Delete VLAN from bridge",   vlan_del    },
	{ NULL, "set",    0, VLAN_SETTINGS,                 "Set VLAN properties",       vlan_set    },
	{ NULL, "attach", 0, PORT_LIST " [tagged]",         "Attach port(s) to VLAN",    vlan_attach },
	{ NULL, "detach", 0, PORT_LIST,                     "Detatch port(s) from VLAN", vlan_detach },
	{ 0 }
};
