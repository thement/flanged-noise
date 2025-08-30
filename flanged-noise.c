/*
 * Flanged noise generator (thement 2025)
 * ======================================
 *
 * Attiny13a must have internal oscillator set to 9.6 MHz (fuses 0x7a).
 *
 * Potentiometer controls the flange speed.
 * Button synces both noise generators (starts the flange).
 *
 * Pins:
 *   5  PB0   noise generator 1 PWM output
 *   6  PB1   noise generator 2 PWM output
 *   7  PB2   potentiometer
 *   3  PB4   button connected to ground
 *
 * 
 *
 *                                                        ▲ 5V
 *                                                        │
 *                                                        │
 *                                                        │
 *                             ▲  VCC 5V                  /
 *                 attiny13a   │                          \
 *                ┌───┬──┬───┐ │                          / Potentiometer 10-100K
 *              ──┼ 1 └──┘ 8 ┼─┘    ┌───────────────────► \
 *                │          │      │                     /
 *   button     ──┼ 2      7 ┼──────┘                     \
 *     ──┴──      │          │                audio out   │
 *  ┌──     ──────┼ 3      6 ┼──/\/\/\/──────┬───►        │
 *  │             │          │               │            │
 * ─┴─        ┌───┼ 4      5 ┼──/\/\/\/──────┤            │
 *  =         │   └──────────┘   10 KOhm     │           ─┴─
 *           ─┴─                             │            =
 *            =                            ──┴──
 *                                         ──┬── 10nF
 *                                           │
 *                                           │
 *                                          ─┴─
 *                                           =
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdint.h>
#include "defs.h"
#include "utils.h"
#include "pins.h"
#include "adc.h"

// State of each noise generator (noisegen1, noisegen2).
static volatile uint32_t state1 = 1, state2 = 1;
// If `going_up` is 1, then we are slowing down noise generator 1,
// otherwise we are slowing noise generator 2.
static volatile uint8_t going_up = 1;
// Phase accumulator for noise generator: we generate a new noise
// sample every time the accumulator overflows 16-bits.
// The higher the `phase_inc`, the slower the effect is. When `phase_inc`
// is too low, the flange effect disappears.
static volatile uint16_t phase, phase_inc = 0xffc0;
// Number of samples the slower noise generator is lagging behind
// the faster one.
static volatile uint16_t dropped_samples = 0;
// The "depth" of the flange - when one noisegen is behind by
// `target_dropped_samples`, we switch direction and start slowing
// the other noisegen so that it catches up.
static volatile uint16_t target_dropped_samples = 80;

// We could speed up the generator 2x if we use just 16-bit lfsr.
// But the period of 16-bit lfsr at 16 KHz is just 4 seconds which
// might be noticable.
static inline uint32_t
lfsr32_next(uint32_t x)
{
	uint32_t y = x >> 1;
	if (x & 1)
		y ^= 0x80200003;
	return y;
}

ISR(TIM0_OVF_vect)
{
	uint16_t old_phase = phase;
	phase += phase_inc;

	if (going_up) {
		// Slowing down noisegen1, getting it more behind noisegen2
		if (phase < old_phase) {
			state1 = lfsr32_next(state1);
		} else {
			dropped_samples += 1;
			if (dropped_samples >= target_dropped_samples) {
				going_up = 0;
			}
		}
		state2 = lfsr32_next(state2);
	} else {
		// Slowing down noisegen2, letting noisegen1 catch up
		if (phase < old_phase) {
			state2 = lfsr32_next(state2);
		} else {
			dropped_samples -= 1;
			if (dropped_samples == 0) {
				// At this moment both noisegens have the
				// same phase
				going_up = 1;
			}
		}
		state1 = lfsr32_next(state1);
	}
	// Output state to pins
	OCR0A = state1 & 0xff;
	OCR0B = state2 & 0xff;
}

void
timer_setup(void)
{
	//         AABB__WW
	TCCR0A = 0b10100011;
	//         ff__Wccc
	TCCR0B = 0b00000001;
	//         ____bao__
	TIMSK0 = 0b00000010;
}

int
main(void) 
{
	pin_mode(0, OUTPUT);
	pin_mode(1, OUTPUT);
	pin_mode(2, INPUT);
	pin_mode(4, INPUT_PULLUP);

	timer_setup();
	adc_init(ADC_DIV128);
	sei();
	// Read button and potentiometer in a loop.
	// Audio generation is done in interrupt.
	for (;;) {
		if (pin_read(4) == 0) {
			// All variables shared with interrupt handler have to be
			// modified with interrupts disabled.
			cli();
			// Force-sync the state of noisegen1 and noisegen2
			state1 = state2;
			dropped_samples = 0;
			going_up = 1;
			sei();
			// Debounce the button
			_delay_ms(1);
			while (pin_read(4) == 0)
				;
			_delay_ms(1);
		}
		// Read ADC 32x oversampled for better stability
		// Potentiometer controls speed of flange.
		uint16_t speed = 0xffff - (adc_get_32x(ADC_PIN2) >> 8);
		cli();
		phase_inc = speed;
		sei();
	}
	return 0;
}
