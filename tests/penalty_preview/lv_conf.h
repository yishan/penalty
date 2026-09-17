#ifndef LV_CONF_H
#define LV_CONF_H

/* Same color depth, pool and enabled fonts as sdkconfig.defaults. The host
 * framebuffer is test-only; it is not linked into ESP32-C3 firmware. */
#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (24 * 1024)
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_MEM_INTEGRITY 1
#define LV_USE_SYSMON 0
#define LV_USE_PERF_MONITOR 0
#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0
#endif
