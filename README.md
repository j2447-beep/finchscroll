# finchscroll

Pages a text file up through the terminal, then starts again.

No dependencies beyond the C++ standard library and POSIX signals, so it
builds anywhere with a C++17 compiler.

## Build

```sh
make
```

Needs a C++17 compiler and `make`. Most systems have both; two that do not:

| System | First |
|---|---|
| Arch Linux | `sudo pacman -S base-devel` |
| Debian / Ubuntu | `sudo apt install build-essential` |

A base Arch install ships neither `gcc` nor `make` — they are in the
`base-devel` group, which is not installed by default. The symptom is
`make: command not found`, which reads as the wrong command rather than a
missing package.

There is nothing to configure and no dependencies to fetch. If you would
rather skip `make` entirely, the Makefile only runs this:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -o finchscroll finchscroll.cpp
```

## Use

```sh
./finchscroll poem.txt              # 150 ms a line, loop until Ctrl-C
./finchscroll notes.md -d 400 -c    # slower, clearing the screen each pass
./finchscroll log.txt -d 50 -n 3    # three passes, then stop
```

| Flag | Meaning | Default |
|---|---|---|
| `-d, --delay MS` | pause between lines | 150 |
| `-n, --passes N` | passes to run; `0` means forever | 0 |
| `-c, --clear` | clear the screen before each pass | off |
| `-b, --blank N` | blank lines between passes | 1 |
| `-h, --help` | usage | |

Ctrl-C stops cleanly and restores the cursor.

Exit codes: `0` success, `1` file problems, `2` bad arguments, `130`
interrupted — the conventional value for SIGINT, so shell scripts behave
sensibly around it.

## Install

```sh
sudo make install            # to /usr/local/bin
make install PREFIX=~/.local # or somewhere without sudo
```

`make uninstall` reverses it. `DESTDIR` is honoured for staged installs.

## Four decisions worth knowing about

**The file is read once, not per pass.** A loop running for hours never
touches the disk again, and the output cannot change halfway through if
something else rewrites the file underneath it.

**Ctrl-C is responsive even at long delays.** Sleeping the whole delay in
one call means `-d 3000` leaves the user waiting three seconds after
pressing Ctrl-C. This sleeps in 20 ms slices and checks the flag between
them. The handler itself only writes a `sig_atomic_t` — anything more in a
signal handler is undefined behaviour.

**The cursor is always restored**, including on the interrupted path. A
hidden cursor left behind outlives the program and makes the user's shell
look broken.

**CRLF is stripped.** A file saved on Windows would otherwise leave a stray
`\r` that returns the carriage to column zero and garbles the display.

## Portability

Written against standard C++17 plus POSIX signals, with no GNU or Apple
extensions.

Built on both:

| Platform | Compiler |
|---|---|
| macOS (arm64) | Apple clang — note `g++` there is clang, not GNU g++ |
| Arch Linux | GNU GCC |

That distinction matters: because macOS ships `g++` as an alias for clang,
a clean build there says nothing about GCC. The one real portability bug
found so far was `errno` used without including `<cerrno>`, which Apple's
libc++ supplies transitively via `<cstring>` and libstdc++ does not
promise to.
