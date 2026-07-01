#include "terminal.h"
#include <stdarg.h>

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;

static uint8_t vga_entry_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

static uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

void terminal_init(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(15, 0); // White on Black
    terminal_clear();
}

void terminal_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            VGA_MEMORY[index] = vga_entry(' ', terminal_color);
        }
    }
}

static void terminal_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
    terminal_row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
    } else if (c == '\r') {
        terminal_column = 0;
    } else {
        const size_t index = terminal_row * VGA_WIDTH + terminal_column;
        VGA_MEMORY[index] = vga_entry(c, terminal_color);
        if (++terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT) {
                terminal_scroll();
            }
        }
    }
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        terminal_putchar(data[i]);
    }
}

void terminal_writestring(const char* data) {
    size_t len = 0;
    while (data[len]) len++;
    terminal_write(data, len);
}

static void print_hex(uint32_t val) {
    char buf[9];
    buf[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        int digit = val & 0xF;
        if (digit < 10) {
            buf[i] = '0' + digit;
        } else {
            buf[i] = 'A' + (digit - 10);
        }
        val >>= 4;
    }
    char* p = buf;
    while (*p == '0' && *(p+1) != '\0') p++;
    terminal_writestring(p);
}

static void print_dec(int val) {
    if (val == 0) {
        terminal_putchar('0');
        return;
    }
    if (val < 0) {
        terminal_putchar('-');
        val = -val;
    }
    char buf[12];
    buf[11] = '\0';
    int i = 10;
    while (val > 0) {
        buf[i--] = '0' + (val % 10);
        val /= 10;
    }
    terminal_writestring(&buf[i+1]);
}

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    while (*format) {
        if (*format == '%') {
            format++;
            if (*format == 's') {
                char* s = va_arg(args, char*);
                terminal_writestring(s ? s : "(null)");
            } else if (*format == 'd') {
                int d = va_arg(args, int);
                print_dec(d);
            } else if (*format == 'x') {
                uint32_t x = va_arg(args, uint32_t);
                print_hex(x);
            } else if (*format == 'c') {
                char c = (char) va_arg(args, int);
                terminal_putchar(c);
            } else if (*format == '%') {
                terminal_putchar('%');
            }
        } else {
            terminal_putchar(*format);
        }
        format++;
    }
    va_end(args);
}
