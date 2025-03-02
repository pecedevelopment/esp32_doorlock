Beléptetőrendszer

dokumentáció hamarosan

ha ilyesmi az error üzenet, majd újrabootol az esp:
Guru Meditation Error: Core  0 panic'ed (LoadProhibited). Exception was unhandled.

akkor törölni kell az sdkconfig fájlt és az sdkconfig.default-ba beleírni a következőket:

 CONFIG_ESP_MAIN_TASK_STACK_SIZE=16000
 CONFIG_ESP_IPC_TASK_STACK_SIZE=4096
 CONFIG_ESP_TIMER_TASK_STACK_SIZE=4584
 CONFIG_FREERTOS_IDLE_TASK_STACKSIZE=2536
 CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=3048
 CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y

 mentsük el és mehet a build flash. nálam ilyenkor panaszkodott a flash méretére, mert a 4MB-ról resetelődött 2-re és a lemezen 2.1MB paríciót foglaltam le. ezt az idf.py menuconfig->serial flasher config->flash size helyen tudjuk állítani