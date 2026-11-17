#ifndef RM839_PLATFORM_H
#define RM839_PLATFORM_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
/* Core entry points remain specific to RM-839 02.33; see docs/abi.json.
 * Audio uses the SDK resolver and does not add fixed entry points here. */
void *fw_os_block_alloc_no_wait(unsigned); void fw_os_block_dealloc(void *);
uint32_t fw_os_current_time_read_in_ms(void); unsigned fw_os_task_own_id_read(void);
unsigned fw_msg_count(void); uintptr_t fw_os_msg_receive(void);
void fw_CM_ComMgr_ProcessPhonetMessage(void *); int fw_CM_ComMgr_ProcessInternalQueue(void);
void fw_CM_ComMgr_ScanISISubscriptions(void); void fw_CM_Timer_Timeout(unsigned); int fw_vm_timer(unsigned);
int fw_app_id(void); int fw_event_pop(int,int *,int *,int *);
int fw_filesys_open(const uint16_t *,unsigned,unsigned); int fw_filesys_close(int);
int fw_filesys_read(int,void *,unsigned); int fw_filesys_write(int,const void *,unsigned);
int fw_filesys_lseek(int,int,int); int fw_filesys_unlink(const uint16_t *); int fw_filesys_mkdir(const uint16_t *,unsigned);
void fw_debug_timer(void);
void *fw_gdi_wrap_pixels(int *error,unsigned format,int width,unsigned height,unsigned flags,int stride,void *pixels);
void fw_gdi_free_context(void *);
/* Final adjustments add to the destination origin and subtract from extent.
 * Use zero for an in-place copy from the full-size framebuffer. */
int fw_ui_refresh(int x,int y,int width,int height,void *context,int adjust_x,int adjust_y);
void s40_cleanup(void); void s40_set_root(const char *); const char *s40_get_root(void);
void s40_set_log(FILE *); void s40_log_close(void); void s40_pump(void);
void s40_finish(int) __attribute__((noreturn));
extern uint32_t *s40_result;
#endif
