#include "timer.h"
#include "io.h"
#include "terminal.h"
#include "task.h"
#include "idt.h"

uint32_t timer_ticks = 0;

static void timer_callback(registers_t* regs) {
    timer_ticks++;
    // Call scheduler for preemption
    schedule();
}

void timer_init(uint32_t frequency) {
    register_interrupt_handler(32, timer_callback);

    // Calculate divisor for PIT (1193180 Hz base clock)
    uint32_t divisor = 1193180 / frequency;

    // Command byte: Channel 0, LSB/MSB, Square Wave Mode, 16-bit binary (0x36)
    outb(0x43, 0x36);

    // Send divisor
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}
