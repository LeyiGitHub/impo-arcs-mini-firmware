/* SPDX-License-Identifier: MIT */
#include "sdk_version.h"

#if SDK_VERSION_NUMBER == SDK_VERSION(0, 1, 8) && CONFIG_BOOT_WITH_WATCHDOG
#include <stdint.h>
#include <stdio.h>
#include "sys_init.h"
#include "arcs_ap.h"

#if CONFIG_WATCHDOG_ENABLE
#error "Impo supervises wdt0; the legacy boot/AP watchdog must be disabled"
#endif
extern int boot_watchdog_init(void);
extern int boot_watchdog_enable(int enabled);
static uint32_t s_boot_wdt_before, s_boot_wdt_after;

static int impo_arcs_boot_watchdog_handoff(void)
{
    /* ListenAI 0.1.6 soc_init did this even with WATCHDOG_ENABLE=n.
     * 0.1.8 removed that handoff. Boot 1.1.9 still leaves AP WDT armed;
     * take it over before devices/IPC, while Impo retains its own wdt0
     * hardware watchdog and task supervision in plat_watchdog.c. */
    s_boot_wdt_before = ((WDT_RegDef *)IP_AP_WDT)->REG_CTRL.all;
    int ret = boot_watchdog_init();
    if (ret == 0) {
        ret = boot_watchdog_enable(0);
    }
    s_boot_wdt_after = ((WDT_RegDef *)IP_AP_WDT)->REG_CTRL.all;
    return ret;
}
SYS_INIT(impo_arcs_boot_watchdog_handoff, SYS_INIT_LEVEL_PRE_SYSTEM_INIT,
         SYS_INIT_SUB_PRIORITY_MIN);

static int impo_arcs_boot_watchdog_report(void)
{
    printf("Boot AP watchdog handoff: before=0x%08lx after=0x%08lx enabled=%u\n",
           (unsigned long)s_boot_wdt_before, (unsigned long)s_boot_wdt_after,
           (unsigned)((s_boot_wdt_after & WDT_CTRL_EN_Msk) != 0));
    return 0;
}
SYS_INIT(impo_arcs_boot_watchdog_report, SYS_INIT_LEVEL_PRE_APPLICATION,
         SYS_INIT_SUB_PRIORITY_LAST);
#endif
