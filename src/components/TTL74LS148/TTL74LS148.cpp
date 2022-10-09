/*
 * TTL74LS148.cpp
 * E64
 *
 * Copyright © 2019-2022 elmerucr. All rights reserved.
 *
 * exception collector and priority encoder, 8 input lines, 3 bit output
 */

#include "TTL74LS148.hpp"
#include "common.hpp"

E64::TTL74LS148_ic::TTL74LS148_ic()
{
	number_of_devices = 0;
	for (int i=0; i<256; i++)
		devices[i] = { true, 0 };
}

uint8_t E64::TTL74LS148_ic::connect_device(int level)
{
	devices[number_of_devices].level = level;
	number_of_devices++;
	return (number_of_devices - 1);
}

void E64::TTL74LS148_ic::pull_line(uint8_t handler)
{
	devices[handler].state = false;
	update_interrupt_level();
}

void E64::TTL74LS148_ic::release_line(uint8_t handler)
{
	devices[handler].state = true;
	update_interrupt_level();
}

void E64::TTL74LS148_ic::update_interrupt_level()
{
	unsigned int level = 0;
	for (int i=0; i<number_of_devices; i++) {
		if ((devices[i].state == false) && (devices[i].level > level))
			level = devices[i].level;
	}
	machine.m68k->setIPL(level);
}
