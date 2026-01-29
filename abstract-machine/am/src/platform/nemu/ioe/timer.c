#include <am.h>
#include <nemu.h>

void __am_timer_init() {
}

void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
uint32_t h1, h2, l;
do {
    h1 = *(volatile uint32_t *)(RTC_ADDR + 4); 
    l  = *(volatile uint32_t *)(RTC_ADDR);     
    h2 = *(volatile uint32_t *)(RTC_ADDR + 4); 
} while (h1 != h2); //防止出现进位

uint64_t uptime = ((uint64_t)h2 << 32) | l;
}

void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour   = 0;
  rtc->day    = 0;
  rtc->month  = 0;
  rtc->year   = 1900;
}
