# Temperature Alarm with DS18B20 and LCD on AVR (Pure C)

A minimalist temperature alarm project using **ATmega328P (AVR)** in **pure C** (no Arduino libraries), featuring:
- **DS18B20 temperature sensor**
- **HD44780-based 16×2 LCD (4-bit mode)**
- **Active 5 V buzzer**
- Clean and procedural code, ideal for learning and bare-metal AVR programming

---

## Pin Mapping (Arduino UNO Labels)

| Component    | Arduino Pin | AVR Port.Pin |
|--------------|-------------|--------------|
| DS18B20      | D8          | PB0          |
| Buzzer       | D9          | PB1          |
| LCD RS       | D7          | PD7          |
| LCD E        | D6          | PD6          |
| LCD D4       | D5          | PD5          |
| LCD D5       | D4          | PD4          |
| LCD D6       | D3          | PD3          |
| LCD D7       | D2          | PD2          |
| LCD VO (contrast) | –     | potentiometer between 5 V and GND |

> Please ensure you have a 10 kΩ pull-up resistor between DS18B20’s DATA line and 5 V.

---

## Features

- Displays the current temperature in °C on the LCD.
- Triggers a buzzer alarm when temperature exceeds **28 °C**.
- Detects sensor absence and signals it (both display and buzzer).
- Fully implemented in C using AVR-GCC (no C++, no Arduino abstractions).

---

## Build & Upload (macOS / Linux)

Ensure you have `avr-gcc`, `avr-libc`, and `avrdude` installed (via Homebrew or system package manager):

```bash
brew tap osx-cross/avr
brew install avr-gcc avrdude
To compile and upload:

bash
Skopiuj kod
# Compile
avr-gcc -mmcu=atmega328p -DF_CPU=16000000UL -Os main.c -o main.elf

# Generate HEX file
avr-objcopy -O ihex -R .eeprom main.elf main.hex

# Upload via Arduino bootloader (Uno CH340)
avrdude -V -patmega328p -c arduino -P /dev/tty.usbserial-1130 -b115200 -U flash:w:main.hex
Replace /dev/tty.usbserial-1130 with your actual device port if different.

How it Works
The LCD is initialized in 4-bit mode, and the first line displays "Temperature:".

The DS18B20 is polled for temperature using the 1-Wire protocol.

Measured temperature is shown on the second LCD line (formatted as xx.x°C).

If temperature ≥ 28.0 °C, the buzzer sounds. If sensor is missing, 'No sensor' appears and buzzer triggers as error.

Customization
Change threshold: Modify PROG in main.c to your desired temperature limit.

Add hysteresis: To prevent buzzer chatter around the threshold.

Battery/low-power mode: Implement sleep modes in the main loop to reduce power consumption.

Project Structure
graphql
Skopiuj kod
├── main.c          # Complete AVR C firmware
├── main.elf        # Compiled ELF file (optional)
├── main.hex        # Uploadable HEX file
└── README.md       # This documentation file
Learn More
AVR-GCC programmer's manual: https://www.nongnu.org/avr-libc/user-manual/index.html

DS18B20 datasheet and 1-Wire protocol

HD44780 LCD interfacing guides

