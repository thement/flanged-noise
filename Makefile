CC=avr-gcc
CLOCK=9600000UL
CFLAGS=-Os -g -std=gnu99 -Wall -mmcu=attiny13a -MP -MD -DF_CPU=$(CLOCK)
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
PROGS=flanged-noise
ELFS=$(addsuffix .elf,$(PROGS))
PROGRAMMER=usbasp-clone
PORT=ussb
AVRDUDE_TARGET=t13

all: $(ELFS)


%.elf: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	avr-size $@
%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

.PHONY: clean prog fuses

clean:
	-rm *.elf *.o *.hex

.PHONY: %.prog
%.prog: %.hex
	avrdude -p $(AVRDUDE_TARGET) -c $(PROGRAMMER) -e -U flash:w:$<

fuses:
	avrdude -p $(AVRDUDE_TARGET) -c $(PROGRAMMER) -e -B1000Hz -U lfuse:w:0x7A:m

-include $(addsuffix .d,$(PROGS))
