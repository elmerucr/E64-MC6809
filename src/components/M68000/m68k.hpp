/*
 * m68k.hpp
 * E64
 *
 * Copyright © 2022 elmerucr. All rights reserved.
 *
 */

#ifndef M68K_HPP
#define M68K_HPP

#include "Moira.h"

using namespace moira;

namespace E64
{

class m68k_ic : public Moira {
	u8  read8 (u32 addr) override;
	u16 read16(u32 addr) override;
	void write8 (u32 addr, u8  val) override;
	void write16(u32 addr, u16 val) override;
};

}

#endif
