# MAME4ALL-pi на Circle (без Linux)

[MAME4ALL-pi](https://github.com/squidrpi/mame4all-pi) (MAME 0.37b5, около 2400 игр) собран как «голое» ядро для Raspberry Pi Zero / Zero W / Pi 1 на [Circle](https://github.com/rsta2/circle) и [circle-stdlib](https://github.com/smuehlst/circle-stdlib). Linux не нужен: питание включается, и через несколько секунд открывается меню выбора игр.

Что добавлено к оригинальному MAME4ALL:

- слой ОС `mame4all-pi/src/circle` вместо SDL/Linux. Видео идёт через фреймбуфер с аппаратным масштабированием GPU, звук через HDMI или PWM, есть USB-клавиатура, USB-геймпады и FAT на SD-карте;
- ядро `kernel/`: класс `CKernel`, функции-переходники `cg_*` между MAME и Circle, автозапуск игры (`game=` в `cmdline.txt` или файл `/mame4all/autostart.txt`);
- новые драйверы, перенесённые из современного MAME на API 0.37b5:
  - `ampoker2`: American Poker II (Novomatic, 1990), Z80 и AY-3-8910;
  - `magicrd2`: Magic Card II (Impera, 1996), железо Fun World: R65C02, MC6845, 2× PIA 6821, AY-3-8910.

## Сборка

Нужны `arm-none-eabi-gcc` 13.x (Arm GNU Toolchain с newlib), `make`, `git`, `texinfo`, `wget`, `zip`.

```sh
git clone --recursive https://github.com/andruxaster/mame4all-pi-circle
cd mame4all-pi-circle
./build.sh
```

`build.sh` выполняет пять шагов:

1. настраивает и собирает circle-stdlib (`./configure -r 1 --kernel-max-size 48`);
2. собирает `mame4all-pi/libmame.a` (`make -f Makefile.circle`);
3. собирает `kernel/kernel.img`;
4. скачивает прошивку Raspberry Pi;
5. складывает всё для SD-карты в `./sd` и `mame4all_pi_sd.zip`.

Чтобы пересобрать после правок в драйверах, запустите:

```sh
make -C mame4all-pi -f Makefile.circle -j4
rm -f kernel/kernel.img && make -C kernel
```

## SD-карта

Скопируйте содержимое `sd/` на карту FAT32. ROM-архивы в формате MAME 0.37b5 положите в `/mame4all/roms`; в меню видны только те игры, ROM которых лежат на карте. Подробности про управление и настройки — в `boot/README.txt`, он же `README.txt` на карте.

Наборы ROM для новых драйверов (только родительские наборы):

| Игра | zip | Файлы |
|------|-----|-------|
| American Poker II | `ampoker2.zip` | poker9.002, poker9.003, poker9.028, 82s147an.u48 |
| Magic Card II (Bulgarian) | `magicrd2.zip` | magicard.004, magicard.01, mc2gr1.bin, mc2gr2.bin, mc2-82s147.bin, magicrd2_nvram.bin |

American Poker II при первом запуске (NVRAM пустая) показывает `INIT MACHINE`. Чтобы проинициализировать автомат:

1. нажмите 9 — ключ Operator включится;
2. держите O (дверца), пока не появится страница счётчиков;
3. снова нажмите 9 — ключ Operator выключится.

### Управление в покерных играх

| | Клавиатура | Геймпад |
|---|---|---|
| Hold 1–5 | Z X C V B | A B X Y LB |
| Deal / Start | 1 | Start |
| Монета | 5 | Select |
| Bet / Red (ampoker2) | N | RB |
| Black (ampoker2) | M | влево |
| Payout (ampoker2) / Clear-Take (magicrd2) | Q / N | вниз |
| Operator / Supervisor (ampoker2) | 9 / 0 (переключатели) | — |
| Door (ampoker2) | O | — |
| Service 1 / 2 (magicrd2) | 9 / 0 | — |

Лампы, счётчики, хоппер не эмулируются.

## Структура

```
mame4all-pi/          исходники MAME4ALL-pi (первый коммит — оригинал squidrpi @ bf71c5f)
  Makefile.circle     сборка libmame.a под Circle
  src/circle/         OSD-слой для Circle
  src/drivers/ampoker2.cpp, funworld.cpp   новые драйверы
kernel/               ядро Circle (main.cpp, kernel.cpp/.h, Makefile)
boot/                 config.txt, cmdline.txt, README.txt, mame.cfg для SD-карты
circle-stdlib/        подмодуль (Circle + newlib)
build.sh              полная сборка и сборка SD-карты
```

## Лицензии

- MAME4ALL и MAME 0.37b5 — некоммерческая лицензия MAME (см. `mame4all-pi/readme.txt`).
- Драйверы ampoker2 и funworld сделаны по драйверам современного MAME (BSD-3-Clause, авторы Roberto Fresca и Peter Ferrie).
- Circle и circle-stdlib — GPLv3. Прошивка Raspberry Pi — LICENCE.broadcom.
- ROM-файлы игр в репозиторий не входят.
