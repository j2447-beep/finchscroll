// finchscroll — page a text file up through the terminal, then start again.
//
// The file is read once into memory, not re-read each pass, so a long loop
// does not keep hitting the disk and the output cannot change halfway
// through if something else rewrites the file.
//
// Build:  g++ -std=c++17 -O2 -o finchscroll finchscroll.cpp
// Usage:  ./finchscroll FILE [-d MS] [-n PASSES] [-c] [-b]

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

// Set from the signal handler, so it must be a plain sig_atomic_t and
// nothing fancier — writing anything else from a handler is undefined.
volatile std::sig_atomic_t g_interrupted = 0;

void on_signal(int) { g_interrupted = 1; }

constexpr const char* kHideCursor = "\033[?25l";
constexpr const char* kShowCursor = "\033[?25h";
constexpr const char* kClearScreen = "\033[2J\033[H";  // erase, then home

// sleep_for would swallow most of a long delay before noticing Ctrl-C, so
// wait in short slices and check the flag between them. 20 ms keeps the
// program responsive without busy-looping.
void interruptible_sleep(std::chrono::milliseconds total) {
    using namespace std::chrono;
    constexpr auto slice = milliseconds(20);
    auto remaining = total;
    while (remaining > milliseconds::zero() && !g_interrupted) {
        const auto chunk = std::min(slice, remaining);
        std::this_thread::sleep_for(chunk);
        remaining -= chunk;
    }
}

std::vector<std::string> read_lines(const std::string& path, std::string& error) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    if (!in) {
        error = "cannot open '" + path + "': " + std::strerror(errno);
        return lines;
    }
    std::string line;
    while (std::getline(in, line)) {
        // A CRLF file would otherwise leave a stray \r that redraws the
        // line from column zero and garbles the output.
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
    }
    if (in.bad()) error = "error reading '" + path + "': " + std::strerror(errno);
    return lines;
}

void usage(const char* argv0) {
    std::cerr
        << "usage: " << argv0 << " FILE [options]\n\n"
        << "  -d, --delay MS     pause between lines (default 150)\n"
        << "  -n, --passes N     number of passes; 0 = forever (default 0)\n"
        << "  -c, --clear        clear the screen before each pass\n"
        << "  -b, --blank N      blank lines between passes (default 1)\n"
        << "  -h, --help         this message\n\n"
        << "Ctrl-C stops cleanly and restores the cursor.\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string path;
    long delay_ms = 150;
    long passes = 0;  // 0 means loop until interrupted
    long blanks = 1;
    bool clear_each = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&](long& out) -> bool {
            if (i + 1 >= argc) {
                std::cerr << arg << " needs a value\n";
                return false;
            }
            try {
                out = std::stol(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << arg << ": '" << argv[i] << "' is not a number\n";
                return false;
            }
            return true;
        };

        if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
            return 0;
        } else if (arg == "-c" || arg == "--clear") {
            clear_each = true;
        } else if (arg == "-d" || arg == "--delay") {
            if (!value(delay_ms)) return 2;
        } else if (arg == "-n" || arg == "--passes") {
            if (!value(passes)) return 2;
        } else if (arg == "-b" || arg == "--blank") {
            if (!value(blanks)) return 2;
        } else if (!arg.empty() && arg[0] == '-' && arg != "-") {
            std::cerr << "unknown option: " << arg << "\n";
            usage(argv[0]);
            return 2;
        } else if (path.empty()) {
            path = arg;
        } else {
            std::cerr << "only one file, please\n";
            return 2;
        }
    }

    if (path.empty()) {
        usage(argv[0]);
        return 2;
    }
    if (delay_ms < 0 || passes < 0 || blanks < 0) {
        std::cerr << "delay, passes and blank must not be negative\n";
        return 2;
    }

    std::string error;
    const std::vector<std::string> lines = read_lines(path, error);
    if (!error.empty()) {
        std::cerr << argv[0] << ": " << error << "\n";
        return 1;
    }
    if (lines.empty()) {
        std::cerr << argv[0] << ": '" << path << "' has no lines to show\n";
        return 1;
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    std::cout << kHideCursor << std::flush;

    for (long pass = 0; (passes == 0 || pass < passes) && !g_interrupted; ++pass) {
        if (clear_each) std::cout << kClearScreen;

        for (const auto& line : lines) {
            if (g_interrupted) break;
            // endl rather than "\n": the flush is what makes each line
            // appear on time instead of arriving in buffered bursts.
            std::cout << line << std::endl;
            interruptible_sleep(std::chrono::milliseconds(delay_ms));
        }

        const bool more_to_come = (passes == 0 || pass + 1 < passes);
        if (!clear_each && more_to_come && !g_interrupted) {
            for (long i = 0; i < blanks; ++i) std::cout << "\n";
            std::cout << std::flush;
        }
    }

    // Always restore the cursor, including on the interrupted path — an
    // invisible cursor left behind outlives the program and makes the
    // user's shell look broken.
    std::cout << kShowCursor << std::flush;

    if (g_interrupted) {
        std::cout << "\n";
        return 130;  // conventional exit status for SIGINT
    }
    return 0;
}
