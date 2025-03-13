

# Kártyaolvaschó

Régi/új SEMhez készített ESP32 mikrokontrollerrel és RC522 kártyaolvasó modullal létrehozott beléptetőrendszer.
## Használati utasítás átlagos felhasználónak
Tedd a kártyádat a kártyaolvasó közelébe (<5cm). Amennyiben beolvasta a kártyádat, a státuszled 5 másodpercig be van kapcsolva. Ha a led villog, akkor hozzá vagy adva a rendszerhez és kinyílik mindkét mágneszár. Amennyiben konstans ég, akkor nem tudsz bemenni, mert nem vagy hozzáadva a rendszerhez. Újraindítani az EN gombbal megnyomásával lehetséges.
## Kártya hozzáadása/törlése
Olvasd be a mesterkulcsot, majd vedd el a kártyaolvasótól. Olvasd be a hozzáadni/törölni kívánt kártyát. Kész :)

Amennyiben mégsem szeretnél hozzáadni/törölni kártyát, de már be van olvasva a mesterkulcs, akkor húzz be egy random kártyát (ami nem a mesterkulcs), majd újra töröld ki/add hozzá. Szándékosan egyszerre csak 1 kártyát lehet hozzáadni/törölni.
## Advanced használati utasítás

UART-on lehet kommunikálni az ESP32-vel. Ehhez szükséged van egy USB UART [átalakítóra](https://www.hestore.hu/prod_10038506.html) és 3 db kenyértábla áthidaló vezetékre (breadboard kábelre :D ) A nyomtatott áramkörön van 3db szabad tüske: TX0, RX0 és GND néven. A lenti táblázat alapján kell bekötni

| ESP32          |  TX0     | RX0   | GND   |
|:-----          |:--------:|------:|------:|
| UART Converter |  RX0     | TX0   | GND   |

A kommunikációhoz szükség van egy szoftverre is. Windowson a [PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html) program ajánlott. Telepítés után nyissuk meg. Állítsuk be az alábbiakat:

Connection type: serial

Serial line: COM8

Speed (baudrate): 115200

Nyissuk meg az eszközkezelőt (device manager). Keressük ki a soros port fület, majd nyissuk le. Ha kihúzzuk az USB-s UART átalakítót, el fog tűnni az egyik elem és ha visszadugod, újra megjelenik ugyan azon a porton. Nálam ez COM8, de nálad ez lehet más. Ez alapján módosítsd a PuTTY configját, majd nyomj az open gombra. Ha mindent jól csináltál, akkor ha beírod az ablakba a list szavat, akkor megjelenik soronként 1 UID (kártya azonosítója).

work in progress

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
