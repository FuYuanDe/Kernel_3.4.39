#include "ra_sw.h"
//#include "rtl8366_smi.h"

#define   RTL8367_PORT_STATUS_REG(_p)		(0x1352 + (_p))
#define   RTL8367_PORT_STATUS_NWAY		BIT(7)
#define   RTL8367_PORT_STATUS_TXPAUSE		BIT(6)
#define   RTL8367_PORT_STATUS_RXPAUSE		BIT(5)
#define   RTL8367_PORT_STATUS_LINK		BIT(4)
#define   RTL8367_PORT_STATUS_DUPLEX		BIT(2)
#define   RTL8367_PORT_STATUS_SPEED_MASK	0x0003
#define   RTL8367_PORT_STATUS_SPEED_10		0
#define   RTL8367_PORT_STATUS_SPEED_100		1
#define   RTL8367_PORT_STATUS_SPEED_1000	2


#define get_state(_dev) container_of((_dev), struct ra_sw_state, dev)

#define DEF_VLAN_ID1 4093
#define DEF_VLAN_ID2 4092
extern acl_cb_t *acl_cb;

struct ra_sw_state *g_sw_state;
int  max_sw_VLAN,err;

u16 GTH1_vid=DEF_VLAN_ID1,GTH0_vid=DEF_VLAN_ID2;

rwlock_t mr_lock_vid = __RW_LOCK_UNLOCKED(mr_lock_vid);

static int clear_vlan(struct ra_sw_state *state,int i)
{
	state->vlans[i].ports=0;
	state->vlans[i].untaggeds=0;
	state->vlans[i].valid=0;
	state->vlans[i].vid=0;
	return 0;
}

static int refresh_defult_port(struct ra_sw_state *state)
{
	u16 ports=0;
	unsigned int i;
	//如果用户对端口0进行了修改，则直接删除wan口对应的vlan，不再需要该默认配置
	if(state->ports[PORT0].pvid!=0)
	{
		clear_vlan(state,MAX_SW_VLANS-2);
	}
	else
	{
		state->vlans[MAX_SW_VLANS-2].ports=0x01;
		state->vlans[MAX_SW_VLANS-2].untaggeds=0x01;//包含的所有端口为access口
	}
	//对端口1~4进行检测，如果所有端口被用户修改，则删除lan口的默认vlan配置
	//用户只定义了部分端口，则将用户定义的端口从默认lan-vlan中剔除
	for(i=PORT1;i<=PORT4;i++)
	{
		if(state->ports[i].pvid==0)//用户没有对端口做过修改，属于默认配置vlan
		{
			unsigned int bitmask = (1<<i);
			ports |= bitmask;
			
			state->vlans[MAX_SW_VLANS-1].ports=ports;
			state->vlans[MAX_SW_VLANS-1].untaggeds=ports;//包含的所有端口为access口
		}
	}
	if((state->ports[1].pvid)&&(state->ports[2].pvid)&&(state->ports[3].pvid)
		&&(state->ports[4].pvid))
	{
		clear_vlan(state,MAX_SW_VLANS-1);
	}

	return 0;
		
}


static int conflict_4092(struct ra_sw_state *state)
{
	u16 i,j;

	//获取一个随机vid=i给默认vlan
	for(i=1;i<4095;i++)
	{
		for(j=0;j<MAX_SW_VLANS;j++)
		{
			if(i==state->vlans[j].vid)
			break;
		}
		if(j==MAX_SW_VLANS)
			break;
	}
	state->vlans[MAX_SW_VLANS-2].vid=i;
	return 0;
}
static int conflict_4093(struct ra_sw_state *state)
{
	u16 i,j;

	//获取一个随机vid=i给默认vlan
	for(i=1;i<4095;i++)
	{
		for(j=0;j<MAX_SW_VLANS;j++)
		{
			if(i==state->vlans[j].vid)
				break;
		}
		if(j==MAX_SW_VLANS)
			break;
	}
	state->vlans[MAX_SW_VLANS-1].vid=i;
	return 0;
	
}
static int resolve_vlan_conflict(struct ra_sw_state *state,int vid,int i)
{
	/*当用户分配的vid为4093时，分配一个随机数给默认VLAN*/
	if((vid==DEF_VLAN_ID1)&&(i<MAX_SW_VLANS-2))
		conflict_4093(state);

	/*当用户分配的vid为4092时，分配一个随机数给默认VLAN*/
	if((vid==DEF_VLAN_ID2)&&(i<MAX_SW_VLANS-2))
		conflict_4092(state);
	/*默认端口被用户定义时需要刷新*/
	refresh_defult_port(state);
	
	write_lock_irq(&mr_lock_vid);
	GTH0_vid=state->vlans[MAX_SW_VLANS-2].vid;
	GTH1_vid=state->vlans[MAX_SW_VLANS-1].vid;
	write_unlock_irq(&mr_lock_vid);
	return 0;
}

static int ra_sw_do_apply_vlan(struct ra_sw_state *state)
{
	int i;
	u16 port,vid,valid,untaggeds;
	rtk_vlan_cfg_t vlan_cfg;
	
	
	for(i=0;i<MAX_SW_VLANS;i++)
		{
			vid=state->vlans[i].vid;
			resolve_vlan_conflict(state,vid,i);//解决vlan_id冲突
			
			port=state->vlans[i].ports;
			vid=state->vlans[i].vid;
			valid=state->vlans[i].valid;
			untaggeds=state->vlans[i].untaggeds;
			
			if ((valid == 0) || (vid < 0) || (vid > 4095)) 
				continue;
			
			memset(&vlan_cfg, 0, sizeof(vlan_cfg));
			vlan_cfg.mbr.bits[0] = (1 << EXT_PORT0) | port;
			vlan_cfg.untag.bits[0] = untaggeds;
			
			mutex_lock(&acl_cb->lock);
			if(RT_ERR_OK != rtk_vlan_set(vid, &vlan_cfg))
			{
				printk(KERN_ERR "%s %d\n", __FUNCTION__, __LINE__);
			}
			mutex_unlock(&acl_cb->lock);

			//mutex_lock(&acl_cb->lock);
			//rtk_vlan_get(state->vlans[i].vid, &vlan_cfg);
			//mutex_unlock(&acl_cb->lock);
			//printk(KERN_ALERT "state->vlans[%d].vid=%d\n",i,state->vlans[i].vid);
			//printk(KERN_ALERT "state->vlans[%d].untaggeds==%x\n",i,vlan_cfg.untag.bits[0]);
			//printk(KERN_ALERT "state->vlans[%d].ports==%x\n",i,vlan_cfg.mbr.bits[0]);
		}
	return 0;
}
EXPORT_SYMBOL(GTH0_vid);
EXPORT_SYMBOL(GTH1_vid);
EXPORT_SYMBOL(mr_lock_vid);

static int get_default_pvid(struct ra_sw_state *state)
{
	int i;
	if(state->ports[PORT0].pvid==0)
	{
		state->ports[PORT0].pvid=state->vlans[MAX_SW_VLANS-2].vid;
		state->ports[PORT0].tagged=0;
	}
	for(i=PORT0;i<=PORT4;i++)
	{
		if(state->ports[i].pvid==0)
		{
			state->ports[i].pvid=state->vlans[MAX_SW_VLANS-1].vid;
			state->ports[i].tagged=0;
		}
	}
	return 0;
}

static int ra_sw_do_apply_port(struct ra_sw_state *state)
{
	int i;
	u16 pvid;
	get_default_pvid(state);
	mutex_lock(&acl_cb->lock);
	for(i=PORT0;i<=PORT4;i++)
	{
		pvid=state->ports[i].pvid;
		rtk_vlan_portPvid_set(i, pvid, 0);
		//rtl8367c_setAsicVlanPortBasedVID(0, 1, 0);
		//rtl8367c_setAsicVlanPortBasedVID(1, 2, 0);

		if(state->ports[i].tagged==1)
		{
			rtk_vlan_tagMode_set(i, VLAN_TAG_MODE_ORIGINAL);
			rtk_vlan_portAcceptFrameType_set(i, ACCEPT_FRAME_TYPE_TAG_ONLY);
		}
		else
		{
			rtk_vlan_tagMode_set(i, VLAN_TAG_MODE_KEEP_FORMAT);
			rtk_vlan_portAcceptFrameType_set(i, ACCEPT_FRAME_TYPE_UNTAG_ONLY);
		}
	}


	rtk_vlan_portAcceptFrameType_set(EXT_PORT0, ACCEPT_FRAME_TYPE_TAG_ONLY);
	
	rtk_vlan_tagMode_set(EXT_PORT0, VLAN_TAG_MODE_ORIGINAL);
	mutex_unlock(&acl_cb->lock);

	
	return 0;
}
static int ra_sw_apply(struct switch_dev *dev)
{
	//printk(KERN_ALERT "ra_sw_apply$$$$$$$$$$$$$$$$$$$$$$\n");
	struct ra_sw_state *state = get_state(dev);
	ra_sw_do_apply_vlan(state); 
    ra_sw_do_apply_port(state);

	return 0;
}

static int ra_sw_vlan_get_vid(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
	int vlan = val->port_vlan;

    
	if (vlan < 0 || vlan >= MAX_SW_VLANS)
		return -EINVAL;

    val->value.i = state->vlans[vlan].vid;

	return 0;
}

static int ra_sw_vlan_set_vid(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
	int vlan = val->port_vlan;
	u16 vid  = val->value.i;
    
    
	if (vlan < 0 || vlan >= MAX_SW_VLANS)
		return -EINVAL;

	if (vid < 0 || vid > 4095)
		return -EINVAL;

    state->vlans[vlan].vid = vid;
    
	return 0;
}

static int ra_sw_port_get_pvid(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
    int  port = val->port_vlan;
    

    if (port < 0 || port >= max_sw_ports)
        return -EINVAL;
   
    val->value.i = state->ports[port].pvid;
	
	return 0;
}

static int ra_sw_port_set_pvid(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
    int  port = val->port_vlan;
    u16  pvid  = val->value.i;


    if (port < 0 || port >= max_sw_ports)
        return -EINVAL;

    if (pvid < 0 || pvid > 4095)
		return -EINVAL;

    state->ports[port].pvid = pvid;
    
	return 0;
}
static int ra_sw_vlan_get_valid(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
	int vlan = val->port_vlan;


	if (vlan < 0 || vlan >= MAX_SW_VLANS)
		return -EINVAL;

    val->value.i = state->vlans[vlan].valid;

	return 0;
}

static int ra_sw_vlan_set_valid(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
	int vlan  = val->port_vlan;
	u16 valid = val->value.i;


	if (vlan < 0 || vlan >= MAX_SW_VLANS)
		return -EINVAL;

	if (valid < 0 || valid > 1)
		return -EINVAL;

    state->vlans[vlan].valid = valid;
    
	return 0;
}

static int ra_sw_port_get_tagged(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
    int  port = val->port_vlan;
    

	if (port < 0 || port >= max_sw_ports)
		return -EINVAL;

    val->value.i = state->ports[port].tagged;
    
	return 0;
}

static int ra_sw_port_set_tagged(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
    int  port   = val->port_vlan;
    int  tagged = val->value.i;


    if (port < 0 || port >= max_sw_ports)
		return -EINVAL;

    if (tagged < 0 || tagged > 2)
        return -EINVAL;

    state->ports[port].tagged = tagged;
    
	return 0;

}

static int ra_sw_vlan_get_ports(struct switch_dev *dev, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
    int vlan = val->port_vlan;
	int id, len;
	u16 ports;

    
	if (vlan < 0 || vlan >= MAX_SW_VLANS)
		return -EINVAL;

	ports = state->vlans[vlan].ports;
	id = 0;
	len = 0;
	while (id < max_sw_ports)
    {
		if (ports & 1)
        {
			int istagged = state->ports[id].tagged;
			val->value.ports[len].id = id;
			val->value.ports[len].flags = (istagged << SWITCH_PORT_FLAG_TAGGED);
			len++;
		}
		id++;
		ports >>= 1;
	}
	val->len = len;

	return 0;
}

static int ra_sw_vlan_set_ports_taggeds(struct switch_dev *dev, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
    int vlan = val->port_vlan;
    u16 ports = 0;
	u16 untaggeds=0;
	int i;

    
	if (vlan < 0 || vlan >= MAX_SW_VLANS)
		return -EINVAL;
    
	for (i = 0; i < val->len; i++) {
		unsigned int id = val->value.ports[i].id;
        unsigned int bitmask = (1<<id);
		ports |= bitmask;
		if(state->ports[id].tagged==0)
		{
			//printk(KERN_ALERT "state->ports[%d].tagged==%d\n",id,state->ports[id].tagged);
			unsigned int untag_mask=(1<<id);
			untaggeds |= untag_mask;
		}
		
	}

    state->vlans[vlan].ports = ports;
	state->vlans[vlan].untaggeds=untaggeds;

	return 0;

}

/** Get the current phy address */
static int ra_sw_get_phy(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);

	val->value.i = state->proc_mii.p;
	return 0;
}

/** Set a new phy address for low level access to registers */
static int ra_sw_set_phy(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
	int new_phy = val->value.i;

    
	if (new_phy < 0 || new_phy > 31)
		state->proc_mii.p = (u16)-1;
	else
		state->proc_mii.p = (u16)new_phy;
	return 0;
}

/** Get the current register number */
static int ra_sw_get_mii(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);

	val->value.i = state->proc_mii.m;
	return 0;
}

/** Set a new register address for low level access to registers */
static int ra_sw_set_mii(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
	int new_mii = val->value.i;

    
	if (new_mii < 0 || new_mii > 0x2714)
		state->proc_mii.m = (u16)-1;
	else
		state->proc_mii.m = (u16)new_mii;
	return 0;
}


static int ra_sw_rtl8367_get_port_link(struct switch_dev *dev, int port)
{
    struct ra_sw_state *state = get_state(dev);
    int speed;
	rtk_uint32 regData;

    rtl8367c_getAsicReg(RTL8367C_REG_PORT0_STATUS+port , &regData);

	
    state->ports[port].link.link = ((regData>>4) & 0x1) ? true : false;
    if (state->ports[port].link.link)
    {
        rtl8367c_getAsicReg(RTL8367C_REG_PORT0_STATUS+port , &regData);
		
        speed  = regData & 0x03;
        state->ports[port].link.duplex = ((regData>>2) & 0x1) ? true : false;

        if (speed==0x00)
            state->ports[port].link.speed = SWITCH_PORT_SPEED_10;
        else if(speed==0x01)
            state->ports[port].link.speed = SWITCH_PORT_SPEED_100;
		else if(speed==0x02)
			state->ports[port].link.speed = SWITCH_PORT_SPEED_1000;
    }
    else
    {
        state->ports[port].link.speed = SWITCH_PORT_SPEED_UNKNOWN;
    }

    rtl8367c_getAsicReg(RTL8367C_REG_PORT0_STATUS+port , &regData);
    state->ports[port].link.aneg = ((regData>>7) & 0x1) ? true : false;
    
    return 0;
}

static int ra_sw_port_get_status(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
	int  port = val->port_vlan;
	char *buf = state->buf; // fixed-length at 80.
	int  len;


	if (port < 0 || port >= max_sw_ports)
		return -EINVAL;
	
	mutex_lock(&acl_cb->lock);
   	ra_sw_rtl8367_get_port_link(dev, port);
	mutex_unlock(&acl_cb->lock);

    
    if (state->ports[port].link.link)
		len = snprintf(buf,80, "link up, %d Mbps, %s duplex",
		                    state->ports[port].link.speed,
		                    state->ports[port].link.duplex ? "full" : "half");
	else
		len = snprintf(buf,80, "link down");

    
    if (state->ports[port].link.aneg)
		len += snprintf(buf+len,80, ", auto negotiate");
	else
		len += snprintf(buf+len,80, ", fixed speed");

    
	buf[len] = '\0';
	val->value.s = buf;
    
	return 0;
}


static int ra_sw_port_get_speed(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
	int port = val->port_vlan;
    

	if (port < 0 || port >= max_sw_ports)
		return -EINVAL;
	mutex_lock(&acl_cb->lock);
    ra_sw_rtl8367_get_port_link(dev, port);
	mutex_unlock(&acl_cb->lock);
	
    val->value.i = state->ports[port].link.speed;

	return 0;
}

static int ra_sw_get_val(struct switch_dev *dev, const struct switch_attr *attr, struct switch_val *val)
{
	struct ra_sw_state *state = get_state(dev);
	int retVal = -EINVAL;
	rtk_uint32 phyData;

    //retVal = geth_phy_read(state->proc_mii.p, state->proc_mii.m) & 0xffff;
    mutex_lock(&acl_cb->lock);
	retVal = rtl8367c_getAsicPHYReg(state->proc_mii.p,state->proc_mii.m,&phyData);
	//mutex_unlock(&acl_cb->lock);
	if (RT_ERR_OK != retVal)
	{
		printk(KERN_ERR "%s: failed! retVal=0x%x\n", __func__, retVal);
		mutex_unlock(&acl_cb->lock);
		return RT_ERR_FAILED;
	}
    else
    {
		val->value.i = phyData;
		mutex_unlock(&acl_cb->lock);
		return 0;
	}
	
}


static int ra_sw_reset(struct switch_dev *dev)
{
	int i;
	struct ra_sw_state *state = get_state(dev);


	rtk_vlan_init();

    
    memset(state->ports, 0, sizeof(state->ports));
    memset(state->vlans, 0, sizeof(state->vlans));   

	state->ports[0].pvid=DEF_VLAN_ID2;
	state->ports[0].tagged=0;
	state->ports[1].pvid=DEF_VLAN_ID1;
	state->ports[1].tagged=0;
	state->ports[2].pvid=DEF_VLAN_ID1;
	state->ports[2].tagged=0;
	state->ports[3].pvid=DEF_VLAN_ID1;
	state->ports[3].tagged=0;
	state->ports[4].pvid=DEF_VLAN_ID1;
	state->ports[4].tagged=0;


    for (i = 0; i < MAX_SW_VLANS; i++)
    {
		state->vlans[i].vid = (i+1);
		state->vlans[i].vid = 0;
        state->vlans[i].ports = 0x0;
        state->vlans[i].valid = 0;
	}
	
	state->vlans[14].valid=1;
	state->vlans[14].ports=0x1;
	state->vlans[14].untaggeds=0x1;
	state->vlans[14].vid=DEF_VLAN_ID2;
	
	state->vlans[15].valid=1;
	state->vlans[15].ports=0x1e;
	state->vlans[15].untaggeds=0x1e;
	state->vlans[15].vid=DEF_VLAN_ID1;
    
	return 0;
}


enum Globals {
	RA_SW_REGISTER_PHY,
	RA_SW_REGISTER_MII,
	RA_SW_REGISTER_VALUE,
	RA_SW_REGISTER_ERRNO,
};

enum Vlans {
	RA_SW_VLAN_VID,
    RA_SW_VLAN_VALID,
};

enum Ports {
	RA_SW_PORT_STATUS,
	RA_SW_PORT_SPEED,
	RA_SW_PORT_TAGGED,
	RA_SW_PORT_PVID,
};

static const struct switch_attr ra_sw_global[] = {
	[RA_SW_REGISTER_PHY] = {
		.id = RA_SW_REGISTER_PHY,
		.type = SWITCH_TYPE_INT,
		.description = "register access: set PHY address (0-31)",
		.name  = "phy",
		.get  = ra_sw_get_phy,
		.set  = ra_sw_set_phy,
	},
	[RA_SW_REGISTER_MII] = {
		.id = RA_SW_REGISTER_MII,
		.type = SWITCH_TYPE_INT,
		.description = "register access: set MII register (0-0x2714)",
		.name  = "mii",
		.get  = ra_sw_get_mii,
		.set  = ra_sw_set_mii,
	},
	[RA_SW_REGISTER_VALUE] = {
		.id = RA_SW_REGISTER_VALUE,
		.type = SWITCH_TYPE_INT,
		.description = "register access: r/w register value (0-65535)",
		.name  = "val",
		.get  = ra_sw_get_val,
		.set  = NULL,
	},
};


static const struct switch_attr ra_sw_vlan[] = {
	[RA_SW_VLAN_VID] = {
		.id = RA_SW_VLAN_VID,
		.type = SWITCH_TYPE_INT,
		.description = "VLAN ID (0-4095)",
		.name = "vid",
		.get = ra_sw_vlan_get_vid,
		.set = ra_sw_vlan_set_vid,
	},

	[RA_SW_VLAN_VALID] = {
		.id = RA_SW_VLAN_VALID,
		.type = SWITCH_TYPE_INT,
		.description = "0 = invalid, 1 = valid",
		.name = "valid",
		.get = ra_sw_vlan_get_valid,
		.set = ra_sw_vlan_set_valid,
	}

};

static const struct switch_attr ra_sw_port[] = {
	[RA_SW_PORT_STATUS] = {
		.id = RA_SW_PORT_STATUS,
		.type = SWITCH_TYPE_STRING,
		.description = "Detailed port status",
		.name  = "status",
		.get  = ra_sw_port_get_status,
		.set  = NULL,
	},
	[RA_SW_PORT_SPEED] = {
		.id = RA_SW_PORT_SPEED,
		.type = SWITCH_TYPE_INT,
		.description = "Show link speed",
		.name  = "speed",
		.get  = ra_sw_port_get_speed,
		.set  = NULL,
	},
	[RA_SW_PORT_TAGGED] = {
		.id = RA_SW_PORT_TAGGED,
		.type = SWITCH_TYPE_INT,
		.description = "0 = do not alter, 1 = add tags, 2 = remove tag",
		.name  = "tagged",
		.get  = ra_sw_port_get_tagged,
		.set  = ra_sw_port_set_tagged,
	},
	[RA_SW_PORT_PVID] = {
		.id = RA_SW_PORT_PVID,
		.type = SWITCH_TYPE_INT,
		.description = "get/set port pvid",
		.name  = "pvid",
		.get  = ra_sw_port_get_pvid,
		.set  = ra_sw_port_set_pvid,
	},
};


static const struct switch_dev_ops ra_sw_ops = {

	.attr_global = {
		.attr = ra_sw_global,
		.n_attr = ARRAY_SIZE(ra_sw_global),
	},
	
	.attr_port = {
		.attr = ra_sw_port,
		.n_attr = ARRAY_SIZE(ra_sw_port),
	},
	
	.attr_vlan = {
		.attr = ra_sw_vlan,
		.n_attr = ARRAY_SIZE(ra_sw_vlan),
	},

    .get_vlan_ports = ra_sw_vlan_get_ports,
	.set_vlan_ports = ra_sw_vlan_set_ports_taggeds,
	
	.get_port_pvid  = NULL,
	.set_port_pvid  = NULL,

	.apply_config   = ra_sw_apply,
	.reset_switch   = ra_sw_reset,   
};

int ra_sw_probe(void)
{
	struct switch_dev *dev;
	
	rtk_vlan_init();	
	g_sw_state = kzalloc(sizeof(struct ra_sw_state), GFP_KERNEL);
	if (!g_sw_state)
		return -ENOMEM;

    memset(g_sw_state, 0, sizeof(struct ra_sw_state));
		
	dev = &g_sw_state->dev;
    dev->name     = "rtl8367";
	dev->alias    = "rtl8367";
	dev->vlans    = MAX_SW_VLANS;
	dev->ports    = max_sw_ports;
    dev->cpu_port = cpu_sw_ports;
	dev->ops      = &ra_sw_ops;

	err = register_switch(dev, NULL);
	if (err) {
		printk("register rtl8367 switch failed!\n");
		return err;
		}
	printk("rtl8367 swconfig founded.\n");
	return 0;
}


























