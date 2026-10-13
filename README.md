Proyecto final MT7002 Sensores y Actuadores, Instituto Tecnológico de Costa Rica, II Semestre 2026.
Integrantes: [Fátima Cárdenas Obando, Aaron Granados Fonseca, Dilana Jiménez Solano]

## Hardware
- Elechouse NFC Module V3 (NXP PN532), modo UART/HSU (DIP 1 = OFF, 2 = OFF)
- ESP32-WROOM-32

| PN532 | ESP32  |
|-------|--------|
| GND   | GND    |
| VCC   | 3V3    |
| TXD   | GPIO21 |
| RXD   | GPIO22 |

## Software
- PlatformIO (framework Arduino)
- Librería Elechouse PN532: https://github.com/elechouse/PN532
  Copiar las carpetas `PN532` y `PN532_HSU` en `lib/`.

## Uso
Monitor serie a 115200 baud. Comandos:
- `f` verificar conexión
- `l` lectura continua (`s` detiene)
- `e` prueba de exactitud (100 lecturas)
- `r <cm>` prueba de rango (20 intentos)
- `t` tiempo de respuesta (50 mediciones)
- `p <uid>` cambiar la credencial patrón
