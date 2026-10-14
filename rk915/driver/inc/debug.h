/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <linux/time.h>

extern int rpu_debug;
extern int rpu_debug_level;

#define MODULE_TAG "RK915: "
#define DEBUG_TAG "DEBUG: "
#define INFO_TAG "INFO: "
#define ERROR_TAG "ERROR: "

#define RPU_DEBUG_MAIN(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_MAIN &&	\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)      \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_INFO_MAIN(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_MAIN &&	\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)      \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_ERROR_MAIN(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_MAIN &&	\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)      \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_DEBUG_UMACIF(fmt, ...)        \
do {                                         \
	if (rpu_debug & RPU_DEBUG_UMACIF &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)  \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);  \
} while (0)

#define RPU_INFO_UMACIF(fmt, ...)        \
do {                                         \
	if (rpu_debug & RPU_DEBUG_UMACIF &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)  \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);  \
} while (0)

#define RPU_ERROR_UMACIF(fmt, ...)        \
do {                                         \
	if (rpu_debug & RPU_DEBUG_UMACIF &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)  \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);  \
} while (0)

#define RPU_DEBUG_CRYPTO(fmt, ...)           \
do {                                           \
	if (rpu_debug & RPU_DEBUG_CRYPTO &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)     \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);    \
} while (0)

#define RPU_INFO_CRYPTO(fmt, ...)           \
do {                                           \
	if (rpu_debug & RPU_DEBUG_CRYPTO &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)     \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);    \
} while (0)

#define RPU_ERROR_CRYPTO(fmt, ...)           \
do {                                           \
	if (rpu_debug & RPU_DEBUG_CRYPTO &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)     \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);    \
} while (0)

#define RPU_DEBUG_VIF(fmt, ...)			\
do {                                             \
	if (rpu_debug & RPU_DEBUG_VIF &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)		  \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);      \
} while (0)

#define RPU_INFO_VIF(fmt, ...)			\
do {                                             \
	if (rpu_debug & RPU_DEBUG_VIF &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)		  \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);      \
} while (0)

#define RPU_ERROR_VIF(fmt, ...)			\
do {                                             \
	if (rpu_debug & RPU_DEBUG_VIF &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)		  \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);      \
} while (0)

#define RPU_DEBUG_TX(fmt, ...)			\
do {                                             \
	if (rpu_debug & RPU_DEBUG_TX &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)		  \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);      \
} while (0)

#define RPU_INFO_TX(fmt, ...)			\
do {                                             \
	if (rpu_debug & RPU_DEBUG_TX &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)		  \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);      \
} while (0)

#define RPU_ERROR_TX(fmt, ...)			\
do {                                             \
	if (rpu_debug & RPU_DEBUG_TX &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)		  \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);      \
} while (0)

#define RPU_DEBUG_SCAN(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_SCAN &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)      \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_INFO_SCAN(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_SCAN &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)      \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_ERROR_SCAN(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_SCAN &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)      \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_DEBUG_ROC(fmt, ...)            \
do {                                         \
	if (rpu_debug & RPU_DEBUG_ROC &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)      \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);  \
} while (0)

#define RPU_INFO_ROC(fmt, ...)            \
do {                                         \
	if (rpu_debug & RPU_DEBUG_ROC &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)      \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);  \
} while (0)

#define RPU_ERROR_ROC(fmt, ...)            \
do {                                         \
	if (rpu_debug & RPU_DEBUG_ROC &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)      \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);  \
} while (0)

#define RPU_DEBUG_TSMC(fmt, ...)             \
do {                                           \
	if (rpu_debug & RPU_DEBUG_TSMC &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)       \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);    \
} while (0)

#define RPU_INFO_TSMC(fmt, ...)             \
do {                                           \
	if (rpu_debug & RPU_DEBUG_TSMC &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)       \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);    \
} while (0)

#define RPU_ERROR_TSMC(fmt, ...)             \
do {                                           \
	if (rpu_debug & RPU_DEBUG_TSMC &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)       \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);    \
} while (0)

#define RPU_DEBUG_IF(fmt, ...)              \
do {                                          \
	if (rpu_debug & RPU_DEBUG_IF &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)        \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_INFO_IF(fmt, ...)              \
do {                                          \
	if (rpu_debug & RPU_DEBUG_IF &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)        \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_ERROR_IF(fmt, ...)              \
do {                                          \
	if (rpu_debug & RPU_DEBUG_IF &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)        \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_DEBUG_HAL(fmt, ...)                           \
do {							\
	if ((rpu_debug & RPU_DEBUG_HAL) /*&& net_ratelimit()*/	&&	\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG) \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);	 \
} while (0)

#define RPU_INFO_HAL(fmt, ...)                           \
do {							\
	if ((rpu_debug & RPU_DEBUG_HAL)	&&	\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO) \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);	 \
} while (0)

#define RPU_ERROR_HAL(fmt, ...)                           \
do {							\
	if ((rpu_debug & RPU_DEBUG_HAL)	&&	\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR) \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);	 \
} while (0)

#define RPU_DEBUG_DUMP_HAL(fmt, ...)                           \
do {							\
	if (rpu_debug & RPU_DEBUG_DUMP_HAL)			\
		print_hex_dump(KERN_DEBUG, fmt, ##__VA_ARGS__);	 \
} while (0)

#define DUMP_HAL (rpu_debug & RPU_DEBUG_DUMP_HAL)

#define RPU_DEBUG_DUMP_TX(fmt, ...)                           \
do {							\
	if (rpu_debug & RPU_DEBUG_DUMP_TX)			\
		print_hex_dump(KERN_DEBUG, "DUMP_TX", fmt, ##__VA_ARGS__);	 \
} while (0)

#define DUMP_TX (rpu_debug & RPU_DEBUG_DUMP_TX)

#define RPU_DEBUG_SDIO(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_SDIO &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)      \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_INFO_SDIO(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_SDIO &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)      \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_ERROR_SDIO(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_SDIO &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)      \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_DEBUG_P2P(fmt, ...)        \
do {                                         \
	if (rpu_debug & RPU_DEBUG_P2P &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)  \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);  \
} while (0)

#define RPU_INFO_P2P(fmt, ...)        \
do {                                         \
	if (rpu_debug & RPU_DEBUG_P2P &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)  \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);  \
} while (0)

#define RPU_ERROR_P2P(fmt, ...)        \
do {                                         \
	if (rpu_debug & RPU_DEBUG_P2P &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)  \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);  \
} while (0)

#define RPU_DEBUG_RX(fmt, ...)                            \
do {                                                        \
	if ((rpu_debug & RPU_DEBUG_RX) /*&& net_ratelimit()*/ &&	\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG) \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);                 \
} while (0)

#define RPU_INFO_RX(fmt, ...)                            \
do {                                                        \
	if ((rpu_debug & RPU_DEBUG_RX) &&	\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO) \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);                 \
} while (0)

#define RPU_ERROR_RX(fmt, ...)                            \
do {                                                        \
	if ((rpu_debug & RPU_DEBUG_RX) &&	\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR) \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);                 \
} while (0)

#define RPU_DEBUG_DUMP_RX(fmt, ...)                           \
do {                                                            \
	if (rpu_debug & RPU_DEBUG_DUMP_RX)                     \
		print_hex_dump(KERN_DEBUG, fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_INFO_FIRMWARE(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_FIRMWARE &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)      \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_ERROR_FIRMWARE(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_FIRMWARE &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)      \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_DEBUG_FIRMWARE(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_FIRMWARE &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)      \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);   \
} while (0)


#define RPU_INFO_HALIO(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_HALIO &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)      \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_ERROR_HALIO(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_HALIO &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)      \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_DEBUG_HALIO(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_HALIO &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)      \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);   \
} while (0)


#define DUMP_RX (rpu_debug & RPU_DEBUG_DUMP_RX)

#define RPU_INFO_DAPT(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_DAPT &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)      \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_ERROR_DAPT(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_DAPT &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)      \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_DEBUG_DAPT(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_DAPT &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)      \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_INFO_ROCOVERY(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_ROCOVERY &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_INFO)      \
		pr_info(MODULE_TAG INFO_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_ERROR_ROCOVERY(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_ROCOVERY &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_ERROR)      \
		pr_err(MODULE_TAG ERROR_TAG fmt, ##__VA_ARGS__);   \
} while (0)

#define RPU_DEBUG_ROCOVERY(fmt, ...)            \
do {                                          \
	if (rpu_debug & RPU_DEBUG_ROCOVERY &&		\
		rpu_debug_level & RPU_DEBUG_LEVEL_DEBUG)      \
		pr_info(MODULE_TAG DEBUG_TAG fmt, ##__VA_ARGS__);   \
} while (0)

enum rpu_debug {
	RPU_DEBUG_SCAN			= BIT(1),
	RPU_DEBUG_ROC			= BIT(2),
	RPU_DEBUG_TX			= BIT(3),
	RPU_DEBUG_MAIN			= BIT(4),
	RPU_DEBUG_IF			= BIT(5),
	RPU_DEBUG_UMACIF		= BIT(6),
	RPU_DEBUG_RX			= BIT(7),
	RPU_DEBUG_HAL			= BIT(8),
	RPU_DEBUG_CRYPTO		= BIT(9),
	RPU_DEBUG_DUMP_RX		= BIT(10),
	RPU_DEBUG_DUMP_HAL		= BIT(11),
	RPU_DEBUG_TSMC			= BIT(12),
	RPU_DEBUG_P2P			= BIT(13),
	RPU_DEBUG_VIF			= BIT(14),
	RPU_DEBUG_DUMP_TX		= BIT(15),
	RPU_DEBUG_SDIO			= BIT(16),
	RPU_DEBUG_FIRMWARE		= BIT(17),
	RPU_DEBUG_HALIO			= BIT(18),
	RPU_DEBUG_DAPT			= BIT(19),
	RPU_DEBUG_ROCOVERY		= BIT(20),
};

enum rpu_debug_level {
	RPU_DEBUG_LEVEL_ERROR			= BIT(1),
	RPU_DEBUG_LEVEL_INFO			= BIT(2),
	RPU_DEBUG_LEVEL_DEBUG			= BIT(3),
};

#define VIF_INDEX_TO_INTERFACE_NAME(x)					\
	((x==0) ? "p2p0":"wlan0")

#define START_PROFILE_LOCAL								\
	struct timeval start_time, end_time;				\
	do_gettimeofday(&start_time);

#define END_PROFILE_LOCAL								\
	do_gettimeofday(&end_time);							\
	pr_info("%s: use %ld (us)\n", __func__, 				\
		((end_time.tv_sec & 0xFFF) * 1000000 + end_time.tv_usec) -	\
		((start_time.tv_sec & 0xFFF) * 1000000 + start_time.tv_usec));

#define INIT_GET_SPEND_TIME(start_time, end_time)		\
	struct timeval start_time, end_time;

#define START_GET_SPEND_TIME(start_time, end_time)		\
	do_gettimeofday(&start_time);

#define END_GET_SPEND_TIME(start_time, end_time)		\
	do_gettimeofday(&end_time);

#define GET_SPEND_TIME_US(start_time, end_time)			\
	(((end_time.tv_sec & 0xFFF) * 1000000 + end_time.tv_usec) -	\
	((start_time.tv_sec & 0xFFF) * 1000000 + start_time.tv_usec))

void convert_cmd_to_str(int id, char *str);
void convert_event_to_str(int id, char *str);

/* enable tx rx stability test */
//#define SDIO_TXRX_STABILITY_TEST

//#define DUMP_MORE_DEBUG_INFO
//#define DUMP_TX_RX_FRAME_INFO

#endif /* _DEBUG_H_ */
