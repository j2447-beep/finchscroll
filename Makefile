# finchscroll — page a text file up through the terminal, then repeat.
#
#   make            build ./finchscroll
#   make debug      rebuild with sanitizers and no optimisation
#   make install    install to $(PREFIX)/bin  (default /usr/local)
#   make clean      remove the binary

CXX      ?= g++
CXXSTD   := -std=c++17
WARN     := -Wall -Wextra -Wpedantic
CXXFLAGS ?= -O2
PREFIX   ?= /usr/local
BINDIR   := $(PREFIX)/bin

TARGET := finchscroll
SRC    := finchscroll.cpp

.PHONY: all debug clean install uninstall run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXSTD) $(WARN) $(CXXFLAGS) -o $@ $<

# Address and UB sanitizers catch the mistakes this program could plausibly
# make -- an out-of-range index, a signal-handler data race -- which a plain
# -O2 build will happily run past.
debug: CXXFLAGS := -O0 -g -fsanitize=address,undefined
debug: clean $(TARGET)

clean:
	rm -f $(TARGET)

# DESTDIR is honoured so packagers can stage into a build root.
install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

# Scrolls the Makefile itself -- a quick smoke test with no fixture needed.
run: $(TARGET)
	./$(TARGET) Makefile -d 60 -n 1
