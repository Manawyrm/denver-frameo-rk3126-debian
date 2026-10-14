#include <linux/vmalloc.h>

#include "core.h"
#include "if_io.h"
#include "sdio.h"
#include "hal_io.h"

#define FW_READ_BACK_CHECK

struct io_cmd {
        unsigned int id;
        unsigned int length;
        unsigned int addr;
};

void rk915_free_firmware_buf(struct firmware_info *fw_info)
{
	RPU_DEBUG_FIRMWARE("%s\n", __func__);

	if (fw_info->block_data)
		kfree(fw_info->block_data);
	if (fw_info->fw_data)
		vfree(fw_info->fw_data);
	fw_info->fw_data = NULL;
#ifdef FW_READ_BACK_CHECK
	if (fw_info->fw_data_check)
		vfree(fw_info->fw_data_check);
	fw_info->fw_data_check = NULL;
#endif
	if (fw_info->fw_start_data)
		kfree(fw_info->fw_start_data);
	fw_info->fw_start_data = NULL;
#ifdef ENABLE_FW_SPLIT	
	if (fw_info->patch_data)
		kfree(fw_info->patch_data);
	fw_info->patch_data = NULL;
#endif
	if (fw_info->patch2_data)
		kfree(fw_info->patch2_data);
	fw_info->patch2_data = NULL;

	if (fw_info->cal_data)
		kfree(fw_info->cal_data);
	fw_info->cal_data = NULL;

	if (fw_info->rf_para_data)
		kfree(fw_info->rf_para_data);
	fw_info->rf_para_data = NULL;
}

int rk915_alloc_firmware_buf(struct firmware_info *fw_info)
{
	RPU_DEBUG_FIRMWARE("%s\n", __func__);

    fw_info->block_data = kmalloc(MAX_BLOCK_DATA_SIZE, GFP_KERNEL);
	if (fw_info->block_data == NULL) {
		RPU_ERROR_FIRMWARE("alloc block_data failed\n");
		return -1;
	}

	fw_info->fw_data = vmalloc(MAX_FW_BUF_SIZE);
	if (fw_info->fw_data == NULL) {
		RPU_ERROR_FIRMWARE("alloc fw_data failed\n");
		return -1;
	}
#ifdef FW_READ_BACK_CHECK
	fw_info->fw_data_check = vmalloc(MAX_FW_BUF_SIZE);
	if (fw_info->fw_data_check == NULL) {
		RPU_ERROR_FIRMWARE("alloc fw_data_check failed\n");
		return -1;
	}
#endif	
	fw_info->fw_start_data = kmalloc(16, GFP_KERNEL);
	if (fw_info->fw_start_data == NULL) {
		RPU_ERROR_FIRMWARE("alloc fw_start_data failed\n");
		return -1;
	}
#ifdef ENABLE_FW_SPLIT	
	fw_info->patch_data = kmalloc(MAX_PATCH_BUF_SIZE, GFP_KERNEL);
	if (fw_info->patch_data == NULL) {
		RPU_ERROR_FIRMWARE("alloc patch_data failed\n");
		return -1;
	}
#endif
	fw_info->patch2_data = kmalloc(MAX_PATCH_BUF_SIZE, GFP_KERNEL);
	if (fw_info->patch2_data == NULL) {
		RPU_ERROR_FIRMWARE("alloc patch2_data failed\n");
		return -1;
	}

	fw_info->cal_data = kmalloc(RF_CAL_DATA_SIZE, GFP_KERNEL);
	if (fw_info->cal_data == NULL) {
		RPU_ERROR_FIRMWARE("alloc cal_data failed\n");
		return -1;
	}

	fw_info->rf_para_data = kmalloc(RF_PARA_DATA_SIZE, GFP_KERNEL);
	if (fw_info->rf_para_data == NULL) {
		RPU_ERROR_FIRMWARE("alloc rf_para_data failed\n");
		return -1;
	}

	return 0;
}

static int rk915_copy_firmware(struct firmware_info *fw_info,
			int fw_size, int patch_size, int patch2_size, int cal_size, int rf_para_size,
			u8 *fw_data, u8 *patch_data, u8 *patch2_data, u8 *cal_data, u8 *rf_para_data)
{
	struct rf_cal_hdr *cal_hdr;

	fw_info->fw_size =  ALIGN(fw_size, 4);
#ifdef ENABLE_FW_SPLIT	
	fw_info->patch_size = ALIGN(patch_size , 4);
#endif
	fw_info->patch2_size = ALIGN(patch2_size , 4);
	/* append rf cal data before patch2 */
	if (cal_size)
		fw_info->patch2_size += ALIGN(sizeof(struct rf_cal_hdr) , 4);

	if (fw_info->patch2_size > MAX_PATCH_BUF_SIZE) {
		return -1;
	}

	RPU_DEBUG_FIRMWARE("%s: fw_info fw_size: %d, patch_size: %d.\n",
				   __func__, fw_info->fw_size, fw_info->patch_size);
	RPU_DEBUG_FIRMWARE("%s: fw_info patch2_size: %d, cal_size: %d.\n",
				   __func__, fw_info->patch2_size, fw_info->cal_size);

#if !FW_LOADER_FROM_USER_OPEN
	if (fw_data)
		memcpy(fw_info->fw_data, fw_data, fw_size);
#ifdef ENABLE_FW_SPLIT
	if (patch_data)
		memcpy(fw_info->patch_data, patch_data, patch_size);
#endif
#endif
	/* append rf cal data before patch2 */
	if (cal_size) {
		RPU_INFO_FIRMWARE("%s: add rf cal data\n", __func__);
#if FW_LOADER_FROM_USER_OPEN
		memmove(fw_info->patch2_data + ALIGN(sizeof(struct rf_cal_hdr) , 4),
				fw_info->patch2_data, ALIGN(patch2_size , 4));
#endif
		cal_hdr = (struct rf_cal_hdr *)fw_info->patch2_data;
		memcpy(cal_hdr->tag, RF_CAL_TAG, 4);
		cal_hdr->cal_enable = 1;
		cal_hdr->size = ALIGN(sizeof(struct rf_cal_hdr) , 4);
		memcpy(cal_hdr->cal_data, cal_data, RF_CAL_DATA_SIZE);	
#if FW_LOADER_FROM_USER_OPEN
	}
#else
		memcpy(fw_info->patch2_data + cal_hdr->size,
				patch2_data, patch2_size);
	} else {
		memcpy(fw_info->patch2_data, patch2_data, patch2_size);
	}
#endif

	if (rf_para_size) {
		set_rf_params(rf_para_data);
	}

	return 0;
}

#if FW_LOADER_FROM_USER_OPEN
static const char * const fw_path[] = {
	"/etc/firmware",
	"/vendor/etc/firmware",
	"/lib/firmware",
	"/system/etc/firmware"
};

static int rk915_read_firmware_file(struct firmware_info *fw_info, char *name, u8 *buf, int *len)
{
	int i, find = 0;
	char path[64];
	struct file *file;
	int read, size = 1024;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	loff_t pos = 0;
#endif

	for (i = 0; i < ARRAY_SIZE(fw_path); i++) {
		if (!fw_path[i][0])
			continue;

		sprintf(path, "%s/%s", fw_path[i], name);

		file = filp_open(path, O_RDONLY, 0);
		if (IS_ERR(file))
			continue;

		find = 1;
		break;
	}

	if (!find) {
		RPU_ERROR_FIRMWARE("%s: failed to open %s\n", __func__, name);
		return -1;
	}

	*len = 0;
	while (1) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 14, 0))
		read = kernel_read(file, buf, size, &pos);
#else
		read = kernel_read(file, file->f_pos, buf, size);
		file->f_pos += read;
#endif
		if (read <= 0)
			break;

		buf += read;
		*len += read;
	}

	filp_close(file, NULL);

	if (*len > MAX_FW_BUF_SIZE) {
		RPU_ERROR_FIRMWARE("%s file exceed max size %d\n", name, *len);
		return -1;
	}

	return 0;
}

static int rk915_get_firmware_from_open(struct host_io_info *host, struct firmware_info *fw_info)
{
	int ret; 

	RPU_INFO_FIRMWARE("firmware and patch file from open\n");

	ret = rk915_read_firmware_file(fw_info, "rockchip/rk915_fw.bin", fw_info->fw_data, &fw_info->fw_size);
	if (ret < 0)
		return -1;

#ifdef ENABLE_FW_SPLIT
	ret = rk915_read_firmware_file(fw_info, "rockchip/rk915_patch_cal.bin", fw_info->patch_data, &fw_info->patch_size);
	if (ret < 0)
		return -1;
#endif

	ret = rk915_read_firmware_file(fw_info, "rockchip/rk915_patch.bin", fw_info->patch2_data, &fw_info->patch2_size);
	if (ret < 0)
		return -1;

	ret = rk915_read_firmware_file(fw_info, RF_CAL_DATA_FILE, fw_info->cal_data, &fw_info->cal_size);
	if (ret < 0) {
		//RPU_INFO_FIRMWARE("%s load error!\n", RF_CAL_DATA_FILE);
		fw_info->cal_size = 0;
	}

	ret = rk915_read_firmware_file(fw_info, RF_PARA_DATA_FILE, fw_info->rf_para_data, &fw_info->rf_para_size);
	if (ret < 0) {
		//RPU_INFO_FIRMWARE("%s load error!\n", RF_PARA_DATA_FILE);
		fw_info->rf_para_size = 0;
	} else if (fw_info->rf_para_size > RF_PARA_DATA_SIZE) {
		RPU_ERROR_FIRMWARE("%s ivalid rf para data!\n", RF_PARA_DATA_FILE);
		fw_info->rf_para_size = 0;
	}

	ret = rk915_copy_firmware(fw_info, fw_info->fw_size, fw_info->patch_size,
					fw_info->patch2_size, fw_info->cal_size, fw_info->rf_para_size,
					(u8 *)fw_info->fw_data, (u8 *)fw_info->patch_data,
					(u8 *)fw_info->patch2_data, (u8 *)fw_info->cal_data, (u8 *)fw_info->rf_para_data);
	if (ret < 0) {
		RPU_INFO_FIRMWARE("rk915_copy_firmware failed!\n");
		return -1;
	}

	return 0;
}
#else
static int rk915_get_firmware_from_request(struct host_io_info *host, struct firmware_info *fw_info)
{
	int error, ret = -1;

	RPU_INFO_FIRMWARE("firmware and patch file from request\n");

	error = request_firmware(&fw_info->fw_fw, "rockchip/rk915_fw.bin", host->dev);
	if (error < 0) {
		RPU_ERROR_FIRMWARE("rk915_fw.bin load error!\n");
		goto exit_request;
	}

#ifdef ENABLE_FW_SPLIT
	error = request_firmware(&fw_info->patch_fw, "rockchip/rk915_patch_cal.bin", host->dev);
	if (error < 0) {
		RPU_ERROR_FIRMWARE("rk915_patch_cal.bin load error!\n");
		goto exit_request;
	}
#endif

	error = request_firmware(&fw_info->patch2_fw, "rockchip/rk915_patch.bin", host->dev);
	if (error < 0) {
		RPU_ERROR_FIRMWARE("rk915_patch.bin load error!\n");
		goto exit_request;
	}

	error = request_firmware(&fw_info->cal_fw, RF_CAL_DATA_FILE, host->dev);
	if (error < 0) {
		//RPU_INFO_FIRMWARE("%s load error!\n", RF_CAL_DATA_FILE);
		fw_info->cal_fw = NULL;
	}

	error = rk915_copy_firmware(fw_info, fw_info->fw_fw->size, fw_info->patch_fw->size,
					fw_info->patch2_fw->size, fw_info->cal_fw?fw_info->cal_fw->size:0,
					(u8 *)fw_info->fw_fw->data, (u8 *)fw_info->patch_fw->data,
					(u8 *)fw_info->patch2_fw->data, fw_info->cal_fw?(u8 *)fw_info->cal_fw->data:NULL);
	if (error < 0) {
		RPU_INFO_FIRMWARE("rk915_copy_firmware failed!\n");
		goto exit_request;
	}

	ret = 0;

exit_request:
	if (fw_info->fw_fw)
		release_firmware(fw_info->fw_fw);
#ifdef ENABLE_FW_SPLIT
	if (fw_info->patch_fw)
		release_firmware(fw_info->patch_fw);
#endif
	if (fw_info->patch2_fw)
		release_firmware(fw_info->patch2_fw);
	if (fw_info->cal_fw)
		release_firmware(fw_info->cal_fw);

	return ret;
}
#endif

static int rk915_get_firmware_info(struct host_io_info *host, struct firmware_info *fw_info)
{
	int ret = 0;

	if (fw_info->fw_saved)
		return 0;

#ifdef FW_LOADER_FROM_USER

#if FW_LOADER_FROM_USER_OPEN
	ret = rk915_get_firmware_from_open(host, fw_info);
#else
	ret = rk915_get_firmware_from_request(host, fw_info);
#endif

#else
	RPU_INFO_FIRMWARE("firmware and patch buildin\n");
	fw_info->fw_size = sizeof(fwdata);
	fw_info->fw_data = fwdata;

	fw_info->patch_size = sizeof(rom_patch);
	fw_info->patch_data = rom_patch;
#endif

	return ret;
}

static int rk915_fw_write(struct hal_priv *priv,
						    void *fw, size_t fw_len)
{
    struct firmware_info *fw_info = &priv->io_info->firmware;
    unsigned char *src = (unsigned char *)fw;
    int size;
    int ret;
    int addr = 0;
    
    while (fw_len) {
        size = fw_len > MAX_BLOCK_DATA_SIZE ? MAX_BLOCK_DATA_SIZE:fw_len;
        memcpy(fw_info->block_data, src, size);
		ret = rk915_data_write(priv, addr, fw_info->block_data, size);
		if (ret) {
			RPU_ERROR_FIRMWARE("%s: write fw block(%d) failed (%d)\n",
                        __func__, addr, ret);
			return -1;
		}
        addr += size;
        src += size;
        fw_len -= size;
    }
    return 0;
}

static int rk915_fw_read(struct hal_priv *priv,
						    void *fw, size_t fw_len)
{
    struct firmware_info *fw_info = &priv->io_info->firmware;
    unsigned char *dst = (unsigned char *)fw;
    int size;
    int ret;
    int addr = 0;
    
    while (fw_len) {
        size = fw_len > MAX_BLOCK_DATA_SIZE ? MAX_BLOCK_DATA_SIZE:fw_len;
		ret = rk915_data_read(priv, addr, fw_info->block_data, size);
		if (ret) {
			RPU_ERROR_FIRMWARE("%s: read fw block(%d) failed (%d)\n",
                        __func__, addr, ret);
			return -1;
		}
        memcpy(dst, fw_info->block_data, size);
        addr += size;
        dst += size;
        fw_len -= size;
    }
    return 0;
}

//#define RK915_MEMORY_CHECK
#ifdef RK915_MEMORY_CHECK
static int err_bits[8];
static void rk915_mem_check_init(void)
{
	memset(err_bits, 0, 8*sizeof(int));
}

static void rk915_mem_check_stat(u8 val, u8 magic, int result, int total)
{
	int i;
	
	if (val == magic)
		goto print_result;

	val ^= magic;
	for (i = 0; i < 8; i++) {
		if ((val >> i) & 0x1)
			err_bits[i]++;
	}

print_result:	
	if (result) {
		RPU_ERROR_FIRMWARE("total error bytes: %d, bit %d %d %d %d %d %d %d %d\n",
				total, err_bits[0], err_bits[1], err_bits[2], err_bits[3],
				err_bits[4], err_bits[5], err_bits[6], err_bits[7]);
		rk915_mem_check_init();
	}
}

#include <linux/random.h>
static void rk915_mem_check(struct hal_priv *priv)
{
	int i, j, k, ret;
	u8 val;
	int size = 128*1024;
	u8 *buf_write = NULL, *buf_read = NULL;
	int fail = 0;
	int start_addr = 0x0000;
	int read_size = size;
	int block_size = 32768;
	int block_count, block_err;
	int randomX;

	buf_write = kzalloc(size, GFP_KERNEL);
	if (buf_write == NULL)
		goto mem_chk_end;
	buf_read = kzalloc(size, GFP_KERNEL);
	if (buf_read == NULL)
		goto mem_chk_end;

	for (j = 0; j < 3; j++) {
		if (j == 0) {
			val = 0;
		} else if (j == 2) {
			val = 0xAA;
		} else {
			val = 0xff;
		}

		if (val == 0xAA)
			RPU_INFO_FIRMWARE("start mem check: value(random) addr %x len %d block %d\n", start_addr, size, read_size);
		else
			RPU_INFO_FIRMWARE("start mem check: value(0x%02X) addr %x len %d block %d\n", val, start_addr, size, read_size);

		if (val == 0xAA) {
			u32 *fill = (u32 *)buf_write;
			for (i = 0; i < size/4; i++) {
				get_random_bytes(&randomX, 4);
				*fill++ = randomX;
			}
		} else {
			memset(buf_write, val, size);
		}
		ret = rk915_data_write(priv, start_addr, buf_write, size);
		if (ret) {
			RPU_ERROR_FIRMWARE("%s: download fw failed (%d)\n", __func__, ret);
			goto mem_chk_end;
		}

		for (k = 0; k < size/read_size; k++) {
			int offset = k * read_size;

			//RPU_INFO_FIRMWARE("read offset %x\n", offset);
			ret = rk915_data_read(priv, start_addr + offset, buf_read + offset, read_size);
			if (ret) {
				RPU_ERROR_FIRMWARE("%s: read fw failed (%d)\n", __func__, ret);
				goto mem_chk_end;
			}
		}

		fail = 0;
		block_count = 0;
		block_err = 0;
		for (i = 0; i < size; i++) {
			if ((i % block_size) == 0) {
				block_count = 0;
				block_err = 0;
				RPU_INFO_FIRMWARE("==========================\n");
			}
			block_count++;
			if (buf_read[i] != buf_write[i]) {
				RPU_ERROR_FIRMWARE("addr: %x, 0x%02x,  0x%02x\n",
							   i + start_addr, buf_write[i], buf_read[i]);
				fail++;
				block_err++;
			}
			if (val == 0xAA)
				rk915_mem_check_stat(buf_read[i], buf_write[i], block_count==block_size, block_err);
			else
				rk915_mem_check_stat(buf_read[i], val, block_count==block_size, block_err);
		}

		if (fail) {
			RPU_ERROR_FIRMWARE("%s: value(0x%02X) size(%d) failed\n", __func__, val, size);
		} else {
			RPU_INFO_FIRMWARE("%s: value(0x%02X) size(%d)  success\n", __func__, val, size);
		}
	}

mem_chk_end:
	if (buf_write)
		kfree(buf_write);
	if (buf_read)
		kfree(buf_read);
}
#endif

void rk915_mem_check2(struct hal_priv *priv, unsigned int addr, unsigned int len)
{
#ifdef FW_READ_BACK_CHECK 
	struct firmware_info *fw = &priv->io_info->firmware;
	int i, ret;
	int fail = 0;

	RPU_INFO_FIRMWARE("%s: addr %x %d\n", __func__, addr, len);
	memset(fw->fw_data_check, 0x55, MAX_FW_BUF_SIZE);
	ret = rk915_data_read(priv, addr, fw->fw_data_check, len);
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: read fw failed (%d)\n", __func__, ret);
		return;
	}

#ifndef RK915
	addr = 0;
#endif

	for (i = addr; i < len + addr; i++) {
		if (fw->fw_data_check[i] != fw->fw_data[i]) {
			fail++;
			RPU_ERROR_FIRMWARE("i: %x, 0x%02x,  0x%02x\n",
							   i, fw->fw_data[i], fw->fw_data_check[i]);
		}
	}

	if (fail) {
		RPU_ERROR_FIRMWARE("%s: failed %d\n", __func__, fail);
	} else {
		RPU_INFO_FIRMWARE("%s: success\n", __func__);
	}
#endif
}

extern bool m0_jtag_enable;

static int rk915_download(struct hal_priv *priv)
{
	int ret, state;
	struct firmware_info *fw = &priv->io_info->firmware;
	struct io_tx_ctrl_info info;
	int delay_ms = 10, count = 0, max_retry = 300;

	priv->during_fw_download = 1;

	if (likely(!m0_jtag_enable)) {
		struct io_cmd *scmd = (struct io_cmd *)fw->fw_start_data;
#ifdef FW_READ_BACK_CHECK  
        int check_size;
		int i;
		u8 val;
#endif
#ifdef RK915_MEMORY_CHECK
		rk915_mem_check(priv);
		return -1;
#endif

		// 1. download fw first
		RPU_INFO_FIRMWARE("%s: start download firmware size: %d\n", __func__, fw->fw_size);

		ret = rk915_fw_write(priv, fw->fw_data, fw->fw_size);
		if (ret) {
			RPU_ERROR_FIRMWARE("%s: download fw failed (%d)\n", __func__, ret);
			goto fail;
		}
#ifdef FW_READ_BACK_CHECK
		ret = rk915_fw_read(priv, fw->fw_data_check, fw->fw_size);
		if (ret) {
			RPU_ERROR_FIRMWARE("%s: read fw failed (%d)\n", __func__, ret);
			goto fail;
		}

		check_size = fw->fw_size;
#if 0//def RK915
		if (check_size > 32*1024)
			check_size = 32*1024;
#endif
		for (i = 0; i < check_size; i++) {
			if (fw->fw_data_check[i] != fw->fw_data[i]) {
				RPU_ERROR_FIRMWARE("i: %x, 0x%02x,  0x%02x\n",
							   i, fw->fw_data[i], fw->fw_data_check[i]);
			}
		}

		val = memcmp(fw->fw_data, fw->fw_data_check, check_size);
		if (val) {
			RPU_ERROR_FIRMWARE("%s: check downloaded fw failed\n", __func__);
			//ret = -1;
			//goto fail;
		} else {
			RPU_INFO_FIRMWARE("%s: check downloaded fw ok.\n", __func__);
		}
#endif
		scmd->id = IO_START_CMD_ID;
		scmd->length = 4;
		scmd->addr = 0;
		ret = rk915_data_write(priv, IO_START_CMD_ADDR, (void *)scmd, sizeof(struct io_cmd));
		if (ret) {
			RPU_ERROR_FIRMWARE("%s: start fw failed (%d)\n", __func__, ret);
			goto fail;
		}

		mdelay(20);
	}

	while (1) {
		state = rk915_readb(priv, IO_FW_STATE);
		if (state < 0) {
			ret = -1;
			goto fail;
		}
#ifdef SDIO_TXRX_STABILITY_TEST
		if (state >= WAIT_PATCH)
#else
		if (state == WAIT_PATCH)
#endif
			break;
		if (count++ > max_retry)
			break;
		mdelay(delay_ms);
		//if (net_ratelimit())
		//	RPU_INFO_FIRMWARE("wait fw ready (state = %d)\n", state);
	};
	if (count > max_retry) {
		RPU_INFO_FIRMWARE("%s: download firmware failed\n", __func__);
		ret = -1;
		goto fail;
	} else {
		RPU_INFO_FIRMWARE("%s: download firmware success\n", __func__);
	}

	mdelay(50);

#ifdef SDIO_TXRX_STABILITY_TEST
	ret = 0;
	goto fail;
#endif

#ifdef ENABLE_FW_SPLIT
	/* 2. download patch */
	RPU_DEBUG_FIRMWARE("%s: start download patch\n", __func__);

	memset(&info, 0, sizeof(struct io_tx_ctrl_info));
	info.type = IO_TX_PKT_PATCH;
	info.patch_len = fw->patch_size;

	ret = rk915_writeb(priv, IO_PATCH_LEN_L, (info.patch_len & 0x00ff));
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch rk915_writeb failed (%d)\n", __func__, ret);
		goto fail;
	}

	ret = rk915_writeb(priv, IO_PATCH_LEN_H, (info.patch_len & 0xff00) >> 8);
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch rk915_writeb failed (%d)\n", __func__, ret);
		goto fail;
	}

	ret = rk915_data_write(priv, IO_PATCH_ADDR, fw->patch_data, fw->patch_size);
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch failed (%d)\n", __func__, ret);
		goto fail;
	}

	count = 0;
	while (1) {
		state = rk915_readb(priv, IO_FW_STATE);
		if (state < 0) {
			ret = -1;
			goto fail;
		}
		if (state >= WAIT_PATCH2)
			break;
		if (count++ > max_retry)
			break;
		mdelay(delay_ms);
		//if (net_ratelimit())
		//	RPU_INFO_FIRMWARE("wait lpw ready (state = %d)\n", state);
	};
	if (count > max_retry) {
		RPU_INFO_FIRMWARE("%s: download patch failed\n", __func__);
		ret = -1;
		goto fail;
	} else {
		RPU_INFO_FIRMWARE("%s: download patch success\n", __func__);
	}
#endif

	/* 3. download patch2 */
	RPU_DEBUG_FIRMWARE("%s: start download patch2\n", __func__);

	memset(&info, 0, sizeof(struct io_tx_ctrl_info));
	info.type = IO_TX_PKT_PATCH;
	info.patch_len = fw->patch2_size;

	ret = rk915_writeb(priv, IO_PATCH_LEN_L, (info.patch_len & 0x00ff));
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch2 rk915_writeb failed (%d)\n", __func__, ret);
		goto fail;
	}

	ret = rk915_writeb(priv, IO_PATCH_LEN_H, (info.patch_len & 0xff00) >> 8);
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch2 rk915_writeb failed (%d)\n", __func__, ret);
		goto fail;
	}

	ret = rk915_data_write(priv, IO_PATCH_ADDR, fw->patch2_data, fw->patch2_size);
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch2 failed (%d)\n", __func__, ret);
		goto fail;
	}

	count = 0;
	while (1) {
		state = rk915_readb(priv, IO_FW_STATE);
		if (state < 0) {
			ret = -1;
			goto fail;
		}
		if (state >= M0_READY)
			break;
		if (count++ > max_retry)
			break;
		mdelay(delay_ms);
		//if (net_ratelimit())
		//	RPU_INFO_FIRMWARE("wait lpw ready (state = %d)\n", state);
	};
	if (count > max_retry) {
		RPU_INFO_FIRMWARE("%s: download patch2 failed\n", __func__);
		ret = -1;
		goto fail;
	} else {
		RPU_INFO_FIRMWARE("%s: download patch2 success\n", __func__);
	}	

	// clear patch len
	ret = rk915_writeb(priv, IO_PATCH_LEN_L, 0);
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch2 rk915_writeb failed (%d)\n", __func__, ret);
		goto fail;
	}

	ret = rk915_writeb(priv, IO_PATCH_LEN_H, 0);
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch2 rk915_writeb failed (%d)\n", __func__, ret);
		goto fail;
	}

fail:
	priv->during_fw_download = 0;
	return ret;
}

/* return 0 means fw download success, otherwise fail */
int rk915_download_firmware(struct hal_priv *priv)
{
	struct host_io_info *host = (struct host_io_info *)priv->io_info;
	struct firmware_info *fw_info;

#ifdef SKIP_DL_FW
	return 0;
#endif

	fw_info = &(priv->io_info->firmware);

	/* get info of firmware and rompatch */
	if (rk915_get_firmware_info(host, fw_info)) {
		RPU_ERROR_FIRMWARE("%s: get firmeware error!.\n", __func__);
		return -1;
	}

	fw_info->fw_saved = 1;

	return rk915_download(priv);
}

#if defined(LPW_RECOVERY_FROM_RPU)
int rk915_download_firmware_patch_only(struct hal_priv *priv)
{
	struct firmware_info *fw_info;
	struct io_tx_ctrl_info info;	
	int delay_ms = 10, count = 0, max_retry = 100;
	int ret = 0;
	int state;

	fw_info = &(priv->io_info->firmware);

	/* download patch2 */
	RPU_DEBUG_FIRMWARE("%s: start download patch2, patch2_size = %d\n", __func__, fw_info->patch2_size);

	memset(&info, 0, sizeof(struct io_tx_ctrl_info));
	info.type = IO_TX_PKT_PATCH;
	info.patch_len = fw_info->patch2_size;

	count = 0;
	
	while (1) 
	{
		state = rk915_readb(priv, IO_FW_STATE);
		if (state == WAIT_PATCH2)
			break;
		if (count++ > max_retry)
			break;
		mdelay(delay_ms);
		if (net_ratelimit())
			RPU_DEBUG_FIRMWARE("wait lpw changing state to WAIT_PATCH2 (state = %d), count=%d\n", state, count);
	};

	if (count >= max_retry) {
		rk915_signal_io_error(FW_ERR_LPW_RECOVERY);
		ret = -1;
		goto fail;
	}

	mdelay(50);
	
	ret = rk915_writeb(priv, IO_PATCH_LEN_L, (info.patch_len & 0x00ff));
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch2 rk915_writeb failed (%d)\n", __func__, ret);
		goto fail;
	}

	ret = rk915_writeb(priv, IO_PATCH_LEN_H, (info.patch_len & 0xff00) >> 8);
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch2 rk915_writeb failed (%d)\n", __func__, ret);
		goto fail;
	}

	ret = rk915_data_write(priv, IO_PATCH_ADDR, fw_info->patch2_data, fw_info->patch2_size);
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch2 failed (%d)\n", __func__, ret);
		goto fail;
	}

	count = 0;
	while (1) {
		state = rk915_readb(priv, IO_FW_STATE);
		if (state >= LPW_READY)
			break;
		if (count++ > max_retry)
			break;
		mdelay(delay_ms);
		//if (net_ratelimit())
		//	RPU_DEBUG_FIRMWARE("wait lpw ready (state = %d), count=%d\n", state, count);
	};
	if (count > max_retry) {
		RPU_INFO_FIRMWARE("%s: download patch2 failed\n", __func__);
		ret = -1;
		goto fail;
	} else {
		RPU_INFO_FIRMWARE("%s: download patch2 success\n", __func__);
	}		

	// clear patch len
	ret = rk915_writeb(priv, IO_PATCH_LEN_L, 0);
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch2 rk915_writeb failed (%d)\n", __func__, ret);
		goto fail;
	}

	ret = rk915_writeb(priv, IO_PATCH_LEN_H, 0);
	if (ret) {
		RPU_ERROR_FIRMWARE("%s: downlaod patch2 rk915_writeb failed (%d)\n", __func__, ret);
		goto fail;
	}

fail:	

	return ret;
}
#endif

