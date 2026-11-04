#ifndef BL_CMD_HANDLER_H
#define BL_CMD_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "frame_parser.h"

/* BL-specific command words */
#define BL_CMD_HEARTBEAT_RESP   0x8888
#define BL_CMD_UPGRADE_PREPARE  0x0502
#define BL_CMD_UPGRADE_EXECUTE  0x0503

/* State of the upgrade reception */
typedef enum {
    BL_UPGRADE_STATE_IDLE = 0,
    BL_UPGRADE_STATE_WAITING,        /* countdown active, waiting for 0x0502 */
    BL_UPGRADE_STATE_RECEIVING,      /* receiving raw bin data */
    BL_UPGRADE_STATE_RECEIVED,       /* bin fully received, waiting for 0x0503 */
    BL_UPGRADE_STATE_COMPLETE        /* upgrade done */
} bl_upgrade_state_t;

void bl_cmd_handler_init(uint16_t device_id);
uint32_t bl_cmd_handler_process(const protocol_frame_t *req, char *out_ascii);
bl_upgrade_state_t bl_cmd_handler_get_state(void);
void bl_cmd_handler_set_state(bl_upgrade_state_t state);

/* Called to feed raw binary firmware bytes */
void bl_firmware_feed(const uint8_t *data, uint32_t len);
bool bl_firmware_is_complete(void);
bool bl_firmware_check_valid(void);
uint32_t bl_firmware_get_size(void);
uint32_t bl_firmware_get_crc32(void);
void bl_firmware_reset(void);
bool bl_firmware_validate_and_copy(void);

#endif
