/** @file moal_main.c
  *
  * @brief This file contains the major functions in WLAN
  * driver. 
  *
  * Copyright (C) 2008-2011, Marvell International Ltd. 
  * 
  * This software file (the "File") is distributed by Marvell International 
  * Ltd. under the terms of the GNU General Public License Version 2, June 1991 
  * (the "License").  You may use, redistribute and/or modify this File in 
  * accordance with the terms and conditions of the License, a copy of which 
  * is available by writing to the Free Software Foundation, Inc.,
  * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
  * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
  * this warranty disclaimer.
  *
  */

/********************************************************
Change log:
    10/21/2008: initial version
********************************************************/

#include	"moal_main.h"
#include 	"moal_sdio.h"
#ifdef UAP_SUPPORT
#include    "moal_uap.h"
#endif
#ifdef STA_SUPPORT
#include    "moal_priv.h"
#endif /* STA_SUPPORT */

/********************************************************
		Local Variables
********************************************************/

/** Driver version */
const char driver_version[] =
    "SD8787-%s-M2614" MLAN_RELEASE_VERSION "-GPL" "-(" "FP" FPNUM ")"
#ifdef	DEBUG_LEVEL2
    "-dbg"
#endif
    " ";

/** Firmware name */
char *fw_name = NULL;
int req_fw_nowait = 0;

/** MAC address */
char *mac_addr = NULL;

#ifdef MFG_CMD_SUPPORT
/** Mfg mode */
int mfg_mode = 0;
#endif

/** SDIO interrupt mode (0: INT_MODE_SDIO, 1: INT_MODE_GPIO) */
int intmode = INT_MODE_SDIO;
/** GPIO interrupt pin number */
int gpiopin = 0;

/** Auto deep sleep */
int auto_ds = 0;

/** IEEE PS mode */
int ps_mode = 0;

/** Max Tx buffer size */
int max_tx_buf = 0;

#ifdef STA_SUPPORT
/** Max STA interfaces */
int max_sta_bss = DEF_STA_BSS;
#endif

#ifdef UAP_SUPPORT
/** Max uAP interfaces */
int max_uap_bss = DEF_UAP_BSS;
#endif

#if defined(WFD_SUPPORT)
/** Max WFD interfaces */
int max_wfd_bss = DEF_WFD_BSS;
#endif

#ifdef SDIO_SUSPEND_RESUME
/** PM keep power */
int pm_keep_power = 1;
#endif

#if defined(STA_SUPPORT)
/** 802.11d configuration */
int cfg_11d = 0;
#endif

/** FW download CRC check */
int fw_crc_check = 1;

/** CAL data config file */
char *cal_data_cfg = NULL;
/** Init config file (MAC address, register etc.) */
char *init_cfg = NULL;

/** woal_callbacks */
static mlan_callbacks woal_callbacks = {
    .moal_get_fw_data = moal_get_fw_data,
    .moal_init_fw_complete = moal_init_fw_complete,
    .moal_shutdown_fw_complete = moal_shutdown_fw_complete,
    .moal_send_packet_complete = moal_send_packet_complete,
    .moal_recv_packet = moal_recv_packet,
    .moal_recv_event = moal_recv_event,
    .moal_ioctl_complete = moal_ioctl_complete,
    .moal_alloc_mlan_buffer = moal_alloc_mlan_buffer,
    .moal_free_mlan_buffer = moal_free_mlan_buffer,
    .moal_write_reg = moal_write_reg,
    .moal_read_reg = moal_read_reg,
    .moal_udelay = moal_udelay,
    .moal_write_data_sync = moal_write_data_sync,
    .moal_read_data_sync = moal_read_data_sync,
    .moal_malloc = moal_malloc,
    .moal_mfree = moal_mfree,
    .moal_memset = moal_memset,
    .moal_memcpy = moal_memcpy,
    .moal_memmove = moal_memmove,
    .moal_memcmp = moal_memcmp,
    .moal_get_system_time = moal_get_system_time,
    .moal_init_timer = moal_init_timer,
    .moal_free_timer = moal_free_timer,
    .moal_start_timer = moal_start_timer,
    .moal_stop_timer = moal_stop_timer,
    .moal_init_lock = moal_init_lock,
    .moal_free_lock = moal_free_lock,
    .moal_spin_lock = moal_spin_lock,
    .moal_spin_unlock = moal_spin_unlock,
    .moal_print = moal_print,
    .moal_assert = moal_assert,
};

/** Default Driver mode */
#if defined(STA_SUPPORT) && defined(UAP_SUPPORT)
#if defined(WFD_SUPPORT)
int drv_mode = (DRV_MODE_STA | DRV_MODE_UAP | DRV_MODE_WFD);
#else
int drv_mode = (DRV_MODE_STA | DRV_MODE_UAP);
#endif
#else
#ifdef STA_SUPPORT
int drv_mode = DRV_MODE_STA;
#else
int drv_mode = DRV_MODE_UAP;
#endif /* STA_SUPPORT */
#endif /* STA_SUPPORT & UAP_SUPPORT */

/********************************************************
		Global Variables
********************************************************/

/** Semaphore for add/remove card */
struct semaphore AddRemoveCardSem;
/**
 * the maximum number of adapter supported 
 **/
#define MAX_MLAN_ADAPTER    2
/**
 * The global variable of a pointer to moal_handle
 * structure variable
 **/
moal_handle *m_handle[MAX_MLAN_ADAPTER];

#ifdef DEBUG_LEVEL1
#ifdef DEBUG_LEVEL2
#define	DEFAULT_DEBUG_MASK	(0xffffffff)
#else
#define DEFAULT_DEBUG_MASK	(MMSG | MFATAL | MERROR)
#endif /* DEBUG_LEVEL2 */
t_u32 drvdbg = DEFAULT_DEBUG_MASK;
t_u32 ifdbg = 0;
#endif /* DEBUG_LEVEL1 */

int woal_open(struct net_device *dev);
int woal_close(struct net_device *dev);
int woal_set_mac_address(struct net_device *dev, void *addr);
void woal_tx_timeout(struct net_device *dev);
struct net_device_stats *woal_get_stats(struct net_device *dev);

/********************************************************
		Local Functions
********************************************************/

/**
 *  @brief This function dynamically populates the driver mode table
 *
 *  @param handle           A pointer to moal_handle structure
 *  @param drv_mode_local   Driver mode
 *
 *  @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static mlan_status
wlan_update_drv_tbl(moal_handle * handle, int drv_mode_local)
{
    mlan_status ret = MLAN_STATUS_SUCCESS;
    unsigned int intf_num = 0;
    int i = 0, j = 0;
    mlan_bss_attr *bss_tbl = NULL;

    ENTER();

    /* Calculate number of interfaces */
#ifdef STA_SUPPORT
    if (drv_mode_local & DRV_MODE_STA) {
        if ((max_sta_bss < 1) || (max_sta_bss > MAX_STA_BSS)) {
            PRINTM(MWARN, "Unsupported max_sta_bss (%d), setting to default\n",
                   max_sta_bss);
            max_sta_bss = DEF_STA_BSS;
        }
        intf_num += max_sta_bss;
    }
#endif /* STA_SUPPORT */

#ifdef UAP_SUPPORT
    if (drv_mode_local & DRV_MODE_UAP) {
        if ((max_uap_bss < 1) || (max_uap_bss > MAX_UAP_BSS)) {
            PRINTM(MWARN, "Unsupported max_uap_bss (%d), setting to default\n",
                   max_uap_bss);
            max_uap_bss = DEF_UAP_BSS;
        }
        intf_num += max_uap_bss;
    }
#endif /* UAP_SUPPORT */

#if defined(WFD_SUPPORT)
    if (drv_mode_local & DRV_MODE_WFD) {
        if ((max_wfd_bss < 1) || (max_wfd_bss > MAX_WFD_BSS)) {
            PRINTM(MWARN, "Unsupported max_wfd_bss (%d), setting to default\n",
                   max_wfd_bss);
            max_wfd_bss = DEF_WFD_BSS;
        }
        intf_num += max_wfd_bss;
    }
#endif /* WFD_SUPPORT && V14_FEATURE */

    /* Create BSS attribute table */
    if ((intf_num == 0) || (intf_num > MLAN_MAX_BSS_NUM)) {
        PRINTM(MERROR, "Unsupported number of BSS %d\n", intf_num);
        ret = MLAN_STATUS_FAILURE;
        goto done;
    } else {
        /* Create new table */
        if (!
            (bss_tbl =
             (mlan_bss_attr *) kmalloc(sizeof(mlan_bss_attr) * intf_num,
                                       GFP_KERNEL))) {
            PRINTM(MERROR, "Could not create BSS attribute table\n");
            ret = MLAN_STATUS_FAILURE;
            goto done;
        }
    }

    /* Populate BSS attribute table */
#ifdef STA_SUPPORT
    if (drv_mode_local & DRV_MODE_STA) {
        for (j = 0; j < max_sta_bss; j++) {
            if (i >= intf_num)
                break;
            bss_tbl[i].bss_type = MLAN_BSS_TYPE_STA;
            bss_tbl[i].frame_type = MLAN_DATA_FRAME_TYPE_ETH_II;
            bss_tbl[i].active = MTRUE;
            bss_tbl[i].bss_priority = 0;
            bss_tbl[i].bss_num = j;
            i++;
        }
    }
#endif /* STA_SUPPORT */

#ifdef UAP_SUPPORT
    if (drv_mode_local & DRV_MODE_UAP) {
        for (j = 0; j < max_uap_bss; j++) {
            if (i >= intf_num)
                break;
            bss_tbl[i].bss_type = MLAN_BSS_TYPE_UAP;
            bss_tbl[i].frame_type = MLAN_DATA_FRAME_TYPE_ETH_II;
            bss_tbl[i].active = MTRUE;
            bss_tbl[i].bss_priority = 0;
            bss_tbl[i].bss_num = j;
            i++;
        }
    }
#endif /* UAP_SUPPORT */

#if defined(WFD_SUPPORT)
    if (drv_mode_local & DRV_MODE_WFD) {
        for (j = 0; j < max_wfd_bss; j++) {
            if (i >= intf_num)
                break;
            bss_tbl[i].bss_type = MLAN_BSS_TYPE_WFD;
            bss_tbl[i].frame_type = MLAN_DATA_FRAME_TYPE_ETH_II;
            bss_tbl[i].active = MTRUE;
            bss_tbl[i].bss_priority = 0;
            bss_tbl[i].bss_num = j;
            i++;
        }
    }
#endif /* WFD_SUPPORT && V14_FEATURE */

    /* Clear existing table, if any */
    if (handle->drv_mode.bss_attr != NULL) {
        kfree(handle->drv_mode.bss_attr);
        handle->drv_mode.bss_attr = NULL;
    }

    /* Create moal_drv_mode entry */
    handle->drv_mode.drv_mode = drv_mode;
    handle->drv_mode.intf_num = intf_num;
    handle->drv_mode.bss_attr = bss_tbl;
    if (fw_name) {
        handle->drv_mode.fw_name = fw_name;
    } else {
#if defined(UAP_SUPPORT) && defined(STA_SUPPORT)
        handle->drv_mode.fw_name = DEFAULT_AP_STA_FW_NAME;
#else
#ifdef UAP_SUPPORT
        handle->drv_mode.fw_name = DEFAULT_AP_FW_NAME;
#else
        handle->drv_mode.fw_name = DEFAULT_FW_NAME;
#endif /* UAP_SUPPORT */
#endif /* UAP_SUPPORT && STA_SUPPORT */
    }

  done:
    if (ret == MLAN_STATUS_FAILURE) {
        if (bss_tbl != NULL) {
            kfree(bss_tbl);
            bss_tbl = NULL;
        }
    }
    LEAVE();
    return ret;
}

/** 
 *  @brief This function initializes software
 *  
 *  @param handle A pointer to moal_handle structure
 *
 *  @return 	   MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static mlan_status
woal_init_sw(moal_handle * handle)
{
    mlan_status ret = MLAN_STATUS_SUCCESS;
    unsigned int i;
    mlan_device device;
    t_void *pmlan;

    ENTER();

    /* Initialize moal_handle structure */
    handle->hardware_status = HardwareStatusInitializing;
    handle->main_state = MOAL_STATE_IDLE;

    if (wlan_update_drv_tbl(handle, drv_mode) != MLAN_STATUS_SUCCESS) {
        PRINTM(MERROR, "Could not update driver mode table\n");
        LEAVE();
        return MLAN_STATUS_FAILURE;
    }

    /* PnP and power profile */
    handle->surprise_removed = MFALSE;
    init_waitqueue_head(&handle->init_wait_q);

#if defined(SDIO_SUSPEND_RESUME)
    handle->is_suspended = MFALSE;
    handle->hs_activated = MFALSE;
    handle->suspend_fail = MFALSE;
#ifdef SDIO_SUSPEND_RESUME
    handle->suspend_notify_req = MFALSE;
#endif
    handle->hs_skip_count = 0;
    handle->hs_force_count = 0;
    handle->cmd52_func = 0;
    handle->cmd52_reg = 0;
    handle->cmd52_val = 0;
    init_waitqueue_head(&handle->hs_activate_wait_q);
#endif

    /* Initialize measurement wait queue */
    handle->meas_wait_q_woken = MFALSE;
    init_waitqueue_head(&handle->meas_wait_q);

#ifdef REASSOCIATION
    MOAL_INIT_SEMAPHORE(&handle->reassoc_sem);

#if (WIRELESS_EXT >= 18)
    handle->reassoc_on = MFALSE;
#else
    handle->reassoc_on = MTRUE;
#endif

    /* Initialize the timer for the reassociation */
    woal_initialize_timer(&handle->reassoc_timer,
                          woal_reassoc_timer_func, handle);

    handle->is_reassoc_timer_set = MFALSE;
#endif /* REASSOCIATION */

    /* Register to MLAN */
    memset(&device, 0, sizeof(mlan_device));
    device.pmoal_handle = handle;

#ifdef MFG_CMD_SUPPORT
    device.mfg_mode = (t_u32) mfg_mode;
#endif
    device.int_mode = (t_u32) intmode;
    device.gpio_pin = (t_u32) gpiopin;
    device.auto_ds = (t_u32) auto_ds;
    device.ps_mode = (t_u32) ps_mode;
    device.max_tx_buf = (t_u32) max_tx_buf;
#if defined(STA_SUPPORT)
    device.cfg_11d = (t_u32) cfg_11d;
#endif
    device.fw_crc_check = (t_u32) fw_crc_check;
#ifdef SDIO_MULTI_PORT_TX_AGGR
#ifdef MMC_QUIRK_BLKSZ_FOR_BYTE_MODE
    device.mpa_tx_cfg = MLAN_INIT_PARA_ENABLED;
#else
    device.mpa_tx_cfg = MLAN_INIT_PARA_DISABLED;
#endif
#endif
#ifdef SDIO_MULTI_PORT_RX_AGGR
#ifdef MMC_QUIRK_BLKSZ_FOR_BYTE_MODE
    device.mpa_rx_cfg = MLAN_INIT_PARA_ENABLED;
#else
    device.mpa_rx_cfg = MLAN_INIT_PARA_DISABLED;
#endif
#endif

    for (i = 0; i < handle->drv_mode.intf_num; i++) {
        device.bss_attr[i].bss_type = handle->drv_mode.bss_attr[i].bss_type;
        device.bss_attr[i].frame_type = handle->drv_mode.bss_attr[i].frame_type;
        device.bss_attr[i].active = handle->drv_mode.bss_attr[i].active;
        device.bss_attr[i].bss_priority =
            handle->drv_mode.bss_attr[i].bss_priority;
        device.bss_attr[i].bss_num = handle->drv_mode.bss_attr[i].bss_num;
    }
    memcpy(&device.callbacks, &woal_callbacks, sizeof(mlan_callbacks));

    sdio_claim_host(((struct sdio_mmc_card *) handle->card)->func);
    if (MLAN_STATUS_SUCCESS == mlan_register(&device, &pmlan))
        handle->pmlan_adapter = pmlan;
    else
        ret = MLAN_STATUS_FAILURE;
    sdio_release_host(((struct sdio_mmc_card *) handle->card)->func);

    LEAVE();
    return ret;
}

/** 
 *  @brief This function frees the structure of moal_handle
 *    
 *  @param handle   A pointer to moal_handle structure
 *
 *  @return 	    N/A
 */
static void
woal_free_moal_handle(moal_handle * handle)
{
    ENTER();
    if (!handle) {
        PRINTM(MERROR, "The handle is NULL\n");
        LEAVE();
        return;
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
    if ((handle->nl_sk) && ((handle->nl_sk)->sk_socket)) {
        sock_release((handle->nl_sk)->sk_socket);
        handle->nl_sk = NULL;
    }
#else
    netlink_kernel_release(handle->nl_sk);
#endif

    if (handle->pmlan_adapter)
        mlan_unregister(handle->pmlan_adapter);

    /* Free BSS attribute table */
    if (handle->drv_mode.bss_attr != NULL) {
        kfree(handle->drv_mode.bss_attr);
        handle->drv_mode.bss_attr = NULL;
    }
    PRINTM(MINFO, "Free Adapter\n");
    if (handle->malloc_count || handle->lock_count || handle->mbufalloc_count) {
        PRINTM(MERROR,
               "mlan has memory leak: malloc_count=%lu lock_count=%lu mbufalloc_count=%lu\n",
               handle->malloc_count, handle->lock_count,
               handle->mbufalloc_count);
    }
    /* Free the moal handle itself */
    kfree(handle);
    LEAVE();
}

/**
 *    @brief WOAL parse Calibration Data From ASCII format to binary
 *   
 *    @param src          Source data
 *    @param len          Source data length
 *    @param dst          Destination data
 *    @return             routnine status
 */
static t_u32
woal_parse_cal_cfg(t_u8 * src, t_size len, t_u8 * dst)
{
    t_u8 *ptr;
    t_u8 *dptr;

    ptr = src;
    dptr = dst;

    while (ptr - src < len) {
        while (*ptr && (isspace(*ptr) || *ptr == '\t')) {
            ptr++;
        }

        if (isxdigit(*ptr)) {
            *dptr++ = woal_atox(ptr);
            ptr += 2;
        } else {
            ptr++;
        }
    }

    return (dptr - dst);
}

/**
 *    @brief WOAL get one line data from ASCII format data
 *   
 *    @param data         Source data
 *    @param size         Source data length
 *    @param line_pos     Destination data
 *    @return             routnine status
 */
static t_size
parse_cfg_get_line(t_u8 * data, t_size size, t_u8 * line_pos)
{
    t_u8 *src, *dest;
    static t_s32 pos = 0;

    if (pos >= size) {          /* reach the end */
        pos = 0;                /* Reset position for rfkill */
        return -1;
    }
    memset(line_pos, 0, MAX_LINE_LEN);
    src = data + pos;
    dest = line_pos;

    while (*src != '\x0A' && *src != '\0') {
        if (*src != ' ' && *src != '\t')        /* parse space */
            *dest++ = *src++;
        else
            src++;
        pos++;
    }
    /* parse new line */
    pos++;
    *dest = '\0';
    return strlen(line_pos);
}

/** 
 *  @brief Process register access request
 *  @param type_string     String format Register type
 *  @param offset_string   String format Register offset
 *  @param value_string    String format Pointer to value
 *  @return                MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static t_u32
woal_process_regrdwr(moal_handle * handle, t_u8 * type_string,
                     t_u8 * offset_string, t_u8 * value_string)
{
    mlan_status ret = MLAN_STATUS_FAILURE;
    int type, offset, value;
    pmlan_ioctl_req ioctl_req = NULL;
    mlan_ds_reg_mem *reg = NULL;

    ENTER();

    /* Alloc ioctl_req */
    ioctl_req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_reg_mem));

    if (ioctl_req == NULL) {
        PRINTM(MERROR, "Can't alloc memory\n");
        goto done;
    }

    if (MLAN_STATUS_SUCCESS != woal_atoi(&type, type_string)) {
        goto done;
    }
    if (MLAN_STATUS_SUCCESS != woal_atoi(&offset, offset_string)) {
        goto done;
    }
    if (MLAN_STATUS_SUCCESS != woal_atoi(&value, value_string)) {
        goto done;
    }

    ioctl_req->req_id = MLAN_IOCTL_REG_MEM;
    ioctl_req->action = MLAN_ACT_SET;

    reg = (mlan_ds_reg_mem *) ioctl_req->pbuf;
    reg->sub_command = MLAN_OID_REG_RW;
    if (type < 5) {
        reg->param.reg_rw.type = type;
    } else {
        PRINTM(MERROR, "Unsupported Type\n");
        goto done;
    }
    reg->param.reg_rw.offset = offset;
    reg->param.reg_rw.value = value;

    /* request ioctl for STA */
    if (MLAN_STATUS_SUCCESS !=
        woal_request_ioctl(handle->priv[0], ioctl_req, MOAL_IOCTL_WAIT)) {
        goto done;
    }
    PRINTM(MINFO, "Register type: %d, offset: 0x%x, value: 0x%x\n", type,
           offset, value);
    ret = MLAN_STATUS_SUCCESS;

  done:
    if (ioctl_req)
        kfree(ioctl_req);
    LEAVE();
    return ret;
}

/**
 *    @brief WOAL parse ASCII format data to MAC address
 *   
 *    @param handle       MOAL handle
 *    @param data         Source data
 *    @param size         data length
 *    @return             MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static t_u32
woal_process_init_cfg(moal_handle * handle, t_u8 * data, t_size size)
{
    mlan_status ret = MLAN_STATUS_FAILURE;
    t_u8 *pos;
    t_u8 *intf_s, *intf_e;
    t_u8 s[MAX_LINE_LEN];       /* 1 line data */
    t_size line_len;
    t_u8 index = 0;
    t_u32 i;
    t_u8 bss_mac_addr[MAX_MAC_ADDR_LEN];
    t_u8 bss_mac_name[MAX_PARAM_LEN];
    t_u8 type[MAX_PARAM_LEN];
    t_u8 offset[MAX_PARAM_LEN];
    t_u8 value[MAX_PARAM_LEN];

    ENTER();

    while ((line_len = parse_cfg_get_line(data, size, s)) != -1) {

        pos = s;
        while (*pos == ' ' || *pos == '\t')
            pos++;

        if (*pos == '#' || (*pos == '\r' && *(pos + 1) == '\n') ||
            *pos == '\n' || *pos == '\0')
            continue;           /* Needn't process this line */

        /* Process MAC addr */
        if (strncmp(pos, "mac_addr", 8) == 0) {
            intf_s = strchr(pos, '=');
            if (intf_s != NULL)
                intf_e = strchr(intf_s, ':');
            else
                intf_e = NULL;
            if (intf_s != NULL && intf_e != NULL) {
                strncpy(bss_mac_addr, intf_e + 1, MAX_MAC_ADDR_LEN - 1);
                bss_mac_addr[MAX_MAC_ADDR_LEN - 1] = '\0';
                if ((intf_e - intf_s) > MAX_PARAM_LEN) {
                    PRINTM(MERROR, "Too long interface name %d\n", __LINE__);
                    goto done;
                }
                strncpy(bss_mac_name, intf_s + 1, intf_e - intf_s - 1);
                bss_mac_name[intf_e - intf_s - 1] = '\0';
                for (i = 0; i < handle->priv_num; i++) {
                    if (strcmp(bss_mac_name, handle->priv[i]->netdev->name) ==
                        0) {
                        memset(handle->priv[i]->current_addr, 0, ETH_ALEN);
                        PRINTM(MINFO, "Interface name: %s mac: %s\n",
                               bss_mac_name, bss_mac_addr);
                        woal_mac2u8(handle->priv[i]->current_addr,
                                    bss_mac_addr);
                        /* Set WLAN MAC addresses */
                        if (MLAN_STATUS_SUCCESS !=
                            woal_request_set_mac_address(handle->priv[i])) {
                            PRINTM(MERROR, "Set MAC address failed\n");
                            goto done;
                        }
                        memcpy(handle->priv[i]->netdev->dev_addr,
                               handle->priv[i]->current_addr, ETH_ALEN);
                        index++;        /* Mark found one interface matching */
                    }
                }
            } else {
                PRINTM(MERROR, "Wrong config file format %d\n", __LINE__);
                goto done;
            }
        }
        /* Process REG value */
        else if (strncmp(pos, "wlan_reg", 8) == 0) {
            intf_s = strchr(pos, '=');
            if (intf_s != NULL)
                intf_e = strchr(intf_s, ',');
            else
                intf_e = NULL;
            if (intf_s != NULL && intf_e != NULL) {
                /* Copy type */
                strncpy(type, intf_s + 1, 1);
                type[1] = '\0';
            } else {
                PRINTM(MERROR, "Wrong config file format %d\n", __LINE__);
                goto done;
            }
            intf_s = intf_e + 1;
            if (intf_s != NULL)
                intf_e = strchr(intf_s, ',');
            else
                intf_e = NULL;
            if (intf_s != NULL && intf_e != NULL) {
                if ((intf_e - intf_s) >= MAX_PARAM_LEN) {
                    PRINTM(MERROR, "Regsier offset is too long %d\n", __LINE__);
                    goto done;
                }
                /* Copy offset */
                strncpy(offset, intf_s, intf_e - intf_s);
                offset[intf_e - intf_s] = '\0';
            } else {
                PRINTM(MERROR, "Wrong config file format %d\n", __LINE__);
                goto done;
            }
            intf_s = intf_e + 1;
            if (intf_s != NULL) {
                if ((strlen(intf_s) >= MAX_PARAM_LEN)) {
                    PRINTM(MERROR, "Regsier value is too long %d\n", __LINE__);
                    goto done;
                }
                /* Copy value */
                strcpy(value, intf_s);
            } else {
                PRINTM(MERROR, "Wrong config file format %d\n", __LINE__);
                goto done;
            }

            if (MLAN_STATUS_SUCCESS !=
                woal_process_regrdwr(handle, type, offset, value)) {
                PRINTM(MERROR, "Access Reg failed\n");
                goto done;
            }
            PRINTM(MINFO, "Reg type: %s, offset: %s, value: %s\n", type, offset,
                   value);
        }
    }

    if (index == 0) {
        PRINTM(MINFO, "Can't find any matching MAC Address");
    }
    ret = MLAN_STATUS_SUCCESS;

  done:
    LEAVE();
    return ret;
}

/**
 *    @brief WOAL set user defined init data and param
 *   
 *    @param handle       MOAL handle structure
 *    @return             MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static t_u32
woal_set_user_init_data(moal_handle * handle)
{
    mlan_status ret = MLAN_STATUS_FAILURE;
    t_u8 *cfg_data = NULL;
    t_size len;

    ENTER();

    if ((request_firmware(&handle->user_data, init_cfg, handle->hotplug_device))
        < 0) {
        PRINTM(MERROR, "Init config file request_firmware() failed\n");
        goto done;
    }

    if (handle->user_data) {
        cfg_data = (t_u8 *) (handle->user_data)->data;
        len = (handle->user_data)->size;
        if (MLAN_STATUS_SUCCESS != woal_process_init_cfg(handle, cfg_data, len)) {
            PRINTM(MERROR, "Can't process init config file\n");
            goto done;
        }
        ret = MLAN_STATUS_SUCCESS;
    }

  done:
    if (handle->user_data)
        release_firmware(handle->user_data);

    LEAVE();
    return ret;
}

/**
 *    @brief WOAL Download calibration data to Firmware by using hostcommand
 *   
 *    @param handle       MOAL handle structure
 *    @return             MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static t_u32
woal_dnld_cal_data(moal_handle * handle)
{
    mlan_status ret = MLAN_STATUS_FAILURE;
    t_u8 *cal_data;
    t_u8 *hostcmd_block = NULL;
    t_size cal_data_size;
    t_u32 cmd_data_offset;
    t_u32 len;
    mlan_ds_misc_cfg *misc = NULL;
    mlan_ioctl_req *ioctl_req = NULL;
    HostCmd_DS_GEN *hostcmd_header;
    HostCmd_DS_802_11_CFG_DATA *pcfg_data;

    ENTER();

    if ((request_firmware
         (&handle->user_data, cal_data_cfg, handle->hotplug_device)) < 0) {
        PRINTM(MERROR, "Calibration config request_firmware() failed\n");
        goto done;
    }

    if (handle->user_data) {
        cal_data = (t_u8 *) (handle->user_data)->data;
        cal_data_size = (handle->user_data)->size;
        /* alloc ioctl request struct */
        ioctl_req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
        if (ioctl_req == NULL) {
            PRINTM(MERROR, "Can't alloc memory\n");
            goto done;
        }

        /* alloc host command data buffer */
        hostcmd_block = (t_u8 *) kzalloc(MLAN_SIZE_OF_CMD_BUFFER, GFP_KERNEL);
        if (hostcmd_block == NULL) {
            PRINTM(MERROR, "Can't alloc mem\n");
            goto done;
        }

        cmd_data_offset = CFG_DATA_HEADER_LEN + sizeof(HostCmd_DS_GEN);
        /* CAL data sanity test before encapsulate into host command */
        if ((cal_data != NULL) && (cal_data_size > 0)) {
            len =
                woal_parse_cal_cfg(cal_data, cal_data_size,
                                   hostcmd_block + cmd_data_offset);
        } else {
            PRINTM(MERROR, "CAL data sanity test failed\n");
            goto done;
        }

        /* Fill CMD */
        ioctl_req->req_id = MLAN_IOCTL_MISC_CFG;

        misc = (mlan_ds_misc_cfg *) ioctl_req->pbuf;
        hostcmd_header = (HostCmd_DS_GEN *) hostcmd_block;
        hostcmd_header->command = HostCmd_CMD_CFG_DATA;

        pcfg_data =
            (HostCmd_DS_802_11_CFG_DATA *) (hostcmd_block +
                                            sizeof(HostCmd_DS_GEN));
        pcfg_data->action = HostCmd_ACT_GEN_SET;
        pcfg_data->type = 2;    /* CAL data type */
        /* PARSE DATA and Set DATALEN */
        pcfg_data->data_len = len;

        hostcmd_header->size =
            woal_le16_to_cpu(pcfg_data->data_len + cmd_data_offset);
        pcfg_data->data_len = woal_le16_to_cpu(pcfg_data->data_len);
        pcfg_data->type = woal_le16_to_cpu(pcfg_data->type);
        pcfg_data->action = woal_le16_to_cpu(pcfg_data->action);

        misc->param.hostcmd.len = hostcmd_header->size;
        misc->sub_command = MLAN_OID_MISC_HOST_CMD;
        memcpy(misc->param.hostcmd.cmd, hostcmd_block, misc->param.hostcmd.len);
        PRINTM(MINFO, "Host command len = %lu\n", (misc->param.hostcmd.len));

        /* request ioctl for STA */
        if (MLAN_STATUS_SUCCESS !=
            woal_request_ioctl(handle->priv[0], ioctl_req, MOAL_IOCTL_WAIT)) {
            PRINTM(MERROR, "IOCTL request failed.\n");
            goto done;
        }
        ret = MLAN_STATUS_SUCCESS;
    }

  done:
    if (hostcmd_block)
        kfree(hostcmd_block);
    if (ioctl_req)
        kfree(ioctl_req);
    if (handle->user_data)
        release_firmware(handle->user_data);

    LEAVE();
    return ret;
}

/** 
 * @brief Add interfaces DPC
 *  
 * @param handle    A pointer to moal_handle structure
 *
 * @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static mlan_status
woal_add_card_dpc(moal_handle * handle)
{
    mlan_status ret = MLAN_STATUS_SUCCESS;
    int i;

    ENTER();

#ifdef CONFIG_PROC_FS
    /* Initialize proc fs */
    woal_proc_init(handle);
#endif /* CONFIG_PROC_FS */

    /* Add interfaces */
    for (i = 0; i < handle->drv_mode.intf_num; i++) {
        if (!woal_add_interface
            (handle, handle->priv_num, handle->drv_mode.bss_attr[i].bss_type)) {
            ret = MLAN_STATUS_FAILURE;
            goto err;
        }
        handle->priv_num++;
    }
    if (init_cfg) {
        if (MLAN_STATUS_SUCCESS != woal_set_user_init_data(handle)) {
            PRINTM(MFATAL, "Set user init data and param failed\n");
            ret = MLAN_STATUS_FAILURE;
            goto err;
        }
    }

    if (cal_data_cfg) {
        if (MLAN_STATUS_SUCCESS != woal_dnld_cal_data(handle)) {
            PRINTM(MFATAL, "Download CAL data failed\n");
            ret = MLAN_STATUS_FAILURE;
            goto err;
        }
    }

  err:
    if (ret != MLAN_STATUS_SUCCESS) {
        PRINTM(MERROR, "Failed to add interface\n");
        for (i = 0; i < handle->priv_num; i++)
            woal_remove_interface(handle, i);
        handle->priv_num = 0;
#ifdef CONFIG_PROC_FS
        woal_proc_exit(handle);
#endif
    }

    LEAVE();
    return ret;
}

/** 
 * @brief Download and Initialize firmware DPC
 *    
 * @param handle    A pointer to moal_handle structure
 *
 * @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static mlan_status
woal_init_fw_dpc(moal_handle * handle)
{
    mlan_status ret = MLAN_STATUS_SUCCESS;
    mlan_fw_image fw;

    ENTER();

    if (handle->firmware) {
        memset(&fw, 0, sizeof(mlan_fw_image));
        fw.pfw_buf = (t_u8 *) handle->firmware->data;
        fw.fw_len = handle->firmware->size;
        sdio_claim_host(((struct sdio_mmc_card *) handle->card)->func);
        ret = mlan_dnld_fw(handle->pmlan_adapter, &fw);
        sdio_release_host(((struct sdio_mmc_card *) handle->card)->func);
        if (ret == MLAN_STATUS_FAILURE) {
            PRINTM(MERROR, "WLAN: Download FW with nowwait: %d\n",
                   req_fw_nowait);
            goto done;
        }
        PRINTM(MMSG, "WLAN FW is active\n");
    }

    handle->hardware_status = HardwareStatusFwReady;
    if (ret != MLAN_STATUS_SUCCESS)
        goto done;

    handle->init_wait_q_woken = MFALSE;
    sdio_claim_host(((struct sdio_mmc_card *) handle->card)->func);
    ret = mlan_init_fw(handle->pmlan_adapter);
    sdio_release_host(((struct sdio_mmc_card *) handle->card)->func);
    if (ret == MLAN_STATUS_FAILURE) {
        goto done;
    } else if (ret == MLAN_STATUS_SUCCESS) {
        handle->hardware_status = HardwareStatusReady;
        goto done;
    }
    /* Wait for mlan_init to complete */
    wait_event_interruptible(handle->init_wait_q, handle->init_wait_q_woken);
    if (handle->hardware_status != HardwareStatusReady) {
        ret = MLAN_STATUS_FAILURE;
        goto done;
    }
    ret = MLAN_STATUS_SUCCESS;
  done:
    LEAVE();
    return ret;
}

/** 
 * @brief Request firmware DPC
 * 
 * @param handle    A pointer to moal_handle structure
 * @param firmware  A pointer to firmware image
 *
 * @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static mlan_status
woal_request_fw_dpc(moal_handle * handle, const struct firmware *firmware)
{
    mlan_status ret = MLAN_STATUS_SUCCESS;
    struct timeval tstamp;

    ENTER();

    if (!firmware) {
        do_gettimeofday(&tstamp);
        if (tstamp.tv_sec > (handle->req_fw_time.tv_sec + REQUEST_FW_TIMEOUT)) {
            PRINTM(MERROR, "No firmware image found. Skipping download\n");
            ret = MLAN_STATUS_FAILURE;
            goto done;
        }
        PRINTM(MERROR, "request_firmware_nowait failed for %s. Retrying..\n",
               handle->drv_mode.fw_name);
        woal_sched_timeout(MOAL_TIMER_1S);
        woal_request_fw(handle);
        LEAVE();
        return ret;
    }
    handle->firmware = firmware;

    if ((ret = woal_init_fw_dpc(handle)))
        goto done;
    if ((ret = woal_add_card_dpc(handle)))
        goto done;

  done:
    /* We should hold the semaphore until callback finishes execution */
    MOAL_REL_SEMAPHORE(&AddRemoveCardSem);
    LEAVE();
    return ret;
}

/** 
 * @brief Request firmware callback
 *        This function is invoked by request_firmware_nowait system call
 * 
 * @param firmware  A pointer to firmware image
 * @param context   A pointer to moal_handle structure
 *
 * @return        N/A
 */
static void
woal_request_fw_callback(const struct firmware *firmware, void *context)
{
    ENTER();
    woal_request_fw_dpc((moal_handle *) context, firmware);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32)
    if (firmware)
        release_firmware(firmware);
#endif
    LEAVE();
    return;
}

/** 
 * @brief   Download firmware using helper
 * 
 * @param handle  A pointer to moal_handle structure
 *
 * @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
woal_request_fw(moal_handle * handle)
{
    mlan_status ret = MLAN_STATUS_SUCCESS;
    int err;

    ENTER();

    if (req_fw_nowait) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32)
        if ((err = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
                                           handle->drv_mode.fw_name,
                                           handle->hotplug_device, GFP_KERNEL,
                                           handle,
                                           woal_request_fw_callback)) < 0) {
#else
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
        if ((err = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
                                           handle->drv_mode.fw_name,
                                           handle->hotplug_device, handle,
                                           woal_request_fw_callback)) < 0) {
#else
        if ((err = request_firmware_nowait(THIS_MODULE,
                                           handle->drv_mode.fw_name,
                                           handle->hotplug_device, handle,
                                           woal_request_fw_callback)) < 0) {
#endif
#endif
            PRINTM(MFATAL,
                   "WLAN: request_firmware_nowait() failed, error code = %d\n",
                   err);
            ret = MLAN_STATUS_FAILURE;
        }
    } else {
        if ((err =
             request_firmware(&handle->firmware, handle->drv_mode.fw_name,
                              handle->hotplug_device)) < 0) {
            PRINTM(MFATAL, "WLAN: request_firmware() failed, error code = %d\n",
                   err);
            ret = MLAN_STATUS_FAILURE;
        } else {
            ret = woal_request_fw_dpc(handle, handle->firmware);
            release_firmware(handle->firmware);
        }
    }

    LEAVE();
    return ret;
}

/** 
 *  @brief This function initializes firmware
 *  
 *  @param handle  A pointer to moal_handle structure
 *
 *  @return 	   MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static mlan_status
woal_init_fw(moal_handle * handle)
{
    mlan_status ret = MLAN_STATUS_SUCCESS;

    ENTER();

    do_gettimeofday(&handle->req_fw_time);
    if ((ret = woal_request_fw(handle)) < 0) {
        PRINTM(MFATAL, "woal_request_fw failed\n");
        ret = MLAN_STATUS_FAILURE;
        goto done;
    }
  done:
    LEAVE();
    return ret;
}

/** 
 *  @brief This function will fill in the mlan_buffer
 *  
 *  @param pmbuf   A pointer to mlan_buffer
 *  @param skb     A pointer to struct sk_buff 
 *
 *  @return        N/A
 */
static void
woal_fill_mlan_buffer(moal_private * priv,
                      mlan_buffer * pmbuf, struct sk_buff *skb)
{
    struct timeval tstamp;
    struct ethhdr *eth;
    t_u8 tid;

    ENTER();

    eth = (struct ethhdr *) skb->data;

    switch (eth->h_proto) {

    case __constant_htons(ETH_P_IP):
        tid = (IPTOS_PREC(SKB_TOS(skb)) >> IPTOS_OFFSET);
        PRINTM(MDATA, "packet type ETH_P_IP: %04x, tid=%#x prio=%#x\n",
               eth->h_proto, tid, skb->priority);
        break;

    case __constant_htons(ETH_P_ARP):
        PRINTM(MDATA, "ARP packet %04x\n", eth->h_proto);
    default:
        tid = 0;
        break;
    }

    skb->priority = tid;

    /* Record the current time the packet was queued; used to determine the
       amount of time the packet was queued in the driver before it was sent to 
       the firmware.  The delay is then sent along with the packet to the
       firmware for aggregate delay calculation for stats and MSDU lifetime
       expiry. */
    do_gettimeofday(&tstamp);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
    skb->tstamp = timeval_to_ktime(tstamp);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
    skb_set_timestamp(skb, &tstamp);
#else
    memcpy(&skb->stamp, &tstamp, sizeof(skb->stamp));
#endif

    pmbuf->pdesc = skb;
    pmbuf->pbuf = skb->head + sizeof(mlan_buffer);
    pmbuf->data_offset = skb->data - (skb->head + sizeof(mlan_buffer));
    pmbuf->data_len = skb->len;
    pmbuf->priority = skb->priority;
    pmbuf->in_ts_sec = (t_u32) tstamp.tv_sec;
    pmbuf->in_ts_usec = (t_u32) tstamp.tv_usec;

    LEAVE();
    return;
}

#ifdef STA_SUPPORT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
/** Network device handlers */
const struct net_device_ops woal_netdev_ops = {
    .ndo_open = woal_open,
    .ndo_start_xmit = woal_hard_start_xmit,
    .ndo_stop = woal_close,
    .ndo_do_ioctl = woal_do_ioctl,
    .ndo_set_mac_address = woal_set_mac_address,
    .ndo_tx_timeout = woal_tx_timeout,
    .ndo_get_stats = woal_get_stats,
    .ndo_set_multicast_list = woal_set_multicast_list,
};
#endif

/**
 *  @brief This function initializes the private structure 
 *  		and dev structure for station mode
 *  
 *  @param dev     A pointer to net_device structure
 *  @param priv    A pointer to moal_private structure
 *
 *  @return 	   MLAN_STATUS_SUCCESS 
 */
static mlan_status
woal_init_sta_dev(struct net_device *dev, moal_private * priv)
{
    ENTER();

    /* Setup the OS Interface to our functions */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
    dev->open = woal_open;
    dev->hard_start_xmit = woal_hard_start_xmit;
    dev->stop = woal_close;
    dev->do_ioctl = woal_do_ioctl;
    dev->set_mac_address = woal_set_mac_address;
    dev->tx_timeout = woal_tx_timeout;
    dev->get_stats = woal_get_stats;
    dev->set_multicast_list = woal_set_multicast_list;
#else
    dev->netdev_ops = &woal_netdev_ops;
#endif
    dev->watchdog_timeo = MRVDRV_DEFAULT_WATCHDOG_TIMEOUT;
    dev->hard_header_len += MLAN_MIN_DATA_HEADER_LEN + sizeof(mlan_buffer);
#ifdef  WIRELESS_EXT
#if WIRELESS_EXT < 21
    dev->get_wireless_stats = woal_get_wireless_stats;
#endif
    dev->wireless_handlers = (struct iw_handler_def *) &woal_handler_def;
#endif
    dev->flags |= IFF_BROADCAST | IFF_MULTICAST;

    /* Initialize private structure */
    init_waitqueue_head(&priv->ioctl_wait_q);
    init_waitqueue_head(&priv->cmd_wait_q);
    init_waitqueue_head(&priv->proc_wait_q);
    init_waitqueue_head(&priv->w_stats_wait_q);

    LEAVE();
    return MLAN_STATUS_SUCCESS;
}
#endif /* STA_SUPPORT */

#ifdef UAP_SUPPORT
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
/** Network device handlers */
const struct net_device_ops woal_uap_netdev_ops = {
    .ndo_open = woal_open,
    .ndo_start_xmit = woal_hard_start_xmit,
    .ndo_stop = woal_close,
    .ndo_do_ioctl = woal_uap_do_ioctl,
    .ndo_set_mac_address = woal_set_mac_address,
    .ndo_tx_timeout = woal_tx_timeout,
    .ndo_get_stats = woal_get_stats,
    .ndo_set_multicast_list = woal_uap_set_multicast_list,
};
#endif

/**
 *  @brief This function initializes the private structure 
 *  		and dev structure for uap mode
 *  
 *  @param dev     A pointer to net_device structure
 *  @param priv    A pointer to moal_private structure
 *
 *  @return 	   MLAN_STATUS_SUCCESS
 */
static mlan_status
woal_init_uap_dev(struct net_device *dev, moal_private * priv)
{
    mlan_status status = MLAN_STATUS_SUCCESS;

    ENTER();

    /* Setup the OS Interface to our functions */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
    dev->open = woal_open;
    dev->hard_start_xmit = woal_hard_start_xmit;
    dev->stop = woal_close;
    dev->set_mac_address = woal_set_mac_address;
    dev->tx_timeout = woal_tx_timeout;
    dev->get_stats = woal_get_stats;
    dev->do_ioctl = woal_uap_do_ioctl;
    dev->set_multicast_list = woal_uap_set_multicast_list;
#else
    dev->netdev_ops = &woal_uap_netdev_ops;
#endif
    dev->watchdog_timeo = MRVDRV_DEFAULT_UAP_WATCHDOG_TIMEOUT;
    dev->hard_header_len += MLAN_MIN_DATA_HEADER_LEN + sizeof(mlan_buffer);
#ifdef  WIRELESS_EXT
#if WIRELESS_EXT < 21
    dev->get_wireless_stats = woal_get_uap_wireless_stats;
#endif
    dev->wireless_handlers = (struct iw_handler_def *) &woal_uap_handler_def;
#endif /* WIRELESS_EXT */
    dev->flags |= IFF_BROADCAST | IFF_MULTICAST;

    /* Initialize private structure */
    init_waitqueue_head(&priv->ioctl_wait_q);
    init_waitqueue_head(&priv->cmd_wait_q);
    init_waitqueue_head(&priv->proc_wait_q);
    init_waitqueue_head(&priv->w_stats_wait_q);

    LEAVE();
    return status;
}
#endif /* UAP_SUPPORT */

/**
 * @brief This function adds a new interface. It will
 * 		allocate, initialize and register the device.
 *  
 *  @param handle    A pointer to moal_handle structure
 *  @param bss_index BSS index number
 *  @param bss_type  BSS type
 *
 *  @return          A pointer to the new priv structure
 */
moal_private *
woal_add_interface(moal_handle * handle, t_u8 bss_index, t_u8 bss_type)
{
    struct net_device *dev = NULL;
    moal_private *priv = NULL;

    ENTER();

    /* Allocate an Ethernet device */
    if (!(dev = alloc_etherdev(sizeof(moal_private)))) {
        PRINTM(MFATAL, "Init virtual ethernet device failed\n");
        goto error;
    }
#ifdef STA_SUPPORT
    /* Allocate device name */
    if ((bss_type == MLAN_BSS_TYPE_STA) && (dev_alloc_name(dev, "mlan%d") < 0)) {
        PRINTM(MERROR, "Could not allocate mlan device name\n");
        goto error;
    }
#endif
#ifdef UAP_SUPPORT
    if ((bss_type == MLAN_BSS_TYPE_UAP) && (dev_alloc_name(dev, "uap%d") < 0)) {
        PRINTM(MERROR, "Could not allocate uap device name\n");
        goto error;
    }
#endif
#if defined(WFD_SUPPORT)
    if ((bss_type == MLAN_BSS_TYPE_WFD) && (dev_alloc_name(dev, "wfd%d") < 0)) {
        PRINTM(MERROR, "Could not allocate wfd device name\n");
        goto error;
    }
#endif
    priv = (moal_private *) netdev_priv(dev);
    /* Save the priv to handle */
    handle->priv[bss_index] = priv;

    /* Use the same handle structure */
    priv->phandle = handle;
    priv->netdev = dev;
    priv->bss_index = bss_index;
    priv->bss_type = bss_type;
    if (bss_type == MLAN_BSS_TYPE_STA)
        priv->bss_role = MLAN_BSS_ROLE_STA;
    else if (bss_type == MLAN_BSS_TYPE_UAP)
        priv->bss_role = MLAN_BSS_ROLE_UAP;
#if defined(WFD_SUPPORT)
    else if (bss_type == MLAN_BSS_TYPE_WFD)
        priv->bss_role = MLAN_BSS_ROLE_STA;
#endif

#ifdef STA_SUPPORT
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
    SET_MODULE_OWNER(dev);
#endif
#ifdef STA_SUPPORT
    if (bss_type == MLAN_BSS_TYPE_STA
#if defined(WFD_SUPPORT)
        || bss_type == MLAN_BSS_TYPE_WFD
#endif
        )
        woal_init_sta_dev(dev, priv);
#endif
#ifdef UAP_SUPPORT
    if (bss_type == MLAN_BSS_TYPE_UAP) {
        if (MLAN_STATUS_SUCCESS != woal_init_uap_dev(dev, priv))
            goto error;
    }
#endif

    /* Initialize priv structure */
    woal_init_priv(priv, MOAL_CMD_WAIT);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
    SET_NETDEV_DEV(dev, handle->hotplug_device);
#endif

    /* Register network device */
    if (register_netdev(dev)) {
        PRINTM(MERROR, "Cannot register virtual network device\n");
        goto error;
    }
    netif_carrier_off(dev);
    netif_stop_queue(dev);

    PRINTM(MINFO, "%s: Marvell 802.11 Adapter\n", dev->name);

    /* Set MAC address from the insmod command line */
    if (handle->set_mac_addr) {
        memset(priv->current_addr, 0, ETH_ALEN);
        memcpy(priv->current_addr, handle->mac_addr, ETH_ALEN);
        if (MLAN_STATUS_SUCCESS != woal_request_set_mac_address(priv)) {
            PRINTM(MERROR, "Set MAC address failed\n");
            goto error;
        }
        memcpy(dev->dev_addr, priv->current_addr, ETH_ALEN);
    }
#ifdef CONFIG_PROC_FS
    woal_create_proc_entry(priv);
#ifdef PROC_DEBUG
    woal_debug_entry(priv);
#endif /* PROC_DEBUG */
#endif /* CONFIG_PROC_FS */

    LEAVE();
    return priv;
  error:
    if (dev && dev->reg_state == NETREG_REGISTERED)
        unregister_netdev(dev);
    if (dev)
        free_netdev(dev);
    LEAVE();
    return NULL;
}

/** 
 *  @brief This function removes an interface.
 *  
 *  @param handle    A pointer to the moal_handle structure
 *  @param bss_index  BSS index number
 *
 *  @return        N/A
 */
void
woal_remove_interface(moal_handle * handle, t_u8 bss_index)
{
    struct net_device *dev = NULL;
    moal_private *priv = handle->priv[bss_index];
    union iwreq_data wrqu;

    ENTER();
    if (!priv)
        goto error;
    dev = priv->netdev;

    if (priv->media_connected == MTRUE) {
        priv->media_connected = MFALSE;
        if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA) {
            memset(wrqu.ap_addr.sa_data, 0x00, ETH_ALEN);
            wrqu.ap_addr.sa_family = ARPHRD_ETHER;
            wireless_send_event(priv->netdev, SIOCGIWAP, &wrqu, NULL);
        }
    }
#ifdef CONFIG_PROC_FS
#ifdef PROC_DEBUG
    /* Remove proc debug */
    woal_debug_remove(priv);
#endif /* PROC_DEBUG */
    woal_proc_remove(priv);
#endif /* CONFIG_PROC_FS */
    /* Last reference is our one */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
    PRINTM(MINFO, "refcnt = %d\n", atomic_read(&dev->refcnt));
#else
    PRINTM(MINFO, "refcnt = %d\n", netdev_refcnt_read(dev));
#endif

    PRINTM(MINFO, "netdev_finish_unregister: %s\n", dev->name);

    if (dev->reg_state == NETREG_REGISTERED)
        unregister_netdev(dev);

    /* Clear the priv in handle */
    priv->phandle->priv[priv->bss_index] = NULL;
    priv->phandle = NULL;
    priv->netdev = NULL;
    free_netdev(dev);
  error:
    LEAVE();
    return;
}

/** 
 *  @brief Send FW shutdown command to MLAN
 *   
 *  @param priv          A pointer to moal_private structure
 *  @param wait_option   Wait option
 *
 *  @return              MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
static mlan_status
woal_shutdown_fw(moal_private * priv, t_u8 wait_option)
{
    mlan_ioctl_req *req = NULL;
    mlan_ds_misc_cfg *misc = NULL;
    mlan_status status;

    ENTER();

    /* Allocate an IOCTL request buffer */
    req =
        (mlan_ioctl_req *) woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
    if (req == NULL) {
        status = MLAN_STATUS_FAILURE;
        goto done;
    }

    /* Fill request buffer */
    misc = (mlan_ds_misc_cfg *) req->pbuf;
    misc->sub_command = MLAN_OID_MISC_INIT_SHUTDOWN;
    misc->param.func_init_shutdown = MLAN_FUNC_SHUTDOWN;
    req->req_id = MLAN_IOCTL_MISC_CFG;
    req->action = MLAN_ACT_SET;

    /* Send IOCTL request to MLAN */
    status = woal_request_ioctl(priv, req, wait_option);

  done:
    if (req)
        kfree(req);
    LEAVE();
    return status;
}

/** 
 *  @brief Return hex value of a give character
 *
 *  @param chr	    Character to be converted
 *
 *  @return 	    The converted character if chr is a valid hex, else 0
 */
static int
woal_hexval(char chr)
{
    ENTER();

    if (chr >= '0' && chr <= '9')
        return chr - '0';
    if (chr >= 'A' && chr <= 'F')
        return chr - 'A' + 10;
    if (chr >= 'a' && chr <= 'f')
        return chr - 'a' + 10;

    LEAVE();
    return 0;
}

/**
 *  @brief This function cancel all works in the queue
 *  and destroy the main workqueue.
 *  
 *  @param handle    A pointer to moal_handle
 *
 *  @return        N/A
 */
static void
woal_terminate_workqueue(moal_handle * handle)
{
    ENTER();

    /* Terminate main workqueue */
    if (handle->workqueue) {
        flush_workqueue(handle->workqueue);
        destroy_workqueue(handle->workqueue);
        handle->workqueue = NULL;
    }

    LEAVE();
}

/********************************************************
		Global Functions
********************************************************/

/** 
 *  @brief This function opens the network device
 *  
 *  @param dev     A pointer to net_device structure
 *
 *  @return        0 --success, otherwise fail
 */
int
woal_open(struct net_device *dev)
{
    moal_private *priv = (moal_private *) netdev_priv(dev);
    t_u8 carrier_on = MFALSE;

    ENTER();

    if (!MODULE_GET) {
        LEAVE();
        return -EFAULT;
    }
#ifdef UAP_SUPPORT
    if ((GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP) && (priv->media_connected))
        carrier_on = MTRUE;
#endif
#ifdef STA_SUPPORT
    if ((GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA) &&
        (priv->media_connected || priv->is_adhoc_link_sensed))
        carrier_on = MTRUE;
#endif
    if (carrier_on == MTRUE) {
        if (!netif_carrier_ok(priv->netdev))
            netif_carrier_on(priv->netdev);
        if (netif_queue_stopped(priv->netdev))
            netif_wake_queue(priv->netdev);
    } else {
        if (netif_carrier_ok(priv->netdev))
            netif_carrier_off(priv->netdev);
    }
    woal_request_open(priv);

    LEAVE();
    return 0;
}

/** 
 *  @brief This function closes the network device
 *  
 *  @param dev     A pointer to net_device structure
 *
 *  @return        0
 */
int
woal_close(struct net_device *dev)
{
    moal_private *priv = (moal_private *) netdev_priv(dev);

    ENTER();

    woal_request_close(priv);
    MODULE_PUT;

    LEAVE();
    return 0;
}

/** 
 *  @brief This function sets the MAC address to firmware.
 *  
 *  @param dev     A pointer to mlan_private structure
 *  @param addr    MAC address to set
 *
 *  @return        0 --success, otherwise fail
 */
int
woal_set_mac_address(struct net_device *dev, void *addr)
{
    int ret = 0;
    moal_private *priv = (moal_private *) netdev_priv(dev);
    struct sockaddr *phw_addr = (struct sockaddr *) addr;
    t_u8 prev_addr[ETH_ALEN];

    ENTER();

    memcpy(prev_addr, priv->current_addr, ETH_ALEN);
    memset(priv->current_addr, 0, ETH_ALEN);
    /* dev->dev_addr is 6 bytes */
    HEXDUMP("dev->dev_addr:", dev->dev_addr, ETH_ALEN);

    HEXDUMP("addr:", (t_u8 *) phw_addr->sa_data, ETH_ALEN);
    memcpy(priv->current_addr, phw_addr->sa_data, ETH_ALEN);
    if (MLAN_STATUS_SUCCESS != woal_request_set_mac_address(priv)) {
        PRINTM(MERROR, "Set MAC address failed\n");
        /* For failure restore the MAC address */
        memcpy(priv->current_addr, prev_addr, ETH_ALEN);
        ret = -EFAULT;
        goto done;
    }
    HEXDUMP("priv->MacAddr:", priv->current_addr, ETH_ALEN);
    memcpy(dev->dev_addr, priv->current_addr, ETH_ALEN);
  done:
    LEAVE();
    return ret;
}

/** 
 *  @brief This function handles the timeout of packet
 *  		transmission
 *  
 *  @param dev     A pointer to net_device structure
 *
 *  @return        N/A
 */
void
woal_tx_timeout(struct net_device *dev)
{
    moal_private *priv = (moal_private *) netdev_priv(dev);
    ENTER();
    PRINTM(MERROR, "%lu : Tx timeout, bss_index=%d\n", jiffies,
           priv->bss_index);
    dev->trans_start = jiffies;
    priv->num_tx_timeout++;
    LEAVE();
}

/** 
 *  @brief This function returns the network statistics
 *  
 *  @param dev     A pointer to net_device structure
 *
 *  @return        A pointer to net_device_stats structure
 */
struct net_device_stats *
woal_get_stats(struct net_device *dev)
{
    moal_private *priv = (moal_private *) netdev_priv(dev);
    return &priv->stats;
}

/** 
 *  @brief This function handles packet transmission
 *  
 *  @param skb     A pointer to sk_buff structure
 *  @param dev     A pointer to net_device structure
 *
 *  @return        0 --success
 */
int
woal_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    moal_private *priv = (moal_private *) netdev_priv(dev);
    mlan_buffer *pmbuf = NULL;
    mlan_status status;
    struct sk_buff *new_skb = NULL;

    ENTER();

    PRINTM(MDATA, "%lu BSS(%d): Data <= kernel\n", jiffies, priv->bss_index);

    if (priv->phandle->surprise_removed == MTRUE) {
        kfree(skb);
        priv->stats.tx_dropped++;
        goto done;
    }
    if (!skb->len || (skb->len > ETH_FRAME_LEN)) {
        PRINTM(MERROR, "Tx Error: Bad skb length %d : %d\n",
               skb->len, ETH_FRAME_LEN);
        kfree(skb);
        priv->stats.tx_dropped++;
        goto done;
    }
    if (skb->cloned ||
        (skb_headroom(skb) <
         (MLAN_MIN_DATA_HEADER_LEN + sizeof(mlan_buffer)))) {
        PRINTM(MWARN, "Tx: Insufficient skb headroom %d\n", skb_headroom(skb));
        /* Insufficient skb headroom - allocate a new skb */
        new_skb =
            skb_realloc_headroom(skb,
                                 MLAN_MIN_DATA_HEADER_LEN +
                                 sizeof(mlan_buffer));
        if (unlikely(!new_skb)) {
            PRINTM(MERROR, "Tx: Cannot allocate skb\n");
            kfree(skb);
            priv->stats.tx_dropped++;
            goto done;
        }
        kfree_skb(skb);
        skb = new_skb;
        PRINTM(MINFO, "new skb headroom %d\n", skb_headroom(skb));
    }
    pmbuf = (mlan_buffer *) skb->head;
    pmbuf->bss_index = priv->bss_index;
    woal_fill_mlan_buffer(priv, pmbuf, skb);
    status = mlan_send_packet(priv->phandle->pmlan_adapter, pmbuf);
    switch (status) {
    case MLAN_STATUS_PENDING:
        atomic_inc(&priv->phandle->tx_pending);
        if (atomic_read(&priv->phandle->tx_pending) >= MAX_TX_PENDING) {
            netif_stop_queue(priv->netdev);
            dev->trans_start = jiffies;
        }
        queue_work(priv->phandle->workqueue, &priv->phandle->main_work);
        break;
    case MLAN_STATUS_SUCCESS:
        priv->stats.tx_packets++;
        priv->stats.tx_bytes += skb->len;
        dev_kfree_skb_any(skb);
        break;
    case MLAN_STATUS_FAILURE:
    default:
        priv->stats.tx_dropped++;
        dev_kfree_skb_any(skb);
        break;
    }
  done:
    LEAVE();
    return 0;
}

/** 
 *  @brief Convert ascii string to Hex integer
 *     
 *  @param d                    A pointer to integer buf
 *  @param s			A pointer to ascii string 
 *  @param dlen			The length of ascii string
 *
 *  @return 	   	        Number of integer  
 */
int
woal_ascii2hex(t_u8 * d, char *s, t_u32 dlen)
{
    unsigned int i;
    t_u8 n;

    ENTER();

    memset(d, 0x00, dlen);

    for (i = 0; i < dlen * 2; i++) {
        if ((s[i] >= 48) && (s[i] <= 57))
            n = s[i] - 48;
        else if ((s[i] >= 65) && (s[i] <= 70))
            n = s[i] - 55;
        else if ((s[i] >= 97) && (s[i] <= 102))
            n = s[i] - 87;
        else
            break;
        if (!(i % 2))
            n = n * 16;
        d[i / 2] += n;
    }

    LEAVE();
    return i;
}

/**
 *  @brief Return integer value of a given ascii string
 *
 *  @param data    Converted data to be returned
 *  @param a       String to be converted
 *
 *  @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
woal_atoi(int *data, char *a)
{
    int i, val = 0, len;

    ENTER();

    len = strlen(a);
    if (!strncmp(a, "0x", 2)) {
        a = a + 2;
        len -= 2;
        *data = woal_atox(a);
        return MLAN_STATUS_SUCCESS;
    }
    for (i = 0; i < len; i++) {
        if (isdigit(a[i])) {
            val = val * 10 + (a[i] - '0');
        } else {
            PRINTM(MERROR, "Invalid char %c in string %s\n", a[i], a);
            return MLAN_STATUS_FAILURE;
        }
    }
    *data = val;

    LEAVE();
    return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Return hex value of a given ascii string
 *
 *  @param a	    String to be converted to ascii
 *
 *  @return 	    The converted character if a is a valid hex, else 0
 */
int
woal_atox(char *a)
{
    int i = 0;

    ENTER();

    while (isxdigit(*a))
        i = i * 16 + woal_hexval(*a++);

    LEAVE();
    return i;
}

/** 
 *  @brief Extension of strsep lib command. This function will also take care
 *	   escape character
 *
 *  @param s         A pointer to array of chars to process
 *  @param delim     The delimiter character to end the string
 *  @param esc       The escape character to ignore for delimiter
 *
 *  @return          Pointer to the separated string if delim found, else NULL
 */
char *
woal_strsep(char **s, char delim, char esc)
{
    char *se = *s, *sb;

    ENTER();

    if (!(*s) || (*se == '\0')) {
        LEAVE();
        return NULL;
    }

    for (sb = *s; *sb != '\0'; ++sb) {
        if (*sb == esc && *(sb + 1) == esc) {
            /* 
             * We get a esc + esc seq then keep the one esc
             * and chop off the other esc character
             */
            memmove(sb, sb + 1, strlen(sb));
            continue;
        }
        if (*sb == esc && *(sb + 1) == delim) {
            /* 
             * We get a delim + esc seq then keep the delim
             * and chop off the esc character
             */
            memmove(sb, sb + 1, strlen(sb));
            continue;
        }
        if (*sb == delim)
            break;
    }

    if (*sb == '\0')
        sb = NULL;
    else
        *sb++ = '\0';

    *s = sb;

    LEAVE();
    return se;
}

/**
 *  @brief Convert mac address from string to t_u8 buffer.
 *
 *  @param mac_addr The buffer to store the mac address in.	    
 *  @param buf      The source of mac address which is a string.	    
 *
 *  @return 	    N/A
 */
void
woal_mac2u8(t_u8 * mac_addr, char *buf)
{
    char *begin = buf, *end;
    int i;

    ENTER();

    for (i = 0; i < ETH_ALEN; ++i) {
        end = woal_strsep(&begin, ':', '/');
        if (end)
            mac_addr[i] = woal_atox(end);
    }

    LEAVE();
}

#ifdef STA_SUPPORT
/** 
 *  @brief This function sets multicast addresses to firmware
 *  
 *  @param dev     A pointer to net_device structure
 *
 *  @return        N/A
 */
void
woal_set_multicast_list(struct net_device *dev)
{
    moal_private *priv = (moal_private *) netdev_priv(dev);
    ENTER();
    woal_request_set_multicast_list(priv, dev);
    LEAVE();
}
#endif

/**
 *  @brief This function initializes the private structure
 *  		and set default value to the member of moal_private.
 *  
 *  @param priv             A pointer to moal_private structure
 *  @param wait_option      Wait option  
 *
 *  @return 	   N/A
 */
void
woal_init_priv(moal_private * priv, t_u8 wait_option)
{
    ENTER();
#ifdef STA_SUPPORT
    if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA) {
        priv->current_key_index = 0;
        priv->rate_index = AUTO_RATE;
        priv->is_adhoc_link_sensed = MFALSE;
        memset(&priv->nick_name, 0, sizeof(priv->nick_name));
        priv->num_tx_timeout = 0;

    }
#endif /* STA_SUPPORT */
#ifdef UAP_SUPPORT
    if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP) {
        priv->bss_started = MFALSE;
    }
#endif
    priv->media_connected = MFALSE;
    woal_request_get_fw_info(priv, wait_option, NULL);

    LEAVE();
}

/**
 *  @brief Reset all interfaces if all_intf flag is TRUE, 
 *  	   otherwise specified interface only
 *
 *  @param priv          A pointer to moal_private structure
 *  @param wait_option   Wait option
 *  @param all_intf      TRUE  : all interfaces
 *                       FALSE : current interface only
 *
 *  @return             MLAN_STATUS_SUCCESS --success, otherwise fail  
 */
int
woal_reset_intf(moal_private * priv, t_u8 wait_option, int all_intf)
{
    int ret = MLAN_STATUS_SUCCESS;
    int intf_num;
    moal_handle *handle = priv->phandle;
    mlan_bss_info bss_info;

    ENTER();

    /* Stop queue and detach device */
    if (!all_intf) {
        netif_stop_queue(priv->netdev);
        netif_device_detach(priv->netdev);
    } else {
        for (intf_num = 0; intf_num < handle->priv_num; intf_num++) {
            netif_stop_queue(handle->priv[intf_num]->netdev);
            netif_device_detach(handle->priv[intf_num]->netdev);
        }
    }

    /* Get BSS info */
    memset(&bss_info, 0, sizeof(bss_info));
    woal_get_bss_info(priv, wait_option, &bss_info);

#ifdef STA_SUPPORT
    /* If scan is in process, cancel the scan command */
    if (handle->scan_pending_on_block == MTRUE) {
        mlan_ioctl_req *req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_scan));
        if (req == NULL) {
            ret = -EFAULT;
            goto err_cancel_scan;
        }
        req->req_id = MLAN_IOCTL_SCAN;
        req->action = MLAN_ACT_SET;
        ((mlan_ds_scan *) req->pbuf)->sub_command = MLAN_OID_SCAN_CANCEL;
        if (MLAN_STATUS_SUCCESS != woal_request_ioctl(priv, req, wait_option)) {
            ret = -EFAULT;
            goto err_cancel_scan;
        }
      err_cancel_scan:
        handle->scan_pending_on_block = MFALSE;
        MOAL_REL_SEMAPHORE(&handle->async_sem);
        if (req)
            kfree(req);
    }
#endif

#ifdef STA_SUPPORT
    /* Exit deep sleep */
    if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA && bss_info.is_deep_sleep)
        woal_set_deep_sleep(priv, wait_option, MFALSE, 0);
#endif /* STA_SUPPORT */

    /* Cancel host sleep */
    if (bss_info.is_hs_configured) {
        if (MLAN_STATUS_SUCCESS != woal_cancel_hs(priv, wait_option)) {
            ret = -EFAULT;
            goto done;
        }
    }

    /* Disconnect from network */
    if (!all_intf) {
        /* Disconnect specified interface only */
        if ((priv->media_connected == MTRUE)
#ifdef UAP_SUPPORT
            || (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP)
#endif
            ) {
            woal_disconnect(priv, wait_option, NULL);
            priv->media_connected = MFALSE;
        }
    } else {
        /* Disconnect all interfaces */
        for (intf_num = 0; intf_num < handle->priv_num; intf_num++) {
            if (handle->priv[intf_num]->media_connected == MTRUE
#ifdef UAP_SUPPORT
                || (GET_BSS_ROLE(handle->priv[intf_num]) == MLAN_BSS_ROLE_UAP)
#endif
                ) {
                woal_disconnect(handle->priv[intf_num], wait_option, NULL);
                handle->priv[intf_num]->media_connected = MFALSE;
            }
        }
    }

#ifdef REASSOCIATION
    /* Reset the reassoc timer and status */
    handle->reassoc_on = MFALSE;
    if (handle->is_reassoc_timer_set) {
        woal_cancel_timer(&handle->reassoc_timer);
        handle->is_reassoc_timer_set = MFALSE;
    }
#endif /* REASSOCIATION */

  done:
    LEAVE();
    return ret;
}

/** 
 *  @brief This function return the point to structure moal_private 
 *  
 *  @param handle       Pointer to structure moal_handle
 *  @param bss_index    BSS index number
 *
 *  @return             moal_private pointer or NULL
 */
moal_private *
woal_bss_index_to_priv(moal_handle * handle, t_u8 bss_index)
{
    int i;

    ENTER();
    if (!handle) {
        LEAVE();
        return NULL;
    }
    for (i = 0; i < MLAN_MAX_BSS_NUM; i++) {
        if (handle->priv[i] && (handle->priv[i]->bss_index == bss_index)) {
            LEAVE();
            return handle->priv[i];
        }
    }

    LEAVE();
    return NULL;
}

/** 
 *  @brief This function alloc mlan_buffer.
 *  @param handle  A pointer to moal_handle structure 
 *  @param size	   buffer size to allocate
 *
 *  @return        mlan_buffer pointer or NULL
 */
pmlan_buffer
woal_alloc_mlan_buffer(moal_handle * handle, int size)
{
    mlan_buffer *pmbuf = NULL;
    struct sk_buff *skb;

    ENTER();
    if (!(pmbuf = kmalloc(sizeof(mlan_buffer), GFP_ATOMIC))) {
        PRINTM(MERROR, "%s: Fail to alloc mlan buffer\n", __FUNCTION__);
        return NULL;
    }
    memset((t_u8 *) pmbuf, 0, sizeof(mlan_buffer));
    if (!(skb = dev_alloc_skb(size))) {
        PRINTM(MERROR, "%s: No free skb\n", __FUNCTION__);
        kfree(pmbuf);
        return NULL;
    }
    pmbuf->pdesc = (t_void *) skb;
    pmbuf->pbuf = (t_u8 *) skb->tail;
    handle->mbufalloc_count++;
    LEAVE();
    return pmbuf;
}

/** 
 *  @brief This function alloc mlan_ioctl_req.
 *
 *  @param size	   buffer size to allocate
 *
 *  @return        mlan_ioctl_req pointer or NULL
 */
pmlan_ioctl_req
woal_alloc_mlan_ioctl_req(int size)
{
    mlan_ioctl_req *req = NULL;

    ENTER();

    if (!
        (req =
         (mlan_ioctl_req *)
         kmalloc((sizeof(mlan_ioctl_req) + size + sizeof(int) +
                  sizeof(wait_queue)), GFP_ATOMIC))) {
        PRINTM(MERROR, "%s: Fail to alloc ioctl buffer\n", __FUNCTION__);
        LEAVE();
        return NULL;
    }
    memset((t_u8 *) req, 0, (sizeof(mlan_ioctl_req) + size +
                             sizeof(int) + sizeof(wait_queue)));
    req->pbuf = (t_u8 *) req + sizeof(mlan_ioctl_req);
    req->buf_len = (t_u32) size;
    req->reserved_1 =
        ALIGN_ADDR((t_u8 *) req + sizeof(mlan_ioctl_req) + size, sizeof(int));

    LEAVE();
    return req;
}

/** 
 *  @brief This function frees mlan_buffer.
 *  @param handle  A pointer to moal_handle structure 
 *  @param pmbuf   Pointer to mlan_buffer
 *
 *  @return        N/A
 */
void
woal_free_mlan_buffer(moal_handle * handle, pmlan_buffer pmbuf)
{
    ENTER();
    if (!pmbuf)
        return;
    if (pmbuf->pdesc)
        dev_kfree_skb_any((struct sk_buff *) pmbuf->pdesc);
    kfree(pmbuf);
    handle->mbufalloc_count--;
    LEAVE();
    return;
}

/** 
 *  @brief This function handles events generated by firmware
 *  
 *  @param priv    A pointer to moal_private structure
 *  @param payload A pointer to payload buffer
 *  @param len	   Length of the payload
 *  @return 	   MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
woal_broadcast_event(moal_private * priv, t_u8 * payload, t_u32 len)
{
    mlan_status ret = MLAN_STATUS_SUCCESS;
    struct sk_buff *skb = NULL;
    struct nlmsghdr *nlh = NULL;
    moal_handle *handle = priv->phandle;
    struct sock *sk = handle->nl_sk;

    ENTER();
    /* interface name to be prepended to event */
    if ((len + IFNAMSIZ) > NL_MAX_PAYLOAD) {
        PRINTM(MERROR, "event size is too big, len=%d\n", (int) len);
        ret = MLAN_STATUS_FAILURE;
        goto done;
    }
    if (sk) {
        /* Allocate skb */
        if (!(skb = alloc_skb(NLMSG_SPACE(NL_MAX_PAYLOAD), GFP_ATOMIC))) {
            PRINTM(MERROR, "Could not allocate skb for netlink\n");
            ret = MLAN_STATUS_FAILURE;
            goto done;
        }
        nlh = (struct nlmsghdr *) skb->data;
        nlh->nlmsg_len = NLMSG_SPACE(len + IFNAMSIZ);

        /* From kernel */
        nlh->nlmsg_pid = 0;
        nlh->nlmsg_flags = 0;

        /* Data */
        skb_put(skb, nlh->nlmsg_len);
        memcpy(NLMSG_DATA(nlh), priv->netdev->name, IFNAMSIZ);
        memcpy(((t_u8 *) (NLMSG_DATA(nlh))) + IFNAMSIZ, payload, len);

        /* From Kernel */
        NETLINK_CB(skb).pid = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
        /* Multicast message */
        NETLINK_CB(skb).dst_pid = 0;
#endif

        /* Multicast group number */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
        NETLINK_CB(skb).dst_groups = NL_MULTICAST_GROUP;
#else
        NETLINK_CB(skb).dst_group = NL_MULTICAST_GROUP;
#endif

        /* Send message */
        netlink_broadcast(sk, skb, 0, NL_MULTICAST_GROUP, GFP_ATOMIC);

        ret = MLAN_STATUS_SUCCESS;
    } else {
        PRINTM(MERROR, "Could not send event through NETLINK. Link down.\n");
        ret = MLAN_STATUS_FAILURE;
    }
  done:
    LEAVE();
    return ret;
}

#ifdef REASSOCIATION
/**
 *  @brief This function handles re-association. it is triggered
 *  by re-assoc timer.
 *
 *  @param data    A pointer to wlan_thread structure
 *  @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
int
woal_reassociation_thread(void *data)
{
    moal_thread *pmoal_thread = data;
    moal_private *priv = NULL;
    moal_handle *handle = (moal_handle *) pmoal_thread->handle;
    wait_queue_t wait;
    int i;
    BOOLEAN reassoc_timer_req;
    mlan_802_11_ssid req_ssid;
    mlan_ssid_bssid ssid_bssid;
    mlan_status status;
    mlan_bss_info bss_info;
    t_u32 timer_val = MOAL_TIMER_10S;

    ENTER();

    woal_activate_thread(pmoal_thread);
    init_waitqueue_entry(&wait, current);

    current->flags |= PF_NOFREEZE;

    for (;;) {
        add_wait_queue(&pmoal_thread->wait_q, &wait);
        set_current_state(TASK_INTERRUPTIBLE);

        schedule();

        set_current_state(TASK_RUNNING);
        remove_wait_queue(&pmoal_thread->wait_q, &wait);

        /* Cancel re-association timer */
        if (handle->is_reassoc_timer_set == MTRUE) {
            woal_cancel_timer(&handle->reassoc_timer);
            handle->is_reassoc_timer_set = MFALSE;
        }

        if (handle->surprise_removed)
            break;
        if (kthread_should_stop())
            break;

        if (handle->hardware_status != HardwareStatusReady) {
            PRINTM(MINFO, "Reassoc: Hardware status is not correct\n");
            continue;
        }

        PRINTM(MEVENT, "Reassoc: Thread waking up...\n");
        reassoc_timer_req = MFALSE;

        for (i = 0; i < handle->priv_num && (priv = handle->priv[i]); i++) {

            if (priv->reassoc_required == MFALSE)
                continue;

            memset(&bss_info, 0x00, sizeof(bss_info));

            if (MLAN_STATUS_SUCCESS != woal_get_bss_info(priv,
                                                         MOAL_CMD_WAIT,
                                                         &bss_info)) {
                PRINTM(MINFO, "Ressoc: Fail to get bss info\n");
                priv->reassoc_required = MFALSE;
                continue;
            }

            if (bss_info.bss_mode != MLAN_BSS_MODE_INFRA ||
                priv->media_connected != MFALSE) {
                PRINTM(MINFO, "Reassoc: ad-hoc mode or media connected\n");
                priv->reassoc_required = MFALSE;
                continue;
            }

            /* The semaphore is used to avoid reassociation thread and
               wlan_set_scan/wlan_set_essid interrupting each other.
               Reassociation should be disabled completely by application if
               wlan_set_user_scan_ioctl/wlan_set_wap is used. */
            if (MOAL_ACQ_SEMAPHORE_BLOCK(&handle->reassoc_sem)) {
                PRINTM(MERROR,
                       "Acquire semaphore error, reassociation thread\n");
                reassoc_timer_req = MTRUE;
                break;
            }

            PRINTM(MINFO, "Reassoc: Required ESSID: %s\n",
                   priv->prev_ssid_bssid.ssid.ssid);
            PRINTM(MINFO, "Reassoc: Performing Active Scan\n");

            memset(&req_ssid, 0x00, sizeof(mlan_802_11_ssid));
            memcpy(&req_ssid,
                   &priv->prev_ssid_bssid.ssid, sizeof(mlan_802_11_ssid));

            /* Do specific SSID scanning */
            if (MLAN_STATUS_SUCCESS !=
                woal_request_scan(priv, MOAL_CMD_WAIT, &req_ssid)) {
                PRINTM(MERROR, "Reassoc: Fail to do specific scan\n");
                reassoc_timer_req = MTRUE;
                MOAL_REL_SEMAPHORE(&handle->reassoc_sem);
                break;
            }

            if (handle->surprise_removed) {
                MOAL_REL_SEMAPHORE(&handle->reassoc_sem);
                break;
            }

            /* Search AP by BSSID first */
            PRINTM(MINFO, "Reassoc: Search AP by BSSID first\n");
            memset(&ssid_bssid, 0, sizeof(mlan_ssid_bssid));
            memcpy(&ssid_bssid.bssid,
                   &priv->prev_ssid_bssid.bssid, MLAN_MAC_ADDR_LENGTH);
            status = woal_find_best_network(priv, MOAL_CMD_WAIT, &ssid_bssid);

            if (MLAN_STATUS_SUCCESS != status) {
                PRINTM(MINFO, "Reassoc: AP not found in scan list\n");
                PRINTM(MINFO, "Reassoc: Search AP by SSID\n");
                /* Search AP by SSID */
                memset(&ssid_bssid, 0, sizeof(mlan_ssid_bssid));
                memcpy(&ssid_bssid.ssid,
                       &priv->prev_ssid_bssid.ssid, sizeof(mlan_802_11_ssid));
                status = woal_find_best_network(priv,
                                                MOAL_CMD_WAIT, &ssid_bssid);
            }

            if (status == MLAN_STATUS_SUCCESS) {
                /* set the wep key */
                if (bss_info.wep_status)
                    woal_enable_wep_key(priv, MOAL_CMD_WAIT);
                /* Zero SSID implies use BSSID to connect */
                memset(&ssid_bssid.ssid, 0, sizeof(mlan_802_11_ssid));
                status = woal_bss_start(priv, MOAL_CMD_WAIT, &ssid_bssid);
            }

            if (priv->media_connected == MFALSE)
                reassoc_timer_req = MTRUE;
            else {
                mlan_ds_rate *rate = NULL;
                mlan_ioctl_req *req = NULL;

                reassoc_timer_req = MFALSE;

                if (priv->rate_index != AUTO_RATE) {
                    req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_rate));

                    if (req == NULL) {
                        LEAVE();
                        return MLAN_STATUS_FAILURE;
                    }

                    rate = (mlan_ds_rate *) req->pbuf;
                    rate->param.rate_cfg.rate_type = MLAN_RATE_INDEX;
                    rate->sub_command = MLAN_OID_RATE_CFG;
                    req->req_id = MLAN_IOCTL_RATE;

                    req->action = MLAN_ACT_SET;

                    rate->param.rate_cfg.rate = priv->rate_index;

                    if (MLAN_STATUS_SUCCESS
                        != woal_request_ioctl(priv, req, MOAL_CMD_WAIT)) {
                        kfree(req);
                        LEAVE();
                        return MLAN_STATUS_FAILURE;
                    }
                    if (req)
                        kfree(req);
                }
            }

            MOAL_REL_SEMAPHORE(&handle->reassoc_sem);
        }

        if (handle->surprise_removed)
            break;

        if (reassoc_timer_req == MTRUE) {
            PRINTM(MEVENT,
                   "Reassoc: No AP found or assoc failed. "
                   "Restarting re-assoc Timer: %d\n", (int) timer_val);
            handle->is_reassoc_timer_set = MTRUE;
            woal_mod_timer(&handle->reassoc_timer, timer_val);
        }
    }
    woal_deactivate_thread(pmoal_thread);

    LEAVE();
    return MLAN_STATUS_SUCCESS;
}

/** 
 *  @brief This function triggers re-association by waking up
 *  re-assoc thread.
 *  
 *  @param context	A pointer to context
 *  @return		N/A
 */
void
woal_reassoc_timer_func(void *context)
{
    moal_handle *handle = (moal_handle *) context;

    ENTER();

    PRINTM(MINFO, "reassoc_timer fired.\n");
    handle->is_reassoc_timer_set = MFALSE;

    PRINTM(MINFO, "Waking Up the Reassoc Thread\n");
    wake_up_interruptible(&handle->reassoc_thread.wait_q);

    LEAVE();
    return;
}
#endif /* REASSOCIATION */

#ifdef STA_SUPPORT
t_void
woal_send_disconnect_to_system(moal_private * priv)
{
    union iwreq_data wrqu;

    priv->media_connected = MFALSE;
    if (!netif_queue_stopped(priv->netdev))
        netif_stop_queue(priv->netdev);
    if (netif_carrier_ok(priv->netdev))
        netif_carrier_off(priv->netdev);
    memset(wrqu.ap_addr.sa_data, 0x00, ETH_ALEN);
    wrqu.ap_addr.sa_family = ARPHRD_ETHER;
    wireless_send_event(priv->netdev, SIOCGIWAP, &wrqu, NULL);
}
#endif

/**
 *  @brief This workqueue function handles main_process
 *  
 *  @param work    A pointer to work_struct
 *
 *  @return        N/A
 */
t_void
woal_main_work_queue(struct work_struct *work)
{
    moal_handle *handle = container_of(work, moal_handle, main_work);

    ENTER();

    if (handle->surprise_removed == MTRUE) {
        LEAVE();
        return;
    }
    handle->main_state = MOAL_ENTER_WORK_QUEUE;
    sdio_claim_host(((struct sdio_mmc_card *) handle->card)->func);
    handle->main_state = MOAL_START_MAIN_PROCESS;
    /* Call MLAN main process */
    mlan_main_process(handle->pmlan_adapter);
    handle->main_state = MOAL_END_MAIN_PROCESS;
    sdio_release_host(((struct sdio_mmc_card *) handle->card)->func);

    LEAVE();
}

void
woal_interrupt(moal_handle * handle)
{
    ENTER();
    handle->main_state = MOAL_RECV_INT;
    PRINTM(MINTR, "*\n");
    if (handle->surprise_removed == MTRUE) {
        LEAVE();
        return;
    }
    /* call mlan_interrupt to read int status */
    mlan_interrupt(handle->pmlan_adapter);
    queue_work(handle->workqueue, &handle->main_work);
    LEAVE();
}

/**
 * @brief This function adds the card. it will probe the
 * 		card, allocate the mlan_private and initialize the device. 
 *  
 *  @param card    A pointer to card
 *
 *  @return        A pointer to moal_handle structure
 */
moal_handle *
woal_add_card(void *card)
{
    moal_handle *handle = NULL;
    mlan_status status = MLAN_STATUS_SUCCESS;
    int netlink_num = NETLINK_MARVELL;
    int index = 0;

    ENTER();

    if (MOAL_ACQ_SEMAPHORE_BLOCK(&AddRemoveCardSem))
        goto exit_sem_err;

    /* Allocate buffer for moal_handle */
    if (!(handle = kmalloc(sizeof(moal_handle), GFP_KERNEL))) {
        PRINTM(MERROR, "Allocate buffer for moal_handle failed!\n");
        goto err_handle;
    }

    /* Init moal_handle */
    memset(handle, 0, sizeof(moal_handle));
    handle->card = card;
    /* Save the handle */
    for (index = 0; index < MAX_MLAN_ADAPTER; index++) {
        if (m_handle[index] == NULL)
            break;
    }
    if (index < MAX_MLAN_ADAPTER) {
        m_handle[index] = handle;
        handle->handle_idx = index;
    } else {
        PRINTM(MERROR, "Exceeded maximum cards supported!\n");
        goto err_kmalloc;
    }

    if (mac_addr) {
        t_u8 temp[20];
        t_u8 len = strlen(mac_addr) + 1;
        if (len < sizeof(temp)) {
            memcpy(temp, mac_addr, len);
            handle->set_mac_addr = 1;
            /* note: the following function overwrites the temp buffer */
            woal_mac2u8(handle->mac_addr, temp);
        }
    }

    ((struct sdio_mmc_card *) card)->handle = handle;
#ifdef STA_SUPPORT
    handle->scan_pending_on_block = MFALSE;
    MOAL_INIT_SEMAPHORE(&handle->async_sem);
#endif

    /* Init SW */
    if (MLAN_STATUS_SUCCESS != woal_init_sw(handle)) {
        PRINTM(MFATAL, "Software Init Failed\n");
        goto err_kmalloc;
    }

    do {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
        handle->nl_sk = netlink_kernel_create(netlink_num, NULL);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
        handle->nl_sk =
            netlink_kernel_create(netlink_num, NL_MULTICAST_GROUP, NULL,
                                  THIS_MODULE);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
        handle->nl_sk =
            netlink_kernel_create(netlink_num, NL_MULTICAST_GROUP, NULL, NULL,
                                  THIS_MODULE);
#else
        handle->nl_sk =
            netlink_kernel_create(&init_net, netlink_num, NL_MULTICAST_GROUP,
                                  NULL, NULL, THIS_MODULE);
#endif
#endif
#endif
        if (handle->nl_sk) {
            PRINTM(MINFO, "Netlink number = %d\n", netlink_num);
            handle->netlink_num = netlink_num;
            break;
        }
        netlink_num--;
    } while (netlink_num > 0);

    if (handle->nl_sk == NULL) {
        PRINTM(MERROR,
               "Could not initialize netlink event passing mechanism!\n");
        goto err_kmalloc;
    }

    /* Create workqueue for main process */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
    /* For kernel less than 2.6.14 name can not be greater than 10 characters */
    handle->workqueue = create_workqueue("MOAL_WORKQ");
#else
    handle->workqueue = create_workqueue("MOAL_WORK_QUEUE");
#endif
    if (!handle->workqueue)
        goto err_kmalloc;

    MLAN_INIT_WORK(&handle->main_work, woal_main_work_queue);

#ifdef REASSOCIATION
    PRINTM(MINFO, "Starting re-association thread...\n");
    handle->reassoc_thread.handle = handle;
    woal_create_thread(woal_reassociation_thread,
                       &handle->reassoc_thread, "woal_reassoc_service");

    while (!handle->reassoc_thread.pid) {
        woal_sched_timeout(2);
    }
#endif /* REASSOCIATION */

    /* Register the device. Fill up the private data structure with relevant
       information from the card and request for the required IRQ. */
    if (woal_register_dev(handle) != MLAN_STATUS_SUCCESS) {
        PRINTM(MFATAL, "Failed to register wlan device!\n");
        goto err_registerdev;
    }

    /* Init FW and HW */
    if (MLAN_STATUS_SUCCESS != woal_init_fw(handle)) {
        PRINTM(MFATAL, "Firmware Init Failed\n");
        goto err_init_fw;
    }

    LEAVE();
    return handle;

  err_init_fw:
    /* Unregister device */
    PRINTM(MINFO, "unregister device\n");
    woal_unregister_dev(handle);
  err_registerdev:
    handle->surprise_removed = MTRUE;
#ifdef REASSOCIATION
    if (handle->reassoc_thread.pid) {
        wake_up_interruptible(&handle->reassoc_thread.wait_q);
    }
    /* waiting for main thread quit */
    while (handle->reassoc_thread.pid) {
        woal_sched_timeout(2);
    }
#endif /* REASSOCIATION */
    woal_terminate_workqueue(handle);
  err_kmalloc:
    if ((handle->hardware_status == HardwareStatusFwReady) ||
        (handle->hardware_status == HardwareStatusReady)) {
        PRINTM(MINFO, "shutdown mlan\n");
        handle->init_wait_q_woken = MFALSE;
        status = mlan_shutdown_fw(handle->pmlan_adapter);
        if (status == MLAN_STATUS_PENDING)
            wait_event_interruptible(handle->init_wait_q,
                                     handle->init_wait_q_woken);
    }
    woal_free_moal_handle(handle);
    if (index < MAX_MLAN_ADAPTER) {
        m_handle[index] = NULL;
    }
    ((struct sdio_mmc_card *) card)->handle = NULL;
  err_handle:
    MOAL_REL_SEMAPHORE(&AddRemoveCardSem);
  exit_sem_err:
    LEAVE();
    return NULL;
}

/** 
 *  @brief This function removes the card.
 *  
 *  @param card    A pointer to card
 *
 *  @return        MLAN_STATUS_SUCCESS
 */
mlan_status
woal_remove_card(void *card)
{
    moal_handle *handle = NULL;
    moal_private *priv = NULL;
    mlan_status status;
    int i;
    int index = 0;

    ENTER();

    if (MOAL_ACQ_SEMAPHORE_BLOCK(&AddRemoveCardSem))
        goto exit_sem_err;
    /* Find the correct handle */
    for (index = 0; index < MAX_MLAN_ADAPTER; index++) {
        if (m_handle[index] && (m_handle[index]->card == card)) {
            handle = m_handle[index];
            break;
        }
    }
    if (!handle)
        goto exit_remove;
    handle->surprise_removed = MTRUE;

    /* Stop data */
    for (i = 0; i < handle->priv_num; i++) {
        if ((priv = handle->priv[i])) {
            if (!netif_queue_stopped(priv->netdev))
                netif_stop_queue(priv->netdev);
            if (netif_carrier_ok(priv->netdev))
                netif_carrier_off(priv->netdev);
        }
    }

    if ((handle->hardware_status == HardwareStatusFwReady) ||
        (handle->hardware_status == HardwareStatusReady)) {
        /* Shutdown firmware */
        PRINTM(MIOCTL, "mlan_shutdown_fw.....\n");
        handle->init_wait_q_woken = MFALSE;

        status = mlan_shutdown_fw(handle->pmlan_adapter);
        if (status == MLAN_STATUS_PENDING)
            wait_event_interruptible(handle->init_wait_q,
                                     handle->init_wait_q_woken);
        PRINTM(MIOCTL, "mlan_shutdown_fw done!\n");
    }
    if (atomic_read(&handle->rx_pending) || atomic_read(&handle->tx_pending) ||
        atomic_read(&handle->ioctl_pending)) {
        PRINTM(MERROR, "ERR: rx_pending=%d,tx_pending=%d,ioctl_pending=%d\n",
               atomic_read(&handle->rx_pending),
               atomic_read(&handle->tx_pending),
               atomic_read(&handle->ioctl_pending));
    }

    /* Remove interface */
    for (i = 0; i < handle->priv_num; i++)
        woal_remove_interface(handle, i);

    woal_terminate_workqueue(handle);

#ifdef REASSOCIATION
    PRINTM(MINFO, "Free reassoc_timer\n");
    if (handle->is_reassoc_timer_set) {
        woal_cancel_timer(&handle->reassoc_timer);
        handle->is_reassoc_timer_set = MFALSE;
    }
    if (handle->reassoc_thread.pid)
        wake_up_interruptible(&handle->reassoc_thread.wait_q);

    /* waiting for main thread quit */
    while (handle->reassoc_thread.pid) {
        woal_sched_timeout(2);
    }
#endif /* REASSOCIATION */

#ifdef CONFIG_PROC_FS
    woal_proc_exit(handle);
#endif
    /* Unregister device */
    PRINTM(MINFO, "unregister device\n");
    woal_unregister_dev(handle);
    /* Free adapter structure */
    PRINTM(MINFO, "Free Adapter\n");
    woal_free_moal_handle(handle);

    for (index = 0; index < MAX_MLAN_ADAPTER; index++) {
        if (m_handle[index] == handle) {
            m_handle[index] = NULL;
            break;
        }
    }
  exit_remove:
    MOAL_REL_SEMAPHORE(&AddRemoveCardSem);
  exit_sem_err:
    LEAVE();
    return MLAN_STATUS_SUCCESS;
}

/** 
 *  @brief This function switch the drv_mode
 *  
 *  @param handle    A pointer to moal_handle structure
 *  @param mode     new drv_mode to switch.
 *
 *  @return        MLAN_STATUS_SUCCESS /MLAN_STATUS_FAILURE /MLAN_STATUS_PENDING
 */
mlan_status
woal_switch_drv_mode(moal_handle * handle, t_u32 mode)
{
    unsigned int i;
    mlan_status status = MLAN_STATUS_SUCCESS;
    moal_private *priv = NULL;

    ENTER();

    if (MOAL_ACQ_SEMAPHORE_BLOCK(&AddRemoveCardSem))
        goto exit_sem_err;

    if (wlan_update_drv_tbl(handle, mode) != MLAN_STATUS_SUCCESS) {
        PRINTM(MERROR, "Could not update driver mode table!\n");
        status = MLAN_STATUS_FAILURE;
        goto exit;
    }

    /* Reset all interfaces */
    priv = woal_get_priv(handle, MLAN_BSS_ROLE_ANY);
    woal_reset_intf(priv, MOAL_PROC_WAIT, MTRUE);

    status = woal_shutdown_fw(priv, MOAL_CMD_WAIT);
    if (status != MLAN_STATUS_SUCCESS) {
        PRINTM(MERROR, "func shutdown failed!\n");
        goto exit;
    }

    /* Shutdown firmware */
    PRINTM(MIOCTL, "mlan_shutdown_fw.....\n");
    handle->init_wait_q_woken = MFALSE;
    status = mlan_shutdown_fw(handle->pmlan_adapter);
    if (status == MLAN_STATUS_PENDING)
        wait_event_interruptible(handle->init_wait_q,
                                 handle->init_wait_q_woken);
    PRINTM(MIOCTL, "mlan_shutdown_fw done!\n");
    if (atomic_read(&handle->rx_pending) || atomic_read(&handle->tx_pending) ||
        atomic_read(&handle->ioctl_pending)) {
        PRINTM(MERROR, "ERR: rx_pending=%d,tx_pending=%d,ioctl_pending=%d\n",
               atomic_read(&handle->rx_pending),
               atomic_read(&handle->tx_pending),
               atomic_read(&handle->ioctl_pending));
    }

    /* Remove interface */
    for (i = 0; i < handle->priv_num; i++)
        woal_remove_interface(handle, i);

    /* Unregister mlan */
    if (handle->pmlan_adapter) {
        mlan_unregister(handle->pmlan_adapter);
        if (handle->malloc_count || handle->lock_count) {
            PRINTM(MERROR,
                   "mlan has memory leak: malloc_count=%lu lock_count=%lu\n",
                   handle->malloc_count, handle->lock_count);
        }
        handle->pmlan_adapter = NULL;
    }

    handle->priv_num = 0;
    drv_mode = mode;
    /* Init SW */
    if (woal_init_sw(handle)) {
        PRINTM(MFATAL, "Software Init Failed\n");
        goto exit;
    }
    /* Init FW and HW */
    if (woal_init_fw(handle)) {
        PRINTM(MFATAL, "Firmware Init Failed\n");
        goto exit;
    }
    LEAVE();
    return status;
  exit:
    MOAL_REL_SEMAPHORE(&AddRemoveCardSem);
  exit_sem_err:
    LEAVE();
    return status;
}

/** 
 *  @brief This function initializes module.
 *  
 *  @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static int
woal_init_module(void)
{
    int ret = (int) MLAN_STATUS_SUCCESS;
    int index = 0;

    ENTER();

    /* Init the wlan_private pointer array first */
    for (index = 0; index < MAX_MLAN_ADAPTER; index++) {
        m_handle[index] = NULL;
    }
    /* Init mutex */
    MOAL_INIT_SEMAPHORE(&AddRemoveCardSem);

    /* Register with bus */
    ret = woal_bus_register();

    LEAVE();
    return ret;
}

/** 
 *  @brief This function cleans module
 *  
 *  @return        N/A
 */
static void
woal_cleanup_module(void)
{
    moal_handle *handle = NULL;
    int index = 0;
    int i;

    ENTER();

    if (MOAL_ACQ_SEMAPHORE_BLOCK(&AddRemoveCardSem))
        goto exit_sem_err;
    for (index = 0; index < MAX_MLAN_ADAPTER; index++) {
        handle = m_handle[index];
        if (!handle)
            continue;
        if (!handle->priv_num)
            goto exit;
#ifdef SDIO_SUSPEND_RESUME
#ifdef MMC_PM_KEEP_POWER
        if (handle->is_suspended == MTRUE) {
            woal_sdio_resume(&(((struct sdio_mmc_card *) handle->card)->func)->
                             dev);
        }
#endif
#endif /* SDIO_SUSPEND_RESUME */

        for (i = 0; i < handle->priv_num; i++) {
#ifdef STA_SUPPORT
            if ((GET_BSS_ROLE(handle->priv[i]) == MLAN_BSS_ROLE_STA) &&
                (handle->priv[i]->media_connected == MTRUE)) {
                woal_disconnect(handle->priv[i], MOAL_CMD_WAIT, NULL);
            }
#endif
#ifdef UAP_SUPPORT
            if (GET_BSS_ROLE(handle->priv[i]) == MLAN_BSS_ROLE_UAP) {
#ifdef MFG_CMD_SUPPORT
                if (mfg_mode != MLAN_INIT_PARA_ENABLED)
#endif
                    woal_disconnect(handle->priv[i], MOAL_CMD_WAIT, NULL);
            }
#endif
        }

#ifdef MFG_CMD_SUPPORT
        if (mfg_mode != MLAN_INIT_PARA_ENABLED)
#endif
            woal_set_deep_sleep(woal_get_priv(handle, MLAN_BSS_ROLE_ANY),
                                MOAL_CMD_WAIT, MFALSE, 0);

#ifdef MFG_CMD_SUPPORT
        if (mfg_mode != MLAN_INIT_PARA_ENABLED)
#endif
            woal_shutdown_fw(woal_get_priv(handle, MLAN_BSS_ROLE_ANY),
                             MOAL_CMD_WAIT);
    }
  exit:
    MOAL_REL_SEMAPHORE(&AddRemoveCardSem);
  exit_sem_err:
    /* Unregister from bus */
    woal_bus_unregister();
    LEAVE();
}

module_init(woal_init_module);
module_exit(woal_cleanup_module);

module_param(fw_name, charp, 0);
MODULE_PARM_DESC(fw_name, "Firmware name");
module_param(req_fw_nowait, int, 0);
MODULE_PARM_DESC(req_fw_nowait,
                 "0: Use request_firmware API; 1: Use request_firmware_nowait API");
module_param(fw_crc_check, int, 1);
MODULE_PARM_DESC(fw_crc_check,
                 "1: Enable FW download CRC check (default); 0: Disable FW download CRC check");
module_param(mac_addr, charp, 0);
MODULE_PARM_DESC(mac_addr, "MAC address");
#ifdef MFG_CMD_SUPPORT
module_param(mfg_mode, int, 0);
MODULE_PARM_DESC(mfg_mode,
                 "0: Download normal firmware; 1: Download MFG firmware");
#endif /* MFG_CMD_SUPPORT */
module_param(drv_mode, int, 0);
#if defined(WFD_SUPPORT)
MODULE_PARM_DESC(drv_mode, "Bit 0: STA; Bit 1: uAP; Bit 2: WFD");
#else
MODULE_PARM_DESC(drv_mode, "Bit 0: STA; Bit 1: uAP");
#endif /* WFD_SUPPORT && V14_FEATURE */
#ifdef STA_SUPPORT
module_param(max_sta_bss, int, 0);
MODULE_PARM_DESC(max_sta_bss, "Number of STA interfaces (1)");
#endif /* STA_SUPPORT */
#ifdef UAP_SUPPORT
module_param(max_uap_bss, int, 0);
MODULE_PARM_DESC(max_uap_bss, "Number of uAP interfaces (1)");
#endif /* UAP_SUPPORT */
#if defined(WFD_SUPPORT)
module_param(max_wfd_bss, int, 0);
MODULE_PARM_DESC(max_wfd_bss, "Number of WFD interfaces (1)");
#endif /* WFD_SUPPORT && V14_FEATURE */
#ifdef DEBUG_LEVEL1
module_param(drvdbg, ulong, 0);
MODULE_PARM_DESC(drvdbg, "Driver debug");
#endif /* DEBUG_LEVEL1 */
module_param(auto_ds, int, 0);
MODULE_PARM_DESC(auto_ds,
                 "0: MLAN default; 1: Enable auto deep sleep; 2: Disable auto deep sleep");
module_param(ps_mode, int, 0);
MODULE_PARM_DESC(ps_mode,
                 "0: MLAN default; 1: Enable IEEE PS mode; 2: Disable IEEE PS mode");
module_param(max_tx_buf, int, 0);
MODULE_PARM_DESC(max_tx_buf, "Maximum Tx buffer size (2048/4096/8192)");
#ifdef SDIO_SUSPEND_RESUME
module_param(pm_keep_power, int, 1);
MODULE_PARM_DESC(pm_keep_power, "1: PM keep power; 0: PM no power");
#endif
#if defined(STA_SUPPORT)
module_param(cfg_11d, int, 0);
MODULE_PARM_DESC(cfg_11d,
                 "0: MLAN default; 1: Enable 802.11d; 2: Disable 802.11d");
#endif
module_param(init_cfg, charp, 0);
MODULE_PARM_DESC(init_cfg, "Init config file name");
module_param(cal_data_cfg, charp, 0);
MODULE_PARM_DESC(cal_data_cfg, "Calibration data file name");
MODULE_DESCRIPTION("M-WLAN Driver");
MODULE_AUTHOR("Marvell International Ltd.");
MODULE_VERSION(MLAN_RELEASE_VERSION);
MODULE_LICENSE("GPL");
