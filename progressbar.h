/*
  Copyright (C) 2018 Sven Willner <sven.willner@gmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include <stdarg.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WINDOWS
#include <windows.h>
#else
#include <sys/ioctl.h>
#endif

#define PROGRESSBAR_TERMINAL_CURSOR_UP "\x1b[A"
#define PROGRESSBAR_TERMINAL_CLEAR_TO_EOL "\x1b[K"

namespace progressbar {

class ProgressBar {
  protected:
    using ticks = std::chrono::steady_clock::rep;
    std::vector<char> buf;
    bool is_tty;
    bool closed = false;
    std::mutex mutex_m;
    std::atomic<std::size_t> current;
    std::size_t reprint_next = 1;
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    std::size_t eta_from_iter = 0;
    std::chrono::time_point<std::chrono::steady_clock> eta_from_time;
    std::size_t last_reprint_iter = 0;
    std::chrono::time_point<std::chrono::steady_clock> last_reprint_time;
    const ticks min_reprint_time;
    std::FILE* out;

    void update(std::size_t n) {
        current += n;
        if (current >= reprint_next) {
            std::lock_guard<std::mutex> guard(mutex_m);
            if (!closed) {
                recalc_and_print(is_tty);
            }
        }
    }

    void recalc_and_print(bool replace_last_line, bool force = false) {
        auto now = std::chrono::steady_clock::now();
        auto duration = (now - last_reprint_time).count();
        reprint_next = current + (current - last_reprint_iter) * min_reprint_time / std::max(duration, min_reprint_time) + 1;
        if (duration >= min_reprint_time || force) {
            auto freq =
                (current - last_reprint_iter) * std::chrono::steady_clock::period::den / static_cast<float>(duration * std::chrono::steady_clock::period::num);
            auto etr = std::lrint((total - current)
                                  * ((1 - smoothing) * duration / static_cast<float>((current - last_reprint_iter))
                                     + smoothing * (now - eta_from_time).count() / static_cast<float>(current - eta_from_iter)));

            print_bar(is_tty, replace_last_line, freq, (now - start_time).count(), etr, true);

            last_reprint_time = now;
            last_reprint_iter = current;
        }
    }

    inline static bool safe_print(char*& c, std::size_t& buf_remaining, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        int res = vsnprintf(c, buf_remaining, fmt, args);
        va_end(args);
        if (res < 0 || buf_remaining <= static_cast<std::size_t>(res)) {
            return false;
        }
        buf_remaining -= res;
        c += res;
        return true;
    }

    inline static bool safe_print_duration(char*& c, std::size_t& buf_remaining, ticks t) {
        auto dur = std::chrono::steady_clock::duration(t);
        auto d = std::chrono::duration_cast<std::chrono::duration<ticks, std::ratio<3600 * 24>>>(dur);
        auto h = std::chrono::duration_cast<std::chrono::hours>(dur -= d);
        auto m = std::chrono::duration_cast<std::chrono::minutes>(dur -= h);
        auto s = std::chrono::duration_cast<std::chrono::seconds>(dur -= m);
        if (d.count() > 0) {
            return safe_print(c, buf_remaining, "%u-%02u:%02u:%02u", d.count(), h.count(), m.count(), s.count());
        }
        if (h.count() > 0) {
            return safe_print(c, buf_remaining, "%02u:%02u:%02u", h.count(), m.count(), s.count());
        }
        if (m.count() > 0) {
            return safe_print(c, buf_remaining, "%02u:%02u", m.count(), s.count());
        }
        return safe_print(c, buf_remaining, "%us", s.count());
    }

    inline void print_to_buf(float freq, ticks runtime, ticks etr, bool etr_known) {
        char* c = &buf[0];
        auto buf_remaining = buf.size();
        if (!description.empty()) {
            if (!safe_print(c, buf_remaining, "%s: ", description.c_str())) {
                return;
            }
        }
        if (!safe_print(c, buf_remaining, "%u/%u ", static_cast<std::size_t>(current), total)) {
            return;
        }
        auto prefix_len = buf.size() - buf_remaining;
        if (!safe_print(c, buf_remaining, " %u%% ", std::lrint(current * 100 / static_cast<float>(total)))) {
            return;
        }
        if (!safe_print_duration(c, buf_remaining, runtime)) {
            return;
        }
        if (freq >= 1 || freq <= 1e-9) {
            if (!safe_print(c, buf_remaining, " %.1f/s ", freq)) {
                return;
            }
        } else {
            if (!safe_print(c, buf_remaining, " %.1fs ", 1 / freq)) {
                return;
            }
        }
        if (etr_known) {
            if (!safe_print_duration(c, buf_remaining, etr)) {
                return;
            }
        } else {
            if (!safe_print(c, buf_remaining, "?")) {
                return;
            }
        }
        std::memmove(&buf[0] + prefix_len + buf_remaining, &buf[0] + prefix_len, buf.size() - buf_remaining - prefix_len);
        auto done_chars = std::lrint(current * buf_remaining / static_cast<float>(total));
        std::memset(&buf[0] + prefix_len, indicator_done, done_chars);
        std::memset(&buf[0] + prefix_len + done_chars, indicator_left, buf_remaining - done_chars);
    }

    void print_bar(bool recalc_width, bool replace_last_line, float freq, ticks runtime, ticks etr, bool etr_known) {
        if (recalc_width) {
            if (is_tty) {
#ifdef _WINDOWS
                CONSOLE_SCREEN_BUFFER_INFO c;
                GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &c);
                buf.resize(c.srWindow.Right - c.srWindow.Left);
#else
                winsize w;
                ioctl(0, TIOCGWINSZ, &w);
                buf.resize(w.ws_col);
#endif
            } else {
                buf.resize(65);
            }
        }
        print_to_buf(freq, runtime, etr, etr_known);
        if (replace_last_line) {
            fwrite(PROGRESSBAR_TERMINAL_CURSOR_UP, 1, sizeof(PROGRESSBAR_TERMINAL_CURSOR_UP), out);
        }
        fwrite(&buf[0], 1, buf.size(), out);
        fputc('\n', out);
    }

  public:
    const std::size_t total;
    std::string description;
    float smoothing = 0.1;
    char indicator_done = '#';
    char indicator_left = '-';

    ProgressBar(std::size_t total_p, std::string description_p = "", std::FILE* out_p = stdout, std::size_t min_reprint_time_ms = 100)
        : min_reprint_time(std::chrono::steady_clock::duration(std::chrono::milliseconds(min_reprint_time_ms)).count()),
          out(out_p),
          total(total_p),
          description(std::move(description_p)) {
        std::lock_guard<std::mutex> guard(mutex_m);
        start_time = std::chrono::steady_clock::now();
        eta_from_time = start_time;
        last_reprint_time = start_time;
        current = 0;
        is_tty = isatty(fileno(out));
        print_bar(true, false, 0, 0, 0, false);
    }
    ~ProgressBar() { close(); }

    inline ProgressBar& operator++(int) {
        update(1);
        return *this;
    }

    inline ProgressBar& operator++() {
        update(1);
        return *this;
    }

    inline void operator+=(std::size_t n) { update(n); }
    inline void operator=(std::size_t n) {
        if (n > current) {
            update(n - current);
        }
    }

    void reset_eta() {
        std::lock_guard<std::mutex> guard(mutex_m);
        eta_from_iter = current;
        eta_from_time = std::chrono::steady_clock::now();
    }

    void close() {
        std::lock_guard<std::mutex> guard(mutex_m);
        if (!closed) {
            auto total_duration = (std::chrono::steady_clock::now() - start_time).count();
            auto freq = current * std::chrono::steady_clock::period::den / static_cast<float>(total_duration * std::chrono::steady_clock::period::num);
            current = total;
            print_bar(is_tty, is_tty, freq, total_duration, 0, true);
            closed = true;
        }
    }

    void println(const std::string& s) {
        std::lock_guard<std::mutex> guard(mutex_m);
        if (is_tty && !closed) {
            fwrite(PROGRESSBAR_TERMINAL_CURSOR_UP PROGRESSBAR_TERMINAL_CLEAR_TO_EOL, 1,
                   sizeof(PROGRESSBAR_TERMINAL_CURSOR_UP PROGRESSBAR_TERMINAL_CLEAR_TO_EOL), out);
        }
        fwrite(s.c_str(), 1, s.size(), out);
        fputc('\n', out);
        if (!closed) {
            recalc_and_print(false, true);
        }
    }

    void refresh() {
        std::lock_guard<std::mutex> guard(mutex_m);
        if (!closed) {
            recalc_and_print(is_tty, true);
        }
    }
};

}  // namespace progressbar

#endif
