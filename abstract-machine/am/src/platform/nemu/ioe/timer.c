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

  uptime->us = ((uint64_t)h2 << 32) | l;
}

void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = *(volatile uint32_t *)(RTC_ADDR + 8);
  rtc->minute = *(volatile uint32_t *)(RTC_ADDR + 12);
  rtc->hour   = *(volatile uint32_t *)(RTC_ADDR + 16);
  
  uint32_t combined = *(volatile uint32_t *)(RTC_ADDR + 20);
  rtc->day    = combined & 0xff;
  rtc->month  = (combined >> 8) & 0xff;
  rtc->year   = (combined >> 16);
}