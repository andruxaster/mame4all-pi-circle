MAME4ALL для Raspberry Pi Zero / Zero W / Pi 1 без Linux
=========================================================

MAME4ALL-pi (MAME 0.37b5, ~2400 игр) собран как "голое" ядро на Circle:
загрузка за несколько секунд сразу в меню выбора игр.

1. SD-карта
   - Отформатируйте карту в FAT32, скопируйте на неё всё содержимое архива.
   - ROM-архивы игр (zip, формат MAME 0.37b5) положите в /mame4all/roms.
     В меню видны только игры, ROM которых есть на карте.
   - Вставьте карту, подключите HDMI, USB-клавиатуру или геймпад
     (к Zero - через переходник OTG) и питание.

2. Управление (как в MAME4ALL)
   Меню:      стрелки - выбор, Enter или Left Ctrl - запуск, Esc - выключить
   В игре:    5 / 6 - монета, 1 / 2 - старт, стрелки - джойстик,
              Left Ctrl, Left Alt, Space, Left Shift - кнопки,
              Tab - меню MAME, P - пауза, Esc - выход в список игр
   Геймпад:   крестовина/стик - направления, A B X Y - кнопки,
              Select - монета, Start - старт
   После Esc в меню игр систему можно выключать из розетки.

   Покерные автоматы (ampoker2 - American Poker II, magicrd2 - Magic Card II):
     Z X C V B - Hold 1-5, 1 - Deal/Start, 5 - монета,
     ampoker2: N - Bet/Red, M - Black, Q - Payout, 9 - ключ Operator,
               0 - ключ Supervisor, O - Door (дверца), 4 - сброс кредитов
     magicrd2: N - Clear/Take, 9 / 0 - Service 1 / 2
     Геймпад:  A B X Y LB - Hold 1-5, Start - Deal, Select - монета,
               RB - Bet (ampoker2), вниз - Payout / Take, влево - Black
   American Poker II при первом запуске (пустая NVRAM) пишет INIT MACHINE.
   Инициализация: нажать 9 (Operator вкл.), держать O, пока не появится
   страница счётчиков, затем снова 9 (Operator выкл.).

   Neo-Geo: нужны neogeo.zip (BIOS) и наборы игр из современного MAME.
   Есть Metal Slug 1-5 и X, Samurai Shodown 1-5 и V Special, KOF 94-98 и др.

3. Настройки
   cmdline.txt (одна строка, параметры через пробел):
     aspect=169       экран 16:9 (по умолчанию), картинка игры 4:3 по центру
     aspect=43        экран 4:3
     game=<имя>       сразу запускать игру без меню (например game=arabian)
     sounddev=sndpwm  звук через GPIO 18/19 вместо HDMI; sndnone - без звука
   Или: файл /mame4all/autostart.txt с именем игры - тоже автозапуск.
   /mame4all/mame.cfg - настройки MAME4ALL (частота звука, пропуск кадров и т.д.).

4. Что сохраняется
   Рекорды (hi), NVRAM игр, настройки кнопок (cfg) пишутся в /mame4all.

Лицензии: MAME4ALL / MAME - некоммерческая лицензия MAME (см. readme_mame4all.txt);
Circle и circle-stdlib - GPLv3; прошивка Raspberry Pi - LICENCE.broadcom.
ROM-файлы игр в комплект не входят.
