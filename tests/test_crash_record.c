/* Exercise the real reset-record lifecycle without resetting hardware. */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#define portTICK_PERIOD_MS 1
static char logs[4096];
static void (*exception_hook)(void);
static uint32_t test_csr_read(unsigned csr) { static const uint32_t values[]={2,0x30001234,0x44,5};return values[csr]; }
uint64_t plat_uptime_ms(void) { return 1234; }
TickType_t xTaskGetTickCountFromISR(void) { assert(!"exception hook must not call ISR APIs");return 5678; }
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (void *)1; }
const char *pcTaskGetName(TaskHandle_t task) { (void)task;return "link_test"; }
void Exception_User_Register_EXC(unsigned long handler) { exception_hook=(void (*)(void))handler; }
void test_log(const char *tag,const char *fmt,...) {
 (void)tag;size_t offset=strlen(logs);va_list ap;va_start(ap,fmt);
 vsnprintf(logs+offset,sizeof(logs)-offset,fmt,ap);va_end(ap);
}
#include "../upstream/arcs/platform/arcs/crash_log.c"
int main(void)
{
    memset(&s_rec,0,sizeof(s_rec));plat_crash_init();assert(exception_hook && !s_previous_valid);
    plat_crash_report_previous();assert(strstr(logs,"reset_record valid=0"));
    exception_hook();assert(s_rec.check==rec_check(&s_rec));
    plat_crash_init();assert(s_previous_valid && !s_rec.magic);
    assert(s_previous.mcause==2 && s_previous.mepc==0x30001234 && s_previous.uptime_ms==0);
    assert(!strcmp(s_previous.task,"link_test"));
    plat_crash_boot_stage("new boot");plat_crash_boot_done();
    logs[0]=0;plat_crash_report_previous();assert(strstr(logs,"cause=00000002") && strstr(logs,"mepc=30001234"));
    /* Startup consumed the noinit record, but a later USB query can still read it. */
    assert(s_previous.mepc==0x30001234);
    plat_crash_note_stall("network","recv",9999);plat_crash_init();
    assert(s_previous_valid && s_previous.mcause==MCAUSE_STALL && s_previous.mtval==9999);
    assert(!strcmp(s_previous.where,"recv"));
    s_rec.magic=CRASH_MAGIC;s_rec.check=0;plat_crash_init();assert(!s_previous_valid);
    logs[0]=0;plat_crash_report_previous();assert(!strstr(logs,"reset_pc"));
    puts("Reset record: late USB query retains prior trap/stall, invalid records never exposed");
}
