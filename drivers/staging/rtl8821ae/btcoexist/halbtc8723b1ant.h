/**********************************************************************
 * The following is for 8723B 1ANT BT Co-exist definition
 **********************************************************************/
#define	BT_AUTO_REPORT_ONLY_8723B_1ANT			1

#define	BT_INFO_8723B_1ANT_B_FTP			BIT7
#define	BT_INFO_8723B_1ANT_B_A2DP			BIT6
#define	BT_INFO_8723B_1ANT_B_HID			BIT5
#define	BT_INFO_8723B_1ANT_B_SCO_BUSY			BIT4
#define	BT_INFO_8723B_1ANT_B_ACL_BUSY			BIT3
#define	BT_INFO_8723B_1ANT_B_INQ_PAGE			BIT2
#define	BT_INFO_8723B_1ANT_B_SCO_ESCO			BIT1
#define	BT_INFO_8723B_1ANT_B_CONNECTION			BIT0

#define	BT_INFO_8723B_1ANT_A2DP_BASIC_RATE(_BT_INFO_EXT_)	\
		(((_BT_INFO_EXT_&BIT0))? true:false)

#define	BTC_RSSI_COEX_THRESH_TOL_8723B_1ANT		2

typedef enum _BT_INFO_SRC_8723B_1ANT{
	BT_INFO_SRC_8723B_1ANT_WIFI_FW			= 0x0,
	BT_INFO_SRC_8723B_1ANT_BT_RSP			= 0x1,
	BT_INFO_SRC_8723B_1ANT_BT_ACTIVE_SEND		= 0x2,
	BT_INFO_SRC_8723B_1ANT_MAX
}BT_INFO_SRC_8723B_1ANT,*PBT_INFO_SRC_8723B_1ANT;

typedef enum _BT_8723B_1ANT_BT_STATUS{
	BT_8723B_1ANT_BT_STATUS_NON_CONNECTED_IDLE	= 0x0,
	BT_8723B_1ANT_BT_STATUS_CONNECTED_IDLE		= 0x1,
	BT_8723B_1ANT_BT_STATUS_INQ_PAGE		= 0x2,
	BT_8723B_1ANT_BT_STATUS_ACL_BUSY		= 0x3,
	BT_8723B_1ANT_BT_STATUS_SCO_BUSY		= 0x4,
	BT_8723B_1ANT_BT_STATUS_ACL_SCO_BUSY		= 0x5,
	BT_8723B_1ANT_BT_STATUS_MAX
}BT_8723B_1ANT_BT_STATUS,*PBT_8723B_1ANT_BT_STATUS;

typedef enum _BT_8723B_1ANT_WIFI_STATUS{
	BT_8723B_1ANT_WIFI_STATUS_NON_CONNECTED_IDLE			= 0x0,
	BT_8723B_1ANT_WIFI_STATUS_NON_CONNECTED_ASSO_AUTH_SCAN		= 0x1,
	BT_8723B_1ANT_WIFI_STATUS_CONNECTED_SCAN			= 0x2,
	BT_8723B_1ANT_WIFI_STATUS_CONNECTED_SPECIAL_PKT			= 0x3,
	BT_8723B_1ANT_WIFI_STATUS_CONNECTED_IDLE			= 0x4,
	BT_8723B_1ANT_WIFI_STATUS_CONNECTED_BUSY			= 0x5,
	BT_8723B_1ANT_WIFI_STATUS_MAX
}BT_8723B_1ANT_WIFI_STATUS,*PBT_8723B_1ANT_WIFI_STATUS;

typedef enum _BT_8723B_1ANT_COEX_ALGO{
	BT_8723B_1ANT_COEX_ALGO_UNDEFINED		= 0x0,
	BT_8723B_1ANT_COEX_ALGO_SCO			= 0x1,
	BT_8723B_1ANT_COEX_ALGO_HID			= 0x2,
	BT_8723B_1ANT_COEX_ALGO_A2DP			= 0x3,
	BT_8723B_1ANT_COEX_ALGO_A2DP_PANHS		= 0x4,
	BT_8723B_1ANT_COEX_ALGO_PANEDR			= 0x5,
	BT_8723B_1ANT_COEX_ALGO_PANHS			= 0x6,
	BT_8723B_1ANT_COEX_ALGO_PANEDR_A2DP		= 0x7,
	BT_8723B_1ANT_COEX_ALGO_PANEDR_HID		= 0x8,
	BT_8723B_1ANT_COEX_ALGO_HID_A2DP_PANEDR		= 0x9,
	BT_8723B_1ANT_COEX_ALGO_HID_A2DP		= 0xa,
	BT_8723B_1ANT_COEX_ALGO_MAX			= 0xb,
}BT_8723B_1ANT_COEX_ALGO,*PBT_8723B_1ANT_COEX_ALGO;

struct coex_dm_8723b_1ant{
	/* fw mechanism */
	bool pre_dec_bt_pwr;
	bool cur_dec_bt_pwr;
	u8 pre_fw_dac_swing_lvl;
	u8 cur_fw_dac_swing_lvl;
	bool cur_ignore_wlan_act;
	bool pre_ignore_wlan_act;
	u8 pre_ps_tdma;
	u8 cur_ps_tdma;
	u8 ps_tdma_para[5];
	u8 ps_tdma_du_adj_type;
	bool auto_tdma_adjust;
	bool pre_ps_tdma_on;
	bool cur_ps_tdma_on;
	bool pre_bt_auto_report;
	bool cur_bt_auto_report;
	u8 pre_lps;
	u8 cur_lps;
	u8 pre_rpwm;
	u8 cur_rpwm;

	/* sw mechanism */
	bool pre_rf_rx_lpf_shrink;
	bool cur_rf_rx_lpf_shrink;
	u32 bt_rf0x1e_backup;
	bool pre_low_penalty_ra;
	bool cur_low_penalty_ra;
	bool pre_dac_swing_on;
	u32 pre_dac_swing_lvl;
	bool cur_dac_swing_on;
	u32 cur_dac_swing_lvl;
	bool pre_adc_backoff;
	bool cur_adc_backoff;
	bool pre_agc_table_en;
	bool cur_agc_table_en;
	u32 pre_val0x6c0;
	u32 cur_val0x6c0;
	u32 pre_val0x6c4;
	u32 cur_val0x6c4;
	u32 pre_val0x6c8;
	u32 cur_val0x6c8;
	u8 pre_val0x6cc;
	u8 cur_val0x6cc;
	bool limited_dig;

	u32 backup_arfr_cnt1;	/* Auto Rate Fallback Retry cnt */
	u32 backup_arfr_cnt2;	/* Auto Rate Fallback Retry cnt */
	u16 backup_retry_limit;
	u8 backup_ampdu_max_time;

	/* algorithm related */
	u8 pre_algorithm;
	u8 cur_algorithm;
	u8 bt_status;
	u8 wifi_chnl_info[3];

	u32 prera_mask;
	u32 curra_mask;
	u8 pre_arfr_type;
	u8 cur_arfr_type;
	u8 pre_retry_limit_type;
	u8 cur_retry_limit_type;
	u8 pre_ampdu_time_type;
	u8 cur_ampdu_time_type;

	u8 error_condition;
};

struct coex_sta_8723b_1ant{
	bool bt_link_exist;
	bool sco_exist;
	bool a2dp_exist;
	bool hid_exist;
	bool pan_exist;

	bool under_lps;
	bool under_ips;
	u32 special_pkt_period_cnt;
	u32 high_priority_tx;
	u32 high_priority_rx;
	u32 low_priority_tx;
	u32 low_priority_rx;
	u8 bt_rssi;
	u8 pre_bt_rssi_state;
	u8 pre_wifi_rssi_state[4];
	bool c2h_bt_info_req_sent;
	u8 bt_info_c2h[BT_INFO_SRC_8723B_1ANT_MAX][10];
	u32 bt_info_c2h_cnt[BT_INFO_SRC_8723B_1ANT_MAX];
	bool c2h_bt_inquiry_page;
	u8 bt_retry_cnt;
	u8 bt_info_ext;
};

/*************************************************************************
 * The following is interface which will notify coex module.
 *************************************************************************/
void ex_halbtc8723b1ant_init_hwconfig(struct btc_coexist *btcoexist);
void ex_halbtc8723b1ant_init_coex_dm(struct btc_coexist *btcoexist);
void ex_halbtc8723b1ant_ips_notify(struct btc_coexist *btcoexist, u8 type);
void ex_halbtc8723b1ant_lps_notify(struct btc_coexist *btcoexist, u8 type);
void ex_halbtc8723b1ant_scan_notify(struct btc_coexist *btcoexist, u8 type);
void ex_halbtc8723b1ant_connect_notify(struct btc_coexist *btcoexist, u8 type);
void ex_halbtc8723b1ant_media_status_notify(struct btc_coexist *btcoexist,
					    u8 type);
void ex_halbtc8723b1ant_special_packet_notify(struct btc_coexist *btcoexist,
					      u8 type);
void ex_halbtc8723b1ant_bt_info_notify(struct btc_coexist *btcoexist,
				       u8 *tmpbuf, u8 length);
void ex_halbtc8723b1ant_halt_notify(struct btc_coexist *btcoexist);
void ex_halbtc8723b1ant_pnp_notify(struct btc_coexist *btcoexist, u8 pnpState);
void ex_halbtc8723b1ant_periodical(struct btc_coexist *btcoexist);
void ex_halbtc8723b1ant_display_coex_info(struct btc_coexist *btcoexist);

