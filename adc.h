enum {
	ADC_PIN5 = 0,
	ADC_PIN2 = 1,
	ADC_PIN4 = 2,
	ADC_PIN3 = 3,
};

enum {
	ADC_DIV2 = 1,
	ADC_DIV4 = 2,
	ADC_DIV8 = 3,
	ADC_DIV16 = 4,
	ADC_DIV32 = 5,
	ADC_DIV64 = 6,
	ADC_DIV128 = 7,
};

static void
adc_init(byte clock_divisor)
{
	ADCSRA = _BV(ADEN) | (clock_divisor & 7);
}

static void
adc_start(byte pin)
{
	ADMUX = pin;
	ADCSRA |= _BV(ADSC);
}

static uint16_t
adc_get(void)
{
	/* wait for conversion to finish */
	while (ADCSRA & _BV(ADSC))
		;
	/* fetch the results */
	byte lo = ADCL;
	return ((uint16_t)ADCH << 8) | lo;
}

static uint16_t
adc_get_32x(byte pin)
{
	uint16_t sum = 0;
	for (byte i = 0; i < 32; i++) {
		adc_start(pin);
		sum += adc_get();
	}
	return sum;
}
