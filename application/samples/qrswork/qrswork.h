#include "common_def.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "app_init.h"
#include "lib/led.h"
#include "lib/rgb.h"
#include "lib/ky.h"


void service(void);
void init_task(void *arg);
void heart_detect_task(void *arg);