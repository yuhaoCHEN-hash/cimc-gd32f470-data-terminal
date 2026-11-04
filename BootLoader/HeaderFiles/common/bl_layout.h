#ifndef COMMON_BL_LAYOUT_H
#define COMMON_BL_LAYOUT_H

#include <stdint.h>

/*
 * BL 与 APP 共用的内部 Flash 分区布局.
 *
 * APP1 = 应用程序运行区，APP2 = OTA 固件暂存区
 * 复位后 BL 校验 APP2 内容，仅在 PENDING 时搬运到 APP1
 *  .
 */
#define BL_FLASH_BASE_ADDR          0x08000000UL
#define BL_FLASH_TOTAL_SIZE         0x00080000UL
#define BL_FLASH_END_ADDR           (BL_FLASH_BASE_ADDR + BL_FLASH_TOTAL_SIZE - 1UL)
#define BL_FLASH_PAGE_SIZE          0x00001000UL

/* BootLoader image. 竞赛要求 64K: 0x08000000 - 0x0800FFFF */
#define BL_BOOT_START_ADDR          0x08000000UL
#define BL_BOOT_SIZE                0x00010000UL
#define BL_BOOT_END_ADDR            (BL_BOOT_START_ADDR + BL_BOOT_SIZE - 1UL)

// 参数区 4K: 0x08010000 - 0x08010FFF
#define BL_PARAM_START_ADDR         0x08010000UL
#define BL_PARAM_SIZE               0x00001000UL

/* App 区 128K: 0x08011000 - 0x08030FFF */
#define BL_APP1_START_ADDR          0x08011000UL
#define BL_APP1_SIZE                0x00020000UL
#define BL_APP1_END_ADDR            (BL_APP1_START_ADDR + BL_APP1_SIZE - 1UL)

/* App 备份区 128K: 0x08031000 - 0x08050FFF */
#define BL_APP2_START_ADDR          0x08031000UL
#define BL_APP2_SIZE                0x00020000UL
#define BL_APP2_END_ADDR            (BL_APP2_START_ADDR + BL_APP2_SIZE - 1UL)

/* 固件暂存区 128K: 0x08051000 - 0x08070FFF */
#define BL_DATA_START_ADDR          0x08051000UL
#define BL_DATA_SIZE                0x00020000UL
#define BL_DATA_END_ADDR            (BL_DATA_START_ADDR + BL_DATA_SIZE - 1UL)

#endif /* COMMON_BL_LAYOUT_H */
