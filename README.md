# quakegeneric

![a low-resolution screenshot of quake](./.github/quakegeneric.png)

it's like [doomgeneric](https://github.com/ozkl/doomgeneric), but for quake. it's based on the GPL WinQuake source code.

currently it can only compile for 32-bit architechtures.

## implementations

- [`quakegeneric_null.c`](./source/quakegeneric_null.c) - null
- [`quakegeneric_dos.c`](./source/quakegeneric_dos.c) - MS-DOS
- [`quakegeneric_sdl2.c`](./source/quakegeneric_sdl2.c) - SDL2
- [`quakegeneric_w32.c`](./source/quakegeneric_w32.c) - Win32
- [`qg-gui.c`](./source/qg-gui.c), with [`qg-c4ix.c`](./source/qg-c4ix.c) or [`qg-bare.c`](./source/qg-bare.c) - [c4](https://github.com/andrakis/c4): C4IX, C4DOS and the bare machine ([`README.c4.md`](./README.c4.md))

## building

on unix-like platforms:

```
cd source/
make
```

for Open Watcom:

```
cd source/
wmake -f makefile.wat
```

for CMake:

```
mkdir cmake-build
cd cmake-build/
cmake ..
make
```

for Meson:

```
mkdir meson-build
meson setup meson-build
cd meson-build/
meson compile
```

for Windows:

```
cd source/
nmake makefile.win
```

for c4 (C4IX, C4DOS and the bare machine), from a checkout of [c4](https://github.com/andrakis/c4), where this repository is the submodule `games/quake`:

```
git submodule update --init --checkout games/quake
QUAKE_ZIP=/path/to/quake106.zip make quake-data
make c4bb-images
```

`quake-data` takes `pak0.pak` out of id's shareware package, which is not in this repository; without `QUAKE_ZIP` it fetches the package from a mirror in [`data/MIRRORS`](./data/MIRRORS). `c4bb-images` builds every game whose source is present, and links this one for each target. `make test-quake` then holds the frames of all three to a gcc build's. Then, in C4IX, or at C4DOS's prompt with 128 MB:

```
quake
```

`quake -window` plays in the Terminal's window on C4IX's desktop. [`README.c4.md`](./README.c4.md) has the rest.

## platforms

the following compilers have been tested to work with this source:

- GCC
- Clang
- MinGW
- TinyCC
- Open Watcom
- MSVC
- [c4lc](https://github.com/andrakis/c4)

## License

Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
