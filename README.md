# E64

![E64](./docs/E64_icon_156x156.png)

## General Description

E64 is a computer system that runs on macOS and linux. It's mainly inspired by the Commodore 64 but uses the Motorola 6809 cpu (big endian) and implements some Amiga 500 and Atari ST technology as well.

* [MC6809](https://github.com/elmerucr/mC6809) cpu running at 4.8MHz able to access 16mb shared video ram using an SN74LS612 memory management unit.
* VIDEO chip with a resolution of 512x288 pixels and a palette of 4096 colors (12bit) running at 60Hz. After each screen refresh, it can raise an IRQ request.
* BLITTER chip with alpha blending capabilities and 16mb video ram.
* Sound:
	* Four SID chips (MOS 6581). Each individual SID chip runs at 985248Hz for the same pitch as the chips in the original C64 pal version. Emulation is achieved with the excellent [reSID](http://www.zimmers.net/anonftp/pub/cbm/crossplatform/emulators/resid/index.html) library by Dag Lem. Registers are remapped to enable big endian systems.
	* Four Analog Sound Devices capable of pure sinusoidal sound next to all common other sound waves.
	* Stereo mixing capabilities for all components.
* Eight independent and programmable timers connected to IRQ.

## Screenshots

Below the startup screen with command ```m 0800 0840``` that inspects a piece of io memory at ```$0800```.

![E64](./docs/E64_2022-05-15.png)

The second screenshot shows debug mode after pressing F9. The virtual machine is frozen and can be run step by step.

![E64](./docs/E64_hud_2022-05-15.png)

## Usage

### Some Keyboard Shortcuts

* ```ALT+Q``` quits application
* ```ALT+W``` start/stop wav file output to settings directory
* ```ALT+R``` resets the system
* ```ALT+S``` turns embedded scanlines on/off
* ```ALT+F``` switches between fullscreen and window
* ```F9``` switches between normal and debug mode
* ```F10``` switches on screen stats on/off (visible in normal mode)

## Technical Specifications

### VIDEO and BLITTER

### SID Chips

For the big endian mapping, see picture below. It is adapted from the original Commodore datasheet.
![E64](./docs/SID_Remapping_Big_Endian.png)

### TIMERS

## Compiling

### macOS

* Install [Xcode](https://developer.apple.com/xcode)
* Install the [SDL2 framework](https://www.libsdl.org/download-2.0.php) development library to /Library/Frameworks
* Open the Xcode project in the ./E64 folder and build the application

### Ubuntu Linux / Debian

Run the following commands in a terminal:

````console
$ sudo apt install build-essential cmake git libsdl2-dev
$ git clone https://github.com/elmerucr/E64
$ cd E64 && mkdir build && cd build
$ cmake ..
$ make
````

Finally, to run the application from the build directory:

````console
$ ./E64
````

## Websites and Projects of Interest

### Emulators

* [CCS64](http://www.ccs64.com) - A Commodore 64 Emulator by Per Håkan Sundell.
* [Commander X16 emulator](https://github.com/commanderx16/x16-emulator) - Software version of Commander X16.
* [Hatari](https://hatari.tuxfamily.org) - Hatari is an Atari ST/STE/TT/Falcon emulator.
* [lib65ce02](https://github.com/elmerucr/lib65ce02) - CSG65CE02 emulator written in C.
* [MC6809](https://github.com/elmerucr/mC6809) - MC6809 cpu emulator written in C++.
* [Moira](https://github.com/dirkwhoffmann/Moira) - Motorola 68000 cpu emulator written in C++ by Dirk W. Hoffmann.
* [reSID](http://www.zimmers.net/anonftp/pub/cbm/crossplatform/emulators/resid/index.html) - ReSID is a Commodore 6581 or 8580 Sound Interface Device emulator by Dag Lem.
* [vAmiga](https://dirkwhoffmann.github.io/vAmiga/) - An Amiga 500, 1000, or 2000 on your Apple Macintosh by Dirk W. Hoffmann.
* [VICE](http://vice-emu.sourceforge.net) - The Versatile Commodore Emulator.
* [VirtualC64](https://dirkwhoffmann.github.io/virtualc64/) - A Commodore 64 on your Apple Macintosh by Dirk W. Hoffmann.

### Other

* [Commander X16](https://www.commanderx16.com) - The Commander X16 is a modern 8-bit computer currently in active development. It is the brainchild of David "the 8 Bit Guy" Murray.
* [Mega65](http://mega65.org) - The 21st century realization of the C65 heritage.
* [SDL Simple DirectMedia Layer](https://www.libsdl.org) - A cross-platform development library by Sam Lantinga designed to provide low level access to audio, keyboard, mouse, joystick, and graphics hardware.
* [stb](https://github.com/nothings/stb) - single-file public domain (or MIT licensed) libraries for C/C++
* [visual6502](http://www.visual6502.org) - Visual Transistor-level Simulation of the 6502 CPU and other chips.

## MIT License

Copyright (c) 2022 elmerucr

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
