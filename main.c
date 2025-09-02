/*
 * Temperature alarm with DS18B20 + LCD1602 + Active Buzzer
 * Target: ATmega328P (Arduino UNO compatible)
 * Language: Pure C (AVR-GCC)
 * Author: Krzysztof Strzelecki
 *
 * Pin mapping:
 *   DS18B20 -> D8 (PB0) + 10k pull-up to VCC
 *   Buzzer  -> D9 (PB1)
 *   LCD 16x2 (HD44780 in 4-bit mode):
 *       RS -> D7 (PD7)
 *       E  -> D6 (PD6)
 *       D4 -> D5 (PD5)
 *       D5 -> D4 (PD4)
 *       D6 -> D3 (PD3)
 *       D7 -> D2 (PD2)
 *       RW -> GND, VSS -> GND, VDD -> +5V, VO -> potentiometer
 *       LED+ -> +5V (with resistor if needed), LED- -> GND
 */

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdint.h>

// ===============================
// Pin definitions
// ===============================
#define DS18B20_PIN   PB0   // Arduino D8
#define BUZZER_PIN    PB1   // Arduino D9

#define LCD_RS        PD7   // Arduino D7
#define LCD_E         PD6   // Arduino D6
#define LCD_D4        PD5   // Arduino D5
#define LCD_D5        PD4   // Arduino D4
#define LCD_D6        PD3   // Arduino D3
#define LCD_D7        PD2   // Arduino D2

// ===============================
// Pin helper macros
// ===============================
#define SET_OUTPUT(ddr, pin)   (ddr |= (1 << pin))
#define SET_INPUT(ddr, pin)    (ddr &= ~(1 << pin))
#define SET_HIGH(port, pin)    (port |= (1 << pin))
#define SET_LOW(port, pin)     (port &= ~(1 << pin))
#define READ_PIN(pinreg, pin)  (pinreg & (1 << pin))

// ===============================
// LCD (HD44780) – 4-bit mode
// ===============================
static void lcd_pulse_enable(void) {
    SET_HIGH(PORTD, LCD_E);
    _delay_us(1);
    SET_LOW(PORTD, LCD_E);
    _delay_us(100);
}

static void lcd_send_nibble(uint8_t nibble) {
    if (nibble & 0x01) SET_HIGH(PORTD, LCD_D4); else SET_LOW(PORTD, LCD_D4);
    if (nibble & 0x02) SET_HIGH(PORTD, LCD_D5); else SET_LOW(PORTD, LCD_D5);
    if (nibble & 0x04) SET_HIGH(PORTD, LCD_D6); else SET_LOW(PORTD, LCD_D6);
    if (nibble & 0x08) SET_HIGH(PORTD, LCD_D7); else SET_LOW(PORTD, LCD_D7);
    lcd_pulse_enable();
}

static void lcd_command(uint8_t cmd) {
    SET_LOW(PORTD, LCD_RS);
    lcd_send_nibble(cmd >> 4);
    lcd_send_nibble(cmd & 0x0F);
    _delay_ms(2);
}

static void lcd_data(uint8_t data) {
    SET_HIGH(PORTD, LCD_RS);
    lcd_send_nibble(data >> 4);
    lcd_send_nibble(data & 0x0F);
    _delay_ms(2);
}

static void lcd_init(void) {
    // Configure LCD pins as output
    SET_OUTPUT(DDRD, LCD_RS);
    SET_OUTPUT(DDRD, LCD_E);
    SET_OUTPUT(DDRD, LCD_D4);
    SET_OUTPUT(DDRD, LCD_D5);
    SET_OUTPUT(DDRD, LCD_D6);
    SET_OUTPUT(DDRD, LCD_D7);

    _delay_ms(50); // LCD startup time

    // Initialization sequence for 4-bit mode
    lcd_send_nibble(0x03);
    _delay_ms(5);
    lcd_send_nibble(0x03);
    _delay_us(100);
    lcd_send_nibble(0x03);
    lcd_send_nibble(0x02);

    lcd_command(0x28); // 4-bit, 2-line mode
    lcd_command(0x0C); // Display on, cursor off
    lcd_command(0x06); // Auto-increment cursor
    lcd_command(0x01); // Clear display
    _delay_ms(2);
}

static void lcd_print(const char *str) {
    while (*str) lcd_data(*str++);
}

// ===============================
// DS18B20 – 1-Wire protocol on PB0 (Arduino D8)
// ===============================
#define DQ_DDR   DDRB
#define DQ_PORT  PORTB
#define DQ_PINR  PINB
#define DQ_BIT   PB0

static inline void dq_as_output(void){ DQ_DDR  |=  (1<<DQ_BIT); }
static inline void dq_as_input_pullup(void){ DQ_DDR &= ~(1<<DQ_BIT); DQ_PORT |= (1<<DQ_BIT); }
static inline void dq_low(void){ DQ_PORT &= ~(1<<DQ_BIT); }
static inline uint8_t dq_read(void){ return (DQ_PINR & (1<<DQ_BIT)) ? 1 : 0; }

// Send reset pulse and check for presence
static uint8_t onewire_reset(void){
    uint8_t presence;
    dq_as_output(); dq_low(); _delay_us(480);
    dq_as_input_pullup(); _delay_us(70);
    presence = (dq_read()==0);
    _delay_us(410);
    return presence;
}

// Write single bit
static void onewire_write_bit(uint8_t b){
    dq_as_output(); dq_low();
    if(b){
        _delay_us(6);
        dq_as_input_pullup();
        _delay_us(64);
    }else{
        _delay_us(60);
        dq_as_input_pullup();
        _delay_us(10);
    }
}

// Read single bit
static uint8_t onewire_read_bit(void){
    uint8_t r;
    dq_as_output(); dq_low(); _delay_us(6);
    dq_as_input_pullup(); _delay_us(9);
    r = dq_read();
    _delay_us(55);
    return r;
}

// Write one byte
static void onewire_write_byte(uint8_t v){
    for(uint8_t i=0;i<8;i++){ onewire_write_bit(v & 1); v >>= 1; }
}

// Read one byte
static uint8_t onewire_read_byte(void){
    uint8_t v=0;
    for(uint8_t i=0;i<8;i++){
        v >>= 1;
        if(onewire_read_bit()) v |= 0x80;
    }
    return v;
}

// Get temperature from DS18B20
static float ds18b20_get_temp(void){
    if(!onewire_reset()) return -127.0f;
    onewire_write_byte(0xCC); // SKIP ROM
    onewire_write_byte(0x44); // CONVERT T
    _delay_ms(750);

    if(!onewire_reset()) return -127.0f;
    onewire_write_byte(0xCC); // SKIP ROM
    onewire_write_byte(0xBE); // READ SCRATCHPAD

    uint8_t lsb = onewire_read_byte();
    uint8_t msb = onewire_read_byte();
    int16_t raw = (int16_t)((msb<<8) | lsb);
    return (float)raw * 0.0625f;
}

// ===============================
// Main program
// ===============================
int main(void) {
    // Buzzer setup
    SET_OUTPUT(DDRB, BUZZER_PIN);
    SET_LOW(PORTB, BUZZER_PIN);

    // LCD setup
    lcd_init();
    lcd_print("Temperature:");

    char buf[16];
    const float PROG = 28.0;

    while (1) {
        float t = ds18b20_get_temp();

        lcd_command(0xC0); // Move to second line
        if (t < -100.0) {
            lcd_print("No sensor      ");
            SET_HIGH(PORTB, BUZZER_PIN);
        } else {
            int tt = (int)(t * 10);
            int whole = tt / 10;
            int frac = tt % 10;
            snprintf(buf, sizeof(buf), "%d.%d%cC   ", whole, frac, 223);
            lcd_print(buf);

            if (t >= PROG) SET_HIGH(PORTB, BUZZER_PIN);
            else           SET_LOW(PORTB, BUZZER_PIN);
        }

        _delay_ms(1000);
    }
}
