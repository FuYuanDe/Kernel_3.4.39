#ifndef _KERN_WATCHDOG_H
#define _KERN_WATCHDOG_H

void kern_watchdog_enable(void);
void wdt_del_timer(void);
void kern_watchdog_disable(void);
void wdt_del_timer_and_disable(void);
#endif
