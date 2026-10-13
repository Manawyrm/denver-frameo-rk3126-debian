#ifndef _RK915_FIRMWARE_H_
#define _RK915_FIRMWARE_H_

#define FW_LOADER_FROM_USER

// 1: use flip_open to get firmware; 0 use request_firmware
#define FW_LOADER_FROM_USER_OPEN	1
#define MAX_FW_BUF_SIZE (64*1024)
#define MAX_PATCH_BUF_SIZE (17*1024)
#define MAX_BLOCK_DATA_SIZE 4096

struct firmware_info {
	int fw_saved;
	int fw_size;
	unsigned char *block_data;
	unsigned char *fw_data;
	int patch_size;
	unsigned char *patch_data;
	int patch2_size;
	unsigned char *patch2_data;
	int cal_size;
	unsigned char *cal_data;
	int rf_para_size;
	unsigned char *rf_para_data;	
	unsigned char *fw_data_check;
	unsigned char *fw_start_data;
	const struct firmware *fw_fw;
	const struct firmware *patch_fw;
	const struct firmware *patch2_fw;
	const struct firmware *cal_fw;
};

#define RF_CAL_DATA_SIZE 156
#define RF_CAL_TAG "RFCD"
#define RF_PARA_DATA_SIZE (94 * 2)

struct rf_cal_hdr {
	unsigned char tag[4];
	unsigned int size;	// total size, include header
	unsigned int cal_enable;
	unsigned char cal_data[RF_CAL_DATA_SIZE];
};

#define RF_CAL_DATA_FILE	"rk915_cal.bin"
#define RF_PARA_DATA_FILE	"rk915_rf_para.txt"

int rk915_download_firmware(struct hal_priv *priv);
int rk915_alloc_firmware_buf(struct firmware_info *fw_info);
void rk915_free_firmware_buf(struct firmware_info *fw_info);
void rk915_mem_check2(struct hal_priv *priv, unsigned int addr, unsigned int len);
#endif
