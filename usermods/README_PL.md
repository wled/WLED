# PIR Dual Lock - WLED 0.16.0.1 - GLEDOPTO GL-C-310WL

## Polaczenie

- GPIO 33 -> PIR 1 przez styk przekaznika do GND
- GPIO 12 -> PIR 2 przez styk przekaznika do GND
- GPIO 2 i GPIO 16 pozostaja dla LED-ow

Czujnik jest aktywny stanem LOW:

- brak ruchu / styk otwarty -> HIGH
- ruch / styk zwarty do GND -> LOW

W programie uzywany jest `INPUT_PULLUP`.

## Dzialanie

1. PIR 1 lub PIR 2 wykrywa ruch.
2. Reagujemy tylko na przejscie HIGH -> LOW.
3. Uruchamiany jest preset przypisany do tego PIR-a.
4. Oba PIR-y zostaja zablokowane na czas przypisany do PIR-a, ktory wygral.
5. Po uplywie czasu oba PIR-y ponownie dzialaja.
6. Jezeli sygnal LOW trwa okolo 30 sekund, preset nie uruchomi sie ponownie w tym czasie.

## Ustawienia

W `Config -> Usermods -> PIR Dual Lock`:

- PIR1 Preset - 0..250, gdzie 0 wylacza wyzwalanie presetu PIR1
- PIR1 LockSec - 0..3600 sekund
- PIR2 Preset - 0..250, gdzie 0 wylacza wyzwalanie presetu PIR2
- PIR2 LockSec - 0..3600 sekund

GPIO 33 i 12 sa stale i celowo nie sa edytowalne, aby nie mozna bylo przypadkiem zajac GPIO2 lub GPIO16, ktore sa uzywane przez LED-y.

## Kompilacja

W katalogu glownym projektu:

`pio run -e esp32dev_pir`

Po kompilacji szukaj:

`build_output/firmware/ESP32.bin`

Nazwa moze zalezec od skryptow builda WLED. Najprosciej uzyc pliku wynikowego wskazanego przez PlatformIO.

## Wazne przed pierwszym uruchomieniem

Jesli GPIO12 lub GPIO33 byly wczesniej skonfigurowane w WLED jako zwykle Button/PIR Sensor, usun ich konfiguracje z `Config -> LED Preferences -> Button` tak, aby te piny nie byly rownoczesnie obslugiwane przez standardowy system przyciskow.
