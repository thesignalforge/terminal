/*
 * terminal.c - PHP Terminal Extension Implementation
 *
 * High-performance terminal control for PHP CLI applications.
 * Provides raw mode, styling, tables, progress bars, and interactive input.
 *
 * Copyright (c) 2024 Signalforge
 */

#include "php_terminal.h"
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <wchar.h>
#include <locale.h>

/* Module globals */
ZEND_DECLARE_MODULE_GLOBALS(terminal)

/* Class entries */
zend_class_entry *terminal_exception_ce = NULL;
zend_class_entry *terminal_progress_bar_ce = NULL;
zend_class_entry *terminal_loader_ce = NULL;

/* Object handlers */
zend_object_handlers progress_bar_handlers;
zend_object_handlers loader_handlers;

/* Original signal handlers */
static struct sigaction orig_sigwinch;
static struct sigaction orig_sigint;
static struct sigaction orig_sigterm;
static struct sigaction orig_sigtstp;
static struct sigaction orig_sigcont;

/* ============================================================================
 * MODULE LIFECYCLE
 * ============================================================================ */

/**
 * Initialize terminal state structure
 */
void terminal_init_state(void)
{
    terminal_state_t *state = pecalloc(1, sizeof(terminal_state_t), 1);
    state->state_flags = TERM_STATE_NONE;
    state->color_support = COLOR_NONE;
    state->cols = 80;
    state->rows = 24;
    state->tty_fd = -1;
    state->write_buffer_pos = 0;
    state->resize_pending = 0;
    ZVAL_UNDEF(&state->resize_callback);

    TERMINAL_G(state) = state;
}

/**
 * Free terminal state structure
 */
void terminal_free_state(void)
{
    terminal_state_t *state = TERM_STATE();
    if (state) {
        if (!Z_ISUNDEF(state->resize_callback)) {
            zval_ptr_dtor(&state->resize_callback);
        }
        pefree(state, 1);
        TERMINAL_G(state) = NULL;
    }
}

/* ============================================================================
 * TERMINAL RAW MODE
 * ============================================================================ */

/**
 * Enter raw terminal mode
 * Returns 0 on success, -1 on failure
 */
int terminal_enter_raw(void)
{
    terminal_state_t *state = TERM_STATE();

    if (state->state_flags & TERM_STATE_RAW) {
        return 0; /* Already in raw mode */
    }

    /* Check if stdin is a TTY */
    if (!isatty(STDIN_FILENO)) {
        return -1;
    }

    state->tty_fd = STDIN_FILENO;

    /* Save original terminal settings */
    if (tcgetattr(state->tty_fd, &state->original_termios) == -1) {
        return -1;
    }

    /* Configure raw mode */
    state->raw_termios = state->original_termios;

    /* Input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control */
    state->raw_termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /* Output modes: disable post processing */
    state->raw_termios.c_oflag &= ~(OPOST);

    /* Control modes: set 8 bit chars */
    state->raw_termios.c_cflag |= (CS8);

    /* Local modes: echo off, canonical off, no extended functions,
     * no signal chars (^Z, ^C) */
    state->raw_termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* Control chars: set return condition min number of bytes and timer */
    state->raw_termios.c_cc[VMIN] = 0;  /* Return immediately with available data */
    state->raw_termios.c_cc[VTIME] = 0; /* No timer */

    /* Apply raw mode settings */
    if (tcsetattr(state->tty_fd, TCSAFLUSH, &state->raw_termios) == -1) {
        return -1;
    }

    state->state_flags |= TERM_STATE_RAW;

    /* Update terminal size */
    terminal_update_size();

    /* Detect color support */
    state->color_support = terminal_detect_color_support();

    /* Setup signal handlers */
    terminal_setup_signal_handlers();

    return 0;
}

/**
 * Exit raw terminal mode and restore original settings
 * Returns 0 on success, -1 on failure
 */
int terminal_exit_raw(void)
{
    terminal_state_t *state = TERM_STATE();

    if (!(state->state_flags & TERM_STATE_RAW)) {
        return 0; /* Not in raw mode */
    }

    /* Flush any pending output */
    terminal_flush_buffer();

    /* Restore cursor if hidden */
    if (state->state_flags & TERM_STATE_CURSOR_HIDDEN) {
        terminal_cursor_show(1);
    }

    /* Exit alternate screen if active */
    if (state->state_flags & TERM_STATE_ALT_SCREEN) {
        terminal_alternate_screen(0);
    }

    /* Restore original terminal settings */
    if (tcsetattr(state->tty_fd, TCSAFLUSH, &state->original_termios) == -1) {
        return -1;
    }

    state->state_flags &= ~TERM_STATE_RAW;

    /* Restore signal handlers */
    terminal_restore_signal_handlers();

    return 0;
}

/**
 * Update cached terminal size
 */
void terminal_update_size(void)
{
    terminal_state_t *state = TERM_STATE();
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        state->cols = ws.ws_col > 0 ? ws.ws_col : 80;
        state->rows = ws.ws_row > 0 ? ws.ws_row : 24;
    }
}

/**
 * Detect terminal color support level
 */
int terminal_detect_color_support(void)
{
    const char *term = getenv("TERM");
    const char *colorterm = getenv("COLORTERM");

    /* Check for true color support */
    if (colorterm) {
        if (strcmp(colorterm, "truecolor") == 0 || strcmp(colorterm, "24bit") == 0) {
            return COLOR_TRUECOLOR;
        }
    }

    /* Check TERM variable */
    if (term) {
        /* True color terminals */
        if (strstr(term, "truecolor") || strstr(term, "24bit")) {
            return COLOR_TRUECOLOR;
        }

        /* 256 color terminals */
        if (strstr(term, "256color") || strstr(term, "256")) {
            return COLOR_256;
        }

        /* Basic color support */
        if (strstr(term, "color") || strstr(term, "xterm") ||
            strstr(term, "screen") || strstr(term, "vt100") ||
            strstr(term, "linux") || strstr(term, "ansi")) {
            return COLOR_16;
        }

        /* No color support (dumb terminals) */
        if (strcmp(term, "dumb") == 0) {
            return COLOR_NONE;
        }
    }

    /* Default to basic color if we have a TTY */
    if (isatty(STDOUT_FILENO)) {
        return COLOR_16;
    }

    return COLOR_NONE;
}

/* ============================================================================
 * SIGNAL HANDLERS
 * ============================================================================ */

/**
 * SIGWINCH handler - terminal resize
 */
void terminal_handle_sigwinch(int sig)
{
    terminal_state_t *state = TERM_STATE();
    if (state) {
        state->resize_pending = 1;
    }
}

/**
 * SIGINT/SIGTERM handler - cleanup and exit
 */
void terminal_handle_sigint(int sig)
{
    terminal_exit_raw();

    /* Re-raise signal with default handler */
    signal(sig, SIG_DFL);
    raise(sig);
}

/**
 * SIGTSTP handler - restore terminal before suspend
 */
void terminal_handle_sigtstp(int sig)
{
    terminal_state_t *state = TERM_STATE();

    /* Temporarily restore terminal */
    if (state && (state->state_flags & TERM_STATE_RAW)) {
        tcsetattr(state->tty_fd, TCSAFLUSH, &state->original_termios);
    }

    /* Reset handler and re-raise */
    signal(SIGTSTP, SIG_DFL);
    raise(SIGTSTP);
}

/**
 * SIGCONT handler - re-enter raw mode after resume
 */
void terminal_handle_sigcont(int sig)
{
    terminal_state_t *state = TERM_STATE();

    /* Re-apply raw mode */
    if (state && (state->state_flags & TERM_STATE_RAW)) {
        tcsetattr(state->tty_fd, TCSAFLUSH, &state->raw_termios);
        terminal_update_size();
    }

    /* Re-install SIGTSTP handler */
    struct sigaction sa;
    sa.sa_handler = terminal_handle_sigtstp;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTSTP, &sa, NULL);
}

/**
 * Setup signal handlers
 */
void terminal_setup_signal_handlers(void)
{
    struct sigaction sa;

    /* SIGWINCH - terminal resize */
    sa.sa_handler = terminal_handle_sigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, &orig_sigwinch);

    /* SIGINT - interrupt */
    sa.sa_handler = terminal_handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, &orig_sigint);

    /* SIGTERM - termination */
    sa.sa_handler = terminal_handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, &orig_sigterm);

    /* SIGTSTP - suspend */
    sa.sa_handler = terminal_handle_sigtstp;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTSTP, &sa, &orig_sigtstp);

    /* SIGCONT - continue */
    sa.sa_handler = terminal_handle_sigcont;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGCONT, &sa, &orig_sigcont);
}

/**
 * Restore original signal handlers
 */
void terminal_restore_signal_handlers(void)
{
    sigaction(SIGWINCH, &orig_sigwinch, NULL);
    sigaction(SIGINT, &orig_sigint, NULL);
    sigaction(SIGTERM, &orig_sigterm, NULL);
    sigaction(SIGTSTP, &orig_sigtstp, NULL);
    sigaction(SIGCONT, &orig_sigcont, NULL);
}

/* ============================================================================
 * OUTPUT BUFFERING
 * ============================================================================ */

/**
 * Flush the write buffer to stdout
 */
void terminal_flush_buffer(void)
{
    terminal_state_t *state = TERM_STATE();

    if (state && state->write_buffer_pos > 0) {
        ssize_t written = write(STDOUT_FILENO, state->write_buffer, state->write_buffer_pos);
        (void)written; /* Ignore write errors for terminal output */
        state->write_buffer_pos = 0;
    }
}

/**
 * Write data to the buffer (auto-flush when full)
 */
void terminal_write(const char *str, size_t len)
{
    terminal_state_t *state = TERM_STATE();

    if (!state) {
        ssize_t written = write(STDOUT_FILENO, str, len);
        (void)written; /* Ignore write errors for terminal output */
        return;
    }

    while (len > 0) {
        size_t available = WRITE_BUFFER_SIZE - state->write_buffer_pos;
        size_t to_copy = len < available ? len : available;

        memcpy(state->write_buffer + state->write_buffer_pos, str, to_copy);
        state->write_buffer_pos += to_copy;
        str += to_copy;
        len -= to_copy;

        if (state->write_buffer_pos >= WRITE_BUFFER_SIZE) {
            terminal_flush_buffer();
        }
    }
}

/**
 * Write a null-terminated string to the buffer
 */
void terminal_write_str(const char *str)
{
    terminal_write(str, strlen(str));
}

/**
 * Printf to the terminal buffer
 */
void terminal_printf(const char *fmt, ...)
{
    char buf[1024];
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        size_t write_len = (size_t)len < sizeof(buf) ? (size_t)len : sizeof(buf) - 1;
        terminal_write(buf, write_len);
    }
}

/* ============================================================================
 * CURSOR AND SCREEN CONTROL
 * ============================================================================ */

/**
 * Clear the entire screen
 */
void terminal_clear_screen(void)
{
    terminal_write_str("\x1b[2J\x1b[H");
}

/**
 * Clear the current line
 */
void terminal_clear_line(void)
{
    terminal_write_str("\x1b[2K\r");
}

/**
 * Move cursor to position (0-indexed)
 */
void terminal_cursor_to(int col, int row)
{
    terminal_printf("\x1b[%d;%dH", row + 1, col + 1);
}

/**
 * Move cursor up N lines
 */
void terminal_cursor_up(int n)
{
    if (n > 0) {
        terminal_printf("\x1b[%dA", n);
    }
}

/**
 * Move cursor down N lines
 */
void terminal_cursor_down(int n)
{
    if (n > 0) {
        terminal_printf("\x1b[%dB", n);
    }
}

/**
 * Move cursor forward N columns
 */
void terminal_cursor_forward(int n)
{
    if (n > 0) {
        terminal_printf("\x1b[%dC", n);
    }
}

/**
 * Move cursor back N columns
 */
void terminal_cursor_back(int n)
{
    if (n > 0) {
        terminal_printf("\x1b[%dD", n);
    }
}

/**
 * Show or hide cursor
 */
void terminal_cursor_show(int visible)
{
    terminal_state_t *state = TERM_STATE();

    if (visible) {
        terminal_write_str("\x1b[?25h");
        if (state) {
            state->state_flags &= ~TERM_STATE_CURSOR_HIDDEN;
        }
    } else {
        terminal_write_str("\x1b[?25l");
        if (state) {
            state->state_flags |= TERM_STATE_CURSOR_HIDDEN;
        }
    }
}

/**
 * Enable/disable alternate screen buffer
 */
void terminal_alternate_screen(int enable)
{
    terminal_state_t *state = TERM_STATE();

    if (enable) {
        terminal_write_str("\x1b[?1049h");
        if (state) {
            state->state_flags |= TERM_STATE_ALT_SCREEN;
        }
    } else {
        terminal_write_str("\x1b[?1049l");
        if (state) {
            state->state_flags &= ~TERM_STATE_ALT_SCREEN;
        }
    }
}

/**
 * Get current cursor position
 * Returns 0 on success, -1 on failure
 */
int terminal_get_cursor_position(int *col, int *row)
{
    terminal_state_t *state = TERM_STATE();
    char buf[32];
    int i = 0;

    if (!state || !(state->state_flags & TERM_STATE_RAW)) {
        return -1;
    }

    /* Flush output first */
    terminal_flush_buffer();

    /* Request cursor position */
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }

    /* Read response: ESC [ rows ; cols R */
    while (i < (int)sizeof(buf) - 1) {
        fd_set fds;
        struct timeval tv = {0, 100000}; /* 100ms timeout */

        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) {
            return -1;
        }

        if (read(STDIN_FILENO, &buf[i], 1) != 1) {
            return -1;
        }

        if (buf[i] == 'R') {
            break;
        }
        i++;
    }
    buf[i] = '\0';

    /* Parse response */
    if (buf[0] != '\x1b' || buf[1] != '[') {
        return -1;
    }

    if (sscanf(&buf[2], "%d;%d", row, col) != 2) {
        return -1;
    }

    /* Convert to 0-indexed */
    (*row)--;
    (*col)--;

    return 0;
}

/* ============================================================================
 * UTF-8 HANDLING
 * ============================================================================ */

/**
 * Unicode width table for common character ranges
 * Returns display width of a Unicode codepoint
 */
static int unicode_char_width(uint32_t cp)
{
    /* ASCII control characters */
    if (cp < 32 || cp == 127) {
        return 0;
    }

    /* Standard ASCII */
    if (cp < 127) {
        return 1;
    }

    /* Common zero-width characters */
    if ((cp >= 0x0300 && cp <= 0x036F) ||  /* Combining Diacritical Marks */
        (cp >= 0x1AB0 && cp <= 0x1AFF) ||  /* Combining Diacritical Marks Extended */
        (cp >= 0x1DC0 && cp <= 0x1DFF) ||  /* Combining Diacritical Marks Supplement */
        (cp >= 0x20D0 && cp <= 0x20FF) ||  /* Combining Diacritical Marks for Symbols */
        (cp >= 0xFE00 && cp <= 0xFE0F) ||  /* Variation Selectors */
        (cp >= 0xFE20 && cp <= 0xFE2F) ||  /* Combining Half Marks */
        (cp == 0x200B) ||                   /* Zero Width Space */
        (cp == 0x200C) ||                   /* Zero Width Non-Joiner */
        (cp == 0x200D) ||                   /* Zero Width Joiner */
        (cp == 0xFEFF)) {                   /* BOM/ZWNBSP */
        return 0;
    }

    /* Wide characters (CJK, etc.) */
    if ((cp >= 0x1100 && cp <= 0x115F) ||   /* Hangul Jamo */
        (cp >= 0x2E80 && cp <= 0x9FFF) ||   /* CJK */
        (cp >= 0xAC00 && cp <= 0xD7A3) ||   /* Hangul Syllables */
        (cp >= 0xF900 && cp <= 0xFAFF) ||   /* CJK Compatibility Ideographs */
        (cp >= 0xFE10 && cp <= 0xFE1F) ||   /* Vertical Forms */
        (cp >= 0xFE30 && cp <= 0xFE6F) ||   /* CJK Compatibility Forms */
        (cp >= 0xFF00 && cp <= 0xFF60) ||   /* Fullwidth Forms */
        (cp >= 0xFFE0 && cp <= 0xFFE6) ||   /* Fullwidth Signs */
        (cp >= 0x20000 && cp <= 0x2FFFF) || /* CJK Extension B-G */
        (cp >= 0x30000 && cp <= 0x3FFFF)) { /* CJK Extension H */
        return 2;
    }

    /* Emoji (approximate - most emoji are wide) */
    if ((cp >= 0x1F300 && cp <= 0x1F9FF) || /* Miscellaneous Symbols and Pictographs, Emoticons, etc. */
        (cp >= 0x2600 && cp <= 0x26FF) ||   /* Miscellaneous Symbols */
        (cp >= 0x2700 && cp <= 0x27BF)) {   /* Dingbats */
        return 2;
    }

    return 1;
}

/**
 * Get width of a single UTF-8 character
 * Returns display width and sets bytes to the number of bytes consumed
 */
int terminal_utf8_char_width(const unsigned char *str, size_t len, size_t *bytes)
{
    uint32_t cp;

    if (len == 0 || !str) {
        *bytes = 0;
        return 0;
    }

    /* ASCII fast path */
    if (str[0] < 0x80) {
        *bytes = 1;
        return unicode_char_width(str[0]);
    }

    /* Decode UTF-8 */
    if ((str[0] & 0xE0) == 0xC0 && len >= 2) {
        /* 2-byte sequence */
        cp = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
        *bytes = 2;
    } else if ((str[0] & 0xF0) == 0xE0 && len >= 3) {
        /* 3-byte sequence */
        cp = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        *bytes = 3;
    } else if ((str[0] & 0xF8) == 0xF0 && len >= 4) {
        /* 4-byte sequence */
        cp = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) |
             ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        *bytes = 4;
    } else {
        /* Invalid UTF-8 - treat as 1 byte */
        *bytes = 1;
        return 1;
    }

    return unicode_char_width(cp);
}

/**
 * Calculate display width of a UTF-8 string
 */
size_t terminal_display_width(const char *str, size_t len)
{
    size_t width = 0;
    size_t pos = 0;

    while (pos < len) {
        size_t bytes;
        width += terminal_utf8_char_width((const unsigned char *)str + pos, len - pos, &bytes);
        pos += bytes;
    }

    return width;
}

/**
 * Count number of UTF-8 characters in a string
 */
size_t terminal_utf8_strlen(const char *str, size_t len)
{
    size_t count = 0;
    size_t pos = 0;

    while (pos < len) {
        size_t bytes;
        terminal_utf8_char_width((const unsigned char *)str + pos, len - pos, &bytes);
        count++;
        pos += bytes;
    }

    return count;
}

/**
 * Strip display width from string (calculate visible width excluding ANSI codes)
 */
static size_t display_width_strip_ansi(const char *str, size_t len)
{
    size_t width = 0;
    size_t pos = 0;

    while (pos < len) {
        /* Check for ANSI escape sequence */
        if (str[pos] == '\x1b' && pos + 1 < len && str[pos + 1] == '[') {
            /* Skip to end of sequence (letter or ~) */
            pos += 2;
            while (pos < len && !isalpha(str[pos]) && str[pos] != '~') {
                pos++;
            }
            if (pos < len) pos++; /* Skip final character */
            continue;
        }

        size_t bytes;
        width += terminal_utf8_char_width((const unsigned char *)str + pos, len - pos, &bytes);
        pos += bytes;
    }

    return width;
}

/* ============================================================================
 * COLOR AND STYLING
 * ============================================================================ */

/* Color name to ANSI code mapping */
typedef struct {
    const char *name;
    int fg_code;
    int bg_code;
} color_mapping_t;

static const color_mapping_t color_map[] = {
    {"black",         30, 40},
    {"red",           31, 41},
    {"green",         32, 42},
    {"yellow",        33, 43},
    {"blue",          34, 44},
    {"magenta",       35, 45},
    {"cyan",          36, 46},
    {"white",         37, 47},
    {"bright_black",  90, 100},
    {"bright_red",    91, 101},
    {"bright_green",  92, 102},
    {"bright_yellow", 93, 103},
    {"bright_blue",   94, 104},
    {"bright_magenta",95, 105},
    {"bright_cyan",   96, 106},
    {"bright_white",  97, 107},
    {"default",       39, 49},
    {NULL, 0, 0}
};

/**
 * Parse a color value and write ANSI code to buffer
 * Returns number of bytes written, or -1 on error
 */
int terminal_parse_color(zval *color, int is_bg, char *buf, size_t buf_size)
{
    terminal_state_t *state = TERM_STATE();
    int color_support = state ? state->color_support : COLOR_16;

    if (Z_TYPE_P(color) == IS_STRING) {
        const char *name = Z_STRVAL_P(color);
        size_t len = Z_STRLEN_P(color);

        /* Hex color (#RRGGBB or #RGB) */
        if (name[0] == '#' && (len == 7 || len == 4)) {
            int r, g, b;

            if (len == 7) {
                sscanf(name + 1, "%2x%2x%2x", &r, &g, &b);
            } else {
                int r1, g1, b1;
                sscanf(name + 1, "%1x%1x%1x", &r1, &g1, &b1);
                r = r1 * 17;
                g = g1 * 17;
                b = b1 * 17;
            }

            if (color_support >= COLOR_TRUECOLOR) {
                return snprintf(buf, buf_size, "%d;2;%d;%d;%d", is_bg ? 48 : 38, r, g, b);
            } else if (color_support >= COLOR_256) {
                /* Convert to 256-color approximation */
                int code = 16 + (r / 51) * 36 + (g / 51) * 6 + (b / 51);
                return snprintf(buf, buf_size, "%d;5;%d", is_bg ? 48 : 38, code);
            } else {
                /* Fall back to closest basic color */
                int bright = (r + g + b) > 384;
                int index = ((r > 127) ? 1 : 0) | ((g > 127) ? 2 : 0) | ((b > 127) ? 4 : 0);
                return snprintf(buf, buf_size, "%d", (is_bg ? 40 : 30) + index + (bright ? 60 : 0));
            }
        }

        /* Named color */
        for (const color_mapping_t *cm = color_map; cm->name; cm++) {
            if (strcasecmp(name, cm->name) == 0) {
                return snprintf(buf, buf_size, "%d", is_bg ? cm->bg_code : cm->fg_code);
            }
        }

        return -1; /* Unknown color name */
    }

    /* RGB array [r, g, b] */
    if (Z_TYPE_P(color) == IS_ARRAY) {
        zval *r_val, *g_val, *b_val;
        HashTable *ht = Z_ARRVAL_P(color);

        if (zend_hash_num_elements(ht) != 3) {
            return -1;
        }

        r_val = zend_hash_index_find(ht, 0);
        g_val = zend_hash_index_find(ht, 1);
        b_val = zend_hash_index_find(ht, 2);

        if (!r_val || !g_val || !b_val) {
            return -1;
        }

        int r = zval_get_long(r_val);
        int g = zval_get_long(g_val);
        int b = zval_get_long(b_val);

        /* Clamp values */
        r = r < 0 ? 0 : (r > 255 ? 255 : r);
        g = g < 0 ? 0 : (g > 255 ? 255 : g);
        b = b < 0 ? 0 : (b > 255 ? 255 : b);

        if (color_support >= COLOR_TRUECOLOR) {
            return snprintf(buf, buf_size, "%d;2;%d;%d;%d", is_bg ? 48 : 38, r, g, b);
        } else if (color_support >= COLOR_256) {
            int code = 16 + (r / 51) * 36 + (g / 51) * 6 + (b / 51);
            return snprintf(buf, buf_size, "%d;5;%d", is_bg ? 48 : 38, code);
        } else {
            int bright = (r + g + b) > 384;
            int index = ((r > 127) ? 1 : 0) | ((g > 127) ? 2 : 0) | ((b > 127) ? 4 : 0);
            return snprintf(buf, buf_size, "%d", (is_bg ? 40 : 30) + index + (bright ? 60 : 0));
        }
    }

    return -1;
}

/**
 * Apply styles to text
 */
void terminal_apply_style(const char *text, size_t len, zval *styles, zend_string **result)
{
    char codes[256] = "";
    size_t codes_len = 0;
    zval *val;

    if (Z_TYPE_P(styles) != IS_ARRAY) {
        *result = zend_string_init(text, len, 0);
        return;
    }

    HashTable *ht = Z_ARRVAL_P(styles);

    /* Foreground color */
    if ((val = zend_hash_str_find(ht, "fg", 2)) != NULL) {
        char color_code[32];
        int cc_len = terminal_parse_color(val, 0, color_code, sizeof(color_code));
        if (cc_len > 0) {
            codes_len += snprintf(codes + codes_len, sizeof(codes) - codes_len,
                                  "%s%s", codes_len > 0 ? ";" : "", color_code);
        }
    }

    /* Background color */
    if ((val = zend_hash_str_find(ht, "bg", 2)) != NULL) {
        char color_code[32];
        int cc_len = terminal_parse_color(val, 1, color_code, sizeof(color_code));
        if (cc_len > 0) {
            codes_len += snprintf(codes + codes_len, sizeof(codes) - codes_len,
                                  "%s%s", codes_len > 0 ? ";" : "", color_code);
        }
    }

    /* Style attributes */
    if ((val = zend_hash_str_find(ht, "bold", 4)) != NULL && zend_is_true(val)) {
        codes_len += snprintf(codes + codes_len, sizeof(codes) - codes_len, "%s1", codes_len > 0 ? ";" : "");
    }
    if ((val = zend_hash_str_find(ht, "dim", 3)) != NULL && zend_is_true(val)) {
        codes_len += snprintf(codes + codes_len, sizeof(codes) - codes_len, "%s2", codes_len > 0 ? ";" : "");
    }
    if ((val = zend_hash_str_find(ht, "italic", 6)) != NULL && zend_is_true(val)) {
        codes_len += snprintf(codes + codes_len, sizeof(codes) - codes_len, "%s3", codes_len > 0 ? ";" : "");
    }
    if ((val = zend_hash_str_find(ht, "underline", 9)) != NULL && zend_is_true(val)) {
        codes_len += snprintf(codes + codes_len, sizeof(codes) - codes_len, "%s4", codes_len > 0 ? ";" : "");
    }
    if ((val = zend_hash_str_find(ht, "blink", 5)) != NULL && zend_is_true(val)) {
        codes_len += snprintf(codes + codes_len, sizeof(codes) - codes_len, "%s5", codes_len > 0 ? ";" : "");
    }
    if ((val = zend_hash_str_find(ht, "reverse", 7)) != NULL && zend_is_true(val)) {
        codes_len += snprintf(codes + codes_len, sizeof(codes) - codes_len, "%s7", codes_len > 0 ? ";" : "");
    }

    if (codes_len == 0) {
        *result = zend_string_init(text, len, 0);
        return;
    }

    /* Build styled string: ESC[<codes>m<text>ESC[0m */
    size_t result_len = 2 + codes_len + 1 + len + 4; /* \x1b[ + codes + m + text + \x1b[0m */
    *result = zend_string_alloc(result_len, 0);

    char *p = ZSTR_VAL(*result);
    p += sprintf(p, "\x1b[%sm", codes);
    memcpy(p, text, len);
    p += len;
    memcpy(p, "\x1b[0m", 4);
    p += 4;
    *p = '\0';

    /* Adjust actual length */
    ZSTR_LEN(*result) = p - ZSTR_VAL(*result);
}

/* ============================================================================
 * TABLE RENDERING
 * ============================================================================ */

/* Box drawing character sets */
typedef struct {
    const char *h;   /* Horizontal */
    const char *v;   /* Vertical */
    const char *tl;  /* Top left */
    const char *tr;  /* Top right */
    const char *bl;  /* Bottom left */
    const char *br;  /* Bottom right */
    const char *lt;  /* Left T */
    const char *rt;  /* Right T */
    const char *tt;  /* Top T */
    const char *bt;  /* Bottom T */
    const char *c;   /* Cross */
} box_chars_t;

static const box_chars_t box_single = {
    BOX_HORIZONTAL, BOX_VERTICAL,
    BOX_TOP_LEFT, BOX_TOP_RIGHT, BOX_BOTTOM_LEFT, BOX_BOTTOM_RIGHT,
    BOX_LEFT_T, BOX_RIGHT_T, BOX_TOP_T, BOX_BOTTOM_T, BOX_CROSS
};

static const box_chars_t box_double = {
    BOX_HORIZONTAL_D, BOX_VERTICAL_D,
    BOX_TOP_LEFT_D, BOX_TOP_RIGHT_D, BOX_BOTTOM_LEFT_D, BOX_BOTTOM_RIGHT_D,
    BOX_LEFT_T_D, BOX_RIGHT_T_D, BOX_TOP_T_D, BOX_BOTTOM_T_D, BOX_CROSS_D
};

static const box_chars_t box_rounded = {
    BOX_HORIZONTAL, BOX_VERTICAL,
    BOX_TOP_LEFT_R, BOX_TOP_RIGHT_R, BOX_BOTTOM_LEFT_R, BOX_BOTTOM_RIGHT_R,
    BOX_LEFT_T, BOX_RIGHT_T, BOX_TOP_T, BOX_BOTTOM_T, BOX_CROSS
};

static const box_chars_t box_ascii = {
    BOX_HORIZONTAL_A, BOX_VERTICAL_A,
    BOX_CORNER_A, BOX_CORNER_A, BOX_CORNER_A, BOX_CORNER_A,
    BOX_CORNER_A, BOX_CORNER_A, BOX_CORNER_A, BOX_CORNER_A, BOX_CORNER_A
};

/**
 * Truncate string to fit within width, adding ellipsis
 */
static void truncate_string(const char *src, size_t src_len, size_t max_width,
                           char *dest, size_t dest_size, size_t *dest_len)
{
    if (max_width <= 3) {
        strncpy(dest, "...", dest_size);
        *dest_len = 3 < dest_size ? 3 : dest_size - 1;
        return;
    }

    size_t width = 0;
    size_t pos = 0;
    size_t last_good_pos = 0;

    while (pos < src_len && width < max_width - 3) {
        size_t bytes;
        int char_width = terminal_utf8_char_width((const unsigned char *)src + pos, src_len - pos, &bytes);

        if (width + char_width <= max_width - 3) {
            width += char_width;
            pos += bytes;
            last_good_pos = pos;
        } else {
            break;
        }
    }

    if (pos >= src_len) {
        /* String fits without truncation */
        memcpy(dest, src, src_len);
        *dest_len = src_len;
        dest[src_len] = '\0';
    } else {
        /* Need truncation */
        memcpy(dest, src, last_good_pos);
        memcpy(dest + last_good_pos, "...", 3);
        *dest_len = last_good_pos + 3;
        dest[*dest_len] = '\0';
    }
}

/**
 * Render a table to the terminal
 */
void terminal_render_table(zval *headers, zval *rows, zval *options)
{
    terminal_state_t *state = TERM_STATE();
    HashTable *headers_ht, *rows_ht, *options_ht = NULL;
    zval *val;
    uint32_t num_cols;
    size_t *col_widths = NULL;
    int padding = 1;
    int border_style = BORDER_SINGLE;
    const box_chars_t *box = &box_single;
    int max_width = state ? state->cols : 80;
    int truncate = 1;
    zval *header_style = NULL;
    zval **align = NULL;

    /* Validate inputs */
    if (Z_TYPE_P(headers) != IS_ARRAY) {
        zend_throw_exception(terminal_exception_ce, "Headers must be an array", 0);
        return;
    }
    if (Z_TYPE_P(rows) != IS_ARRAY) {
        zend_throw_exception(terminal_exception_ce, "Rows must be an array", 0);
        return;
    }

    headers_ht = Z_ARRVAL_P(headers);
    rows_ht = Z_ARRVAL_P(rows);
    num_cols = zend_hash_num_elements(headers_ht);

    if (num_cols == 0) {
        return; /* Empty table */
    }

    /* Parse options */
    if (options && Z_TYPE_P(options) == IS_ARRAY) {
        options_ht = Z_ARRVAL_P(options);

        if ((val = zend_hash_str_find(options_ht, "padding", 7)) != NULL) {
            padding = zval_get_long(val);
            if (padding < 0) padding = 0;
            if (padding > 5) padding = 5;
        }

        if ((val = zend_hash_str_find(options_ht, "border", 6)) != NULL && Z_TYPE_P(val) == IS_STRING) {
            const char *bs = Z_STRVAL_P(val);
            if (strcmp(bs, "none") == 0) {
                border_style = BORDER_NONE;
            } else if (strcmp(bs, "ascii") == 0) {
                border_style = BORDER_ASCII;
                box = &box_ascii;
            } else if (strcmp(bs, "double") == 0) {
                border_style = BORDER_DOUBLE;
                box = &box_double;
            } else if (strcmp(bs, "rounded") == 0) {
                border_style = BORDER_ROUNDED;
                box = &box_rounded;
            } else {
                border_style = BORDER_SINGLE;
                box = &box_single;
            }
        }

        if ((val = zend_hash_str_find(options_ht, "maxWidth", 8)) != NULL && Z_TYPE_P(val) == IS_LONG) {
            int mw = Z_LVAL_P(val);
            if (mw > 0 && mw < max_width) {
                max_width = mw;
            }
        }

        if ((val = zend_hash_str_find(options_ht, "truncate", 8)) != NULL) {
            truncate = zend_is_true(val);
        }

        if ((val = zend_hash_str_find(options_ht, "headerStyle", 11)) != NULL && Z_TYPE_P(val) == IS_ARRAY) {
            header_style = val;
        }

        if ((val = zend_hash_str_find(options_ht, "align", 5)) != NULL && Z_TYPE_P(val) == IS_ARRAY) {
            align = ecalloc(num_cols, sizeof(zval *));
            uint32_t i = 0;
            zval *a;
            ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(val), a) {
                if (i < num_cols) {
                    align[i] = a;
                }
                i++;
            } ZEND_HASH_FOREACH_END();
        }
    }

    /* Calculate column widths */
    col_widths = ecalloc(num_cols, sizeof(size_t));

    /* Header widths */
    {
        uint32_t i = 0;
        zval *header;
        ZEND_HASH_FOREACH_VAL(headers_ht, header) {
            if (i >= num_cols) break;
            zend_string *str = zval_get_string(header);
            col_widths[i] = terminal_display_width(ZSTR_VAL(str), ZSTR_LEN(str));
            zend_string_release(str);
            i++;
        } ZEND_HASH_FOREACH_END();
    }

    /* Row widths */
    {
        zval *row;
        ZEND_HASH_FOREACH_VAL(rows_ht, row) {
            if (Z_TYPE_P(row) != IS_ARRAY) continue;

            uint32_t i = 0;
            zval *cell;
            ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(row), cell) {
                if (i >= num_cols) break;
                zend_string *str = zval_get_string(cell);
                size_t w = terminal_display_width(ZSTR_VAL(str), ZSTR_LEN(str));
                if (w > col_widths[i]) {
                    col_widths[i] = w;
                }
                zend_string_release(str);
                i++;
            } ZEND_HASH_FOREACH_END();
        } ZEND_HASH_FOREACH_END();
    }

    /* Calculate total width and adjust if needed */
    size_t total_width = (border_style != BORDER_NONE) ? 1 : 0; /* Left border */
    for (uint32_t i = 0; i < num_cols; i++) {
        total_width += col_widths[i] + (padding * 2);
        if (border_style != BORDER_NONE) {
            total_width += 1; /* Column separator */
        }
    }

    /* Truncate columns if table is too wide */
    if (truncate && (int)total_width > max_width && num_cols > 0) {
        int excess = total_width - max_width;
        /* Reduce largest columns first */
        while (excess > 0) {
            size_t max_col_width = 0;
            uint32_t max_col = 0;
            for (uint32_t i = 0; i < num_cols; i++) {
                if (col_widths[i] > max_col_width) {
                    max_col_width = col_widths[i];
                    max_col = i;
                }
            }
            if (max_col_width <= 3) break; /* Can't reduce further */
            col_widths[max_col]--;
            excess--;
        }
    }

    /* Render table */

    /* Top border */
    if (border_style != BORDER_NONE) {
        terminal_write_str(box->tl);
        for (uint32_t i = 0; i < num_cols; i++) {
            for (size_t j = 0; j < col_widths[i] + (padding * 2); j++) {
                terminal_write_str(box->h);
            }
            if (i < num_cols - 1) {
                terminal_write_str(box->tt);
            }
        }
        terminal_write_str(box->tr);
        terminal_write_str("\n");
    }

    /* Header row */
    {
        if (border_style != BORDER_NONE) {
            terminal_write_str(box->v);
        }

        uint32_t i = 0;
        zval *header;
        ZEND_HASH_FOREACH_VAL(headers_ht, header) {
            if (i >= num_cols) break;

            zend_string *str = zval_get_string(header);
            size_t display_w = display_width_strip_ansi(ZSTR_VAL(str), ZSTR_LEN(str));

            /* Truncate if needed */
            char *text = ZSTR_VAL(str);
            size_t text_len = ZSTR_LEN(str);
            char truncated[256];
            size_t trunc_len;

            if (display_w > col_widths[i]) {
                truncate_string(ZSTR_VAL(str), ZSTR_LEN(str), col_widths[i],
                               truncated, sizeof(truncated), &trunc_len);
                text = truncated;
                text_len = trunc_len;
                display_w = terminal_display_width(text, text_len);
            }

            /* Padding */
            for (int p = 0; p < padding; p++) {
                terminal_write_str(" ");
            }

            /* Apply header style */
            if (header_style) {
                zend_string *styled;
                terminal_apply_style(text, text_len, header_style, &styled);
                terminal_write(ZSTR_VAL(styled), ZSTR_LEN(styled));
                zend_string_release(styled);
            } else {
                terminal_write(text, text_len);
            }

            /* Right padding */
            size_t pad_right = col_widths[i] - display_w + padding;
            for (size_t p = 0; p < pad_right; p++) {
                terminal_write_str(" ");
            }

            if (border_style != BORDER_NONE) {
                terminal_write_str(box->v);
            }

            zend_string_release(str);
            i++;
        } ZEND_HASH_FOREACH_END();

        terminal_write_str("\n");
    }

    /* Header separator */
    if (border_style != BORDER_NONE) {
        terminal_write_str(box->lt);
        for (uint32_t i = 0; i < num_cols; i++) {
            for (size_t j = 0; j < col_widths[i] + (padding * 2); j++) {
                terminal_write_str(box->h);
            }
            if (i < num_cols - 1) {
                terminal_write_str(box->c);
            }
        }
        terminal_write_str(box->rt);
        terminal_write_str("\n");
    }

    /* Data rows */
    {
        zval *row;
        ZEND_HASH_FOREACH_VAL(rows_ht, row) {
            if (Z_TYPE_P(row) != IS_ARRAY) continue;

            if (border_style != BORDER_NONE) {
                terminal_write_str(box->v);
            }

            uint32_t i = 0;
            zval *cell;
            ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(row), cell) {
                if (i >= num_cols) break;

                zend_string *str = zval_get_string(cell);
                size_t display_w = display_width_strip_ansi(ZSTR_VAL(str), ZSTR_LEN(str));

                /* Truncate if needed */
                char *text = ZSTR_VAL(str);
                size_t text_len = ZSTR_LEN(str);
                char truncated[256];
                size_t trunc_len;

                if (display_w > col_widths[i]) {
                    truncate_string(ZSTR_VAL(str), ZSTR_LEN(str), col_widths[i],
                                   truncated, sizeof(truncated), &trunc_len);
                    text = truncated;
                    text_len = trunc_len;
                    display_w = terminal_display_width(text, text_len);
                }

                /* Determine alignment */
                int align_right = 0;
                int align_center = 0;
                if (align && align[i] && Z_TYPE_P(align[i]) == IS_STRING) {
                    if (strcmp(Z_STRVAL_P(align[i]), "right") == 0) {
                        align_right = 1;
                    } else if (strcmp(Z_STRVAL_P(align[i]), "center") == 0) {
                        align_center = 1;
                    }
                }

                size_t space = col_widths[i] - display_w;
                size_t pad_left = padding;
                size_t pad_right = padding;

                if (align_right) {
                    pad_left += space;
                } else if (align_center) {
                    pad_left += space / 2;
                    pad_right += space - (space / 2);
                } else {
                    pad_right += space;
                }

                /* Left padding */
                for (size_t p = 0; p < pad_left; p++) {
                    terminal_write_str(" ");
                }

                terminal_write(text, text_len);

                /* Right padding */
                for (size_t p = 0; p < pad_right; p++) {
                    terminal_write_str(" ");
                }

                if (border_style != BORDER_NONE) {
                    terminal_write_str(box->v);
                }

                zend_string_release(str);
                i++;
            } ZEND_HASH_FOREACH_END();

            /* Pad missing columns */
            while (i < num_cols) {
                for (size_t p = 0; p < col_widths[i] + (padding * 2); p++) {
                    terminal_write_str(" ");
                }
                if (border_style != BORDER_NONE) {
                    terminal_write_str(box->v);
                }
                i++;
            }

            terminal_write_str("\n");
        } ZEND_HASH_FOREACH_END();
    }

    /* Bottom border */
    if (border_style != BORDER_NONE) {
        terminal_write_str(box->bl);
        for (uint32_t i = 0; i < num_cols; i++) {
            for (size_t j = 0; j < col_widths[i] + (padding * 2); j++) {
                terminal_write_str(box->h);
            }
            if (i < num_cols - 1) {
                terminal_write_str(box->bt);
            }
        }
        terminal_write_str(box->br);
        terminal_write_str("\n");
    }

    terminal_flush_buffer();

    /* Cleanup */
    efree(col_widths);
    if (align) {
        efree(align);
    }
}

/* ============================================================================
 * INPUT HANDLING
 * ============================================================================ */

/**
 * Read a single keypress with optional timeout
 * Returns: 1 on success, 0 on timeout, -1 on error
 * key_char: buffer for the character (UTF-8)
 * char_len: length of character in bytes
 * key_name: buffer for key name (e.g., "up", "enter")
 */
int terminal_read_key(double timeout, char *key_char, size_t *char_len, char *key_name, size_t name_size)
{
    terminal_state_t *state = TERM_STATE();
    fd_set fds;
    struct timeval tv;
    unsigned char buf[INPUT_BUFFER_SIZE];
    ssize_t nread;

    if (!state || !(state->state_flags & TERM_STATE_RAW)) {
        return -1;
    }

    /* Flush output before reading */
    terminal_flush_buffer();

    /* Setup select with timeout */
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    if (timeout < 0) {
        /* Blocking read */
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, NULL) <= 0) {
            return -1;
        }
    } else {
        tv.tv_sec = (time_t)timeout;
        tv.tv_usec = (suseconds_t)((timeout - tv.tv_sec) * 1000000);

        int result = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        if (result < 0) {
            return -1;
        }
        if (result == 0) {
            return 0; /* Timeout */
        }
    }

    /* Read available bytes */
    nread = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (nread <= 0) {
        return -1;
    }
    buf[nread] = '\0';

    /* Initialize outputs */
    key_char[0] = '\0';
    *char_len = 0;
    key_name[0] = '\0';

    /* Parse input */
    if (buf[0] == '\x1b') {
        /* Escape sequence */
        if (nread == 1) {
            strncpy(key_name, "esc", name_size);
            return 1;
        }

        if (buf[1] == '[') {
            /* CSI sequence */
            switch (buf[2]) {
                case 'A': strncpy(key_name, "up", name_size); return 1;
                case 'B': strncpy(key_name, "down", name_size); return 1;
                case 'C': strncpy(key_name, "right", name_size); return 1;
                case 'D': strncpy(key_name, "left", name_size); return 1;
                case 'H': strncpy(key_name, "home", name_size); return 1;
                case 'F': strncpy(key_name, "end", name_size); return 1;
                case '1':
                    if (nread >= 4 && buf[3] == '~') {
                        strncpy(key_name, "home", name_size);
                        return 1;
                    }
                    /* Function keys F1-F4 */
                    if (nread >= 5 && buf[4] == '~') {
                        int fn = buf[3] - '0';
                        if (fn >= 5 && fn <= 8) {
                            snprintf(key_name, name_size, "f%d", fn - 4);
                            return 1;
                        }
                    }
                    break;
                case '2':
                    if (nread >= 4 && buf[3] == '~') {
                        strncpy(key_name, "insert", name_size);
                        return 1;
                    }
                    /* F9-F12 */
                    if (nread >= 5 && buf[4] == '~') {
                        int code = (buf[3] - '0');
                        if (code >= 0 && code <= 4) {
                            snprintf(key_name, name_size, "f%d", code + 9);
                            return 1;
                        }
                    }
                    break;
                case '3':
                    if (nread >= 4 && buf[3] == '~') {
                        strncpy(key_name, "delete", name_size);
                        return 1;
                    }
                    break;
                case '4':
                    if (nread >= 4 && buf[3] == '~') {
                        strncpy(key_name, "end", name_size);
                        return 1;
                    }
                    break;
                case '5':
                    if (nread >= 4 && buf[3] == '~') {
                        strncpy(key_name, "pageup", name_size);
                        return 1;
                    }
                    break;
                case '6':
                    if (nread >= 4 && buf[3] == '~') {
                        strncpy(key_name, "pagedown", name_size);
                        return 1;
                    }
                    break;
            }
        } else if (buf[1] == 'O') {
            /* SS3 sequence (function keys on some terminals) */
            switch (buf[2]) {
                case 'P': strncpy(key_name, "f1", name_size); return 1;
                case 'Q': strncpy(key_name, "f2", name_size); return 1;
                case 'R': strncpy(key_name, "f3", name_size); return 1;
                case 'S': strncpy(key_name, "f4", name_size); return 1;
            }
        }

        /* Unknown escape sequence */
        strncpy(key_name, "esc", name_size);
        return 1;
    }

    /* Control characters */
    if (buf[0] < 32) {
        switch (buf[0]) {
            case '\r':
            case '\n':
                strncpy(key_name, "enter", name_size);
                break;
            case '\t':
                strncpy(key_name, "tab", name_size);
                break;
            case 127:
            case '\b':
                strncpy(key_name, "backspace", name_size);
                break;
            default:
                /* Ctrl+letter */
                snprintf(key_name, name_size, "ctrl+%c", buf[0] + 'a' - 1);
                break;
        }
        return 1;
    }

    /* DEL character */
    if (buf[0] == 127) {
        strncpy(key_name, "backspace", name_size);
        return 1;
    }

    /* Regular character (possibly UTF-8) */
    size_t bytes;
    terminal_utf8_char_width(buf, nread, &bytes);

    if (bytes > 0 && bytes <= 4) {
        memcpy(key_char, buf, bytes);
        key_char[bytes] = '\0';
        *char_len = bytes;
        strncpy(key_name, "char", name_size);
    }

    return 1;
}

/* ============================================================================
 * PROGRESS BAR OBJECT
 * ============================================================================ */

static zend_object *progress_bar_create(zend_class_entry *ce)
{
    progress_bar_t *bar = zend_object_alloc(sizeof(progress_bar_t), ce);

    bar->total = 100;
    bar->current = 0;
    bar->label = NULL;
    bar->label_len = 0;
    bar->finished = 0;
    bar->last_rendered_width = 0;
    clock_gettime(CLOCK_MONOTONIC, &bar->start_time);

    zend_object_std_init(&bar->std, ce);
    object_properties_init(&bar->std, ce);
    bar->std.handlers = &progress_bar_handlers;

    return &bar->std;
}

static void progress_bar_free(zend_object *object)
{
    progress_bar_t *bar = (progress_bar_t *)((char *)object - XtOffsetOf(progress_bar_t, std));

    if (bar->label) {
        efree(bar->label);
    }

    zend_object_std_dtor(&bar->std);
}

static inline progress_bar_t *progress_bar_from_zval(zval *zv)
{
    return (progress_bar_t *)((char *)Z_OBJ_P(zv) - XtOffsetOf(progress_bar_t, std));
}

static void progress_bar_render(progress_bar_t *bar)
{
    terminal_state_t *state = TERM_STATE();
    int width = state ? state->cols : 80;
    struct timespec now;
    double elapsed;
    double rate;
    int eta_sec;

    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsed = (now.tv_sec - bar->start_time.tv_sec) +
              (now.tv_nsec - bar->start_time.tv_nsec) / 1e9;

    /* Calculate rate and ETA */
    rate = elapsed > 0 ? bar->current / elapsed : 0;
    eta_sec = rate > 0 ? (int)((bar->total - bar->current) / rate) : 0;

    /* Format: [====>     ] 42% (42/100) 5.2/s ETA: 00:12 */
    char info[128];
    int info_len = snprintf(info, sizeof(info), " %d%% (%d/%d) %.1f/s ETA: %02d:%02d",
                            bar->total > 0 ? (bar->current * 100 / bar->total) : 0,
                            bar->current, bar->total, rate,
                            eta_sec / 60, eta_sec % 60);

    int label_width = bar->label_len > 0 ? bar->label_len + 1 : 0;
    int bar_width = width - label_width - info_len - 3; /* 3 for [ ] */

    if (bar_width < 10) bar_width = 10;

    int filled = bar->total > 0 ? (bar->current * bar_width / bar->total) : 0;
    if (filled > bar_width) filled = bar_width;

    /* Clear line and render */
    terminal_write_str("\r\x1b[K");

    if (bar->label && bar->label_len > 0) {
        terminal_write(bar->label, bar->label_len);
        terminal_write_str(" ");
    }

    terminal_write_str("[");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            terminal_write_str("=");
        } else if (i == filled) {
            terminal_write_str(">");
        } else {
            terminal_write_str(" ");
        }
    }
    terminal_write_str("]");
    terminal_write(info, info_len);

    terminal_flush_buffer();
}

/* ============================================================================
 * LOADER/SPINNER OBJECT
 * ============================================================================ */

/* Spinner frame arrays */
static const char *spinner_dots_frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏", NULL};
static const char *spinner_line_frames[] = {"-", "\\", "|", "/", NULL};
static const char *spinner_arrow_frames[] = {"←", "↖", "↑", "↗", "→", "↘", "↓", "↙", NULL};

static int count_frames(const char **frames)
{
    int count = 0;
    while (frames[count]) count++;
    return count;
}

static zend_object *loader_create(zend_class_entry *ce)
{
    loader_t *loader = zend_object_alloc(sizeof(loader_t), ce);

    loader->message = NULL;
    loader->message_len = 0;
    loader->style = NULL;
    loader->frame = 0;
    loader->running = 0;
    loader->frame_count = 0;

    zend_object_std_init(&loader->std, ce);
    object_properties_init(&loader->std, ce);
    loader->std.handlers = &loader_handlers;

    return &loader->std;
}

static void loader_free(zend_object *object)
{
    loader_t *loader = (loader_t *)((char *)object - XtOffsetOf(loader_t, std));

    if (loader->message) {
        efree(loader->message);
    }

    zend_object_std_dtor(&loader->std);
}

static inline loader_t *loader_from_zval(zval *zv)
{
    return (loader_t *)((char *)Z_OBJ_P(zv) - XtOffsetOf(loader_t, std));
}

static void loader_render_frame(loader_t *loader)
{
    if (!loader->running) return;

    const char **frames;
    if (loader->style && strcmp(loader->style, "line") == 0) {
        frames = spinner_line_frames;
    } else if (loader->style && strcmp(loader->style, "arrow") == 0) {
        frames = spinner_arrow_frames;
    } else {
        frames = spinner_dots_frames;
    }

    if (loader->frame_count == 0) {
        loader->frame_count = count_frames(frames);
    }

    terminal_write_str("\r\x1b[K");
    terminal_write_str(frames[loader->frame % loader->frame_count]);
    terminal_write_str(" ");
    if (loader->message && loader->message_len > 0) {
        terminal_write(loader->message, loader->message_len);
    }
    terminal_flush_buffer();

    loader->frame++;
}

/* ============================================================================
 * PHP FUNCTION IMPLEMENTATIONS
 * ============================================================================ */

/* {{{ proto void Terminal::enter()
   Enter raw terminal mode */
PHP_METHOD(Terminal, enter)
{
    ZEND_PARSE_PARAMETERS_NONE();

    if (terminal_enter_raw() != 0) {
        TERM_THROW("Failed to enter raw mode: terminal may not be a TTY");
    }
    /* Shutdown is handled by RSHUTDOWN and signal handlers */
}
/* }}} */

/* {{{ proto void Terminal::exit()
   Exit raw terminal mode */
PHP_METHOD(Terminal, exit)
{
    ZEND_PARSE_PARAMETERS_NONE();

    if (terminal_exit_raw() != 0) {
        TERM_THROW("Failed to restore terminal settings");
    }
}
/* }}} */

/* {{{ proto array Terminal::size()
   Get terminal size */
PHP_METHOD(Terminal, size)
{
    terminal_state_t *state = TERM_STATE();

    ZEND_PARSE_PARAMETERS_NONE();

    /* Check for pending resize */
    if (state && state->resize_pending) {
        terminal_update_size();
        state->resize_pending = 0;

        /* Call PHP callback if registered */
        if (!Z_ISUNDEF(state->resize_callback)) {
            zval retval;
            if (call_user_function(NULL, NULL, &state->resize_callback, &retval, 0, NULL) == SUCCESS) {
                zval_ptr_dtor(&retval);
            }
        }
    }

    array_init(return_value);
    add_assoc_long(return_value, "cols", state ? state->cols : 80);
    add_assoc_long(return_value, "rows", state ? state->rows : 24);
}
/* }}} */

/* {{{ proto bool Terminal::supportsColor()
   Check if terminal supports basic colors */
PHP_METHOD(Terminal, supportsColor)
{
    terminal_state_t *state = TERM_STATE();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(state && state->color_support >= COLOR_16);
}
/* }}} */

/* {{{ proto bool Terminal::supports256Color()
   Check if terminal supports 256 colors */
PHP_METHOD(Terminal, supports256Color)
{
    terminal_state_t *state = TERM_STATE();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(state && state->color_support >= COLOR_256);
}
/* }}} */

/* {{{ proto bool Terminal::supportsTrueColor()
   Check if terminal supports true color (24-bit) */
PHP_METHOD(Terminal, supportsTrueColor)
{
    terminal_state_t *state = TERM_STATE();

    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL(state && state->color_support >= COLOR_TRUECOLOR);
}
/* }}} */

/* {{{ proto void Terminal::clear()
   Clear the screen */
PHP_METHOD(Terminal, clear)
{
    ZEND_PARSE_PARAMETERS_NONE();

    terminal_clear_screen();
    terminal_flush_buffer();
}
/* }}} */

/* {{{ proto void Terminal::clearLine()
   Clear the current line */
PHP_METHOD(Terminal, clearLine)
{
    ZEND_PARSE_PARAMETERS_NONE();

    terminal_clear_line();
    terminal_flush_buffer();
}
/* }}} */

/* {{{ proto void Terminal::alternateScreen(bool $enable)
   Enable or disable alternate screen buffer */
PHP_METHOD(Terminal, alternateScreen)
{
    zend_bool enable;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    terminal_alternate_screen(enable);
    terminal_flush_buffer();
}
/* }}} */

/* {{{ proto void Terminal::cursor(bool $visible)
   Show or hide cursor */
PHP_METHOD(Terminal, cursor)
{
    zend_bool visible;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(visible)
    ZEND_PARSE_PARAMETERS_END();

    terminal_cursor_show(visible);
    terminal_flush_buffer();
}
/* }}} */

/* {{{ proto void Terminal::cursorTo(int $col, int $row)
   Move cursor to position (0-indexed) */
PHP_METHOD(Terminal, cursorTo)
{
    zend_long col, row;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_LONG(col)
        Z_PARAM_LONG(row)
    ZEND_PARSE_PARAMETERS_END();

    terminal_cursor_to((int)col, (int)row);
    terminal_flush_buffer();
}
/* }}} */

/* {{{ proto void Terminal::cursorUp(int $n = 1)
   Move cursor up */
PHP_METHOD(Terminal, cursorUp)
{
    zend_long n = 1;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(n)
    ZEND_PARSE_PARAMETERS_END();

    terminal_cursor_up((int)n);
    terminal_flush_buffer();
}
/* }}} */

/* {{{ proto void Terminal::cursorDown(int $n = 1)
   Move cursor down */
PHP_METHOD(Terminal, cursorDown)
{
    zend_long n = 1;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(n)
    ZEND_PARSE_PARAMETERS_END();

    terminal_cursor_down((int)n);
    terminal_flush_buffer();
}
/* }}} */

/* {{{ proto void Terminal::cursorForward(int $n = 1)
   Move cursor forward */
PHP_METHOD(Terminal, cursorForward)
{
    zend_long n = 1;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(n)
    ZEND_PARSE_PARAMETERS_END();

    terminal_cursor_forward((int)n);
    terminal_flush_buffer();
}
/* }}} */

/* {{{ proto void Terminal::cursorBack(int $n = 1)
   Move cursor back */
PHP_METHOD(Terminal, cursorBack)
{
    zend_long n = 1;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(n)
    ZEND_PARSE_PARAMETERS_END();

    terminal_cursor_back((int)n);
    terminal_flush_buffer();
}
/* }}} */

/* {{{ proto array Terminal::cursorPosition()
   Get current cursor position */
PHP_METHOD(Terminal, cursorPosition)
{
    int col, row;

    ZEND_PARSE_PARAMETERS_NONE();

    if (terminal_get_cursor_position(&col, &row) != 0) {
        TERM_THROW("Failed to get cursor position");
    }

    array_init(return_value);
    add_assoc_long(return_value, "col", col);
    add_assoc_long(return_value, "row", row);
}
/* }}} */

/* {{{ proto void Terminal::onResize(callable $callback)
   Register callback for terminal resize */
PHP_METHOD(Terminal, onResize)
{
    zval *callback;
    terminal_state_t *state = TERM_STATE();

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ZVAL(callback)
    ZEND_PARSE_PARAMETERS_END();

    if (!state) {
        TERM_THROW("Terminal not initialized");
    }

    if (!zend_is_callable(callback, 0, NULL)) {
        TERM_THROW("Callback must be callable");
    }

    if (!Z_ISUNDEF(state->resize_callback)) {
        zval_ptr_dtor(&state->resize_callback);
    }

    ZVAL_COPY(&state->resize_callback, callback);
}
/* }}} */

/* {{{ proto string Terminal::style(string $text, array $styles)
   Apply styles to text */
PHP_METHOD(Terminal, style)
{
    zend_string *text;
    zval *styles;
    zend_string *result;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STR(text)
        Z_PARAM_ARRAY(styles)
    ZEND_PARSE_PARAMETERS_END();

    terminal_apply_style(ZSTR_VAL(text), ZSTR_LEN(text), styles, &result);
    RETURN_STR(result);
}
/* }}} */

/* {{{ proto void Terminal::table(array $headers, array $rows, array $options = [])
   Render a table */
PHP_METHOD(Terminal, table)
{
    zval *headers, *rows, *options = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_ARRAY(headers)
        Z_PARAM_ARRAY(rows)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_OR_NULL(options)
    ZEND_PARSE_PARAMETERS_END();

    terminal_render_table(headers, rows, options);
}
/* }}} */

/* {{{ proto ?array Terminal::readKey(?float $timeout = null)
   Read a single keypress */
PHP_METHOD(Terminal, readKey)
{
    double timeout = -1;
    zend_bool timeout_is_null = 1;
    char key_char[8] = "";
    size_t char_len = 0;
    char key_name[32] = "";

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_DOUBLE_OR_NULL(timeout, timeout_is_null)
    ZEND_PARSE_PARAMETERS_END();

    if (timeout_is_null) {
        timeout = -1; /* Blocking */
    }

    int result = terminal_read_key(timeout, key_char, &char_len, key_name, sizeof(key_name));

    if (result < 0) {
        TERM_THROW("Failed to read key");
    }

    if (result == 0) {
        RETURN_NULL(); /* Timeout */
    }

    array_init(return_value);

    if (char_len > 0) {
        add_assoc_stringl(return_value, "char", key_char, char_len);
    }
    add_assoc_string(return_value, "key", key_name);
}
/* }}} */

/* {{{ proto ?string Terminal::select(string $prompt, array $options, int $default = 0)
   Single-select UI */
PHP_METHOD(Terminal, select)
{
    zend_string *prompt;
    zval *options;
    zend_long default_idx = 0;
    terminal_state_t *state = TERM_STATE();

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_STR(prompt)
        Z_PARAM_ARRAY(options)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(default_idx)
    ZEND_PARSE_PARAMETERS_END();

    if (!state || !(state->state_flags & TERM_STATE_RAW)) {
        TERM_THROW("Terminal must be in raw mode for select()");
    }

    HashTable *opts_ht = Z_ARRVAL_P(options);
    int num_options = zend_hash_num_elements(opts_ht);

    if (num_options == 0) {
        RETURN_NULL();
    }

    int selected = (int)default_idx;
    if (selected < 0) selected = 0;
    if (selected >= num_options) selected = num_options - 1;

    /* Build options array */
    char **opt_strings = ecalloc(num_options, sizeof(char *));
    size_t *opt_lens = ecalloc(num_options, sizeof(size_t));

    {
        int i = 0;
        zval *opt;
        ZEND_HASH_FOREACH_VAL(opts_ht, opt) {
            zend_string *str = zval_get_string(opt);
            opt_strings[i] = estrndup(ZSTR_VAL(str), ZSTR_LEN(str));
            opt_lens[i] = ZSTR_LEN(str);
            zend_string_release(str);
            i++;
        } ZEND_HASH_FOREACH_END();
    }

    terminal_cursor_show(0);

    /* Draw initial state */
    terminal_write(ZSTR_VAL(prompt), ZSTR_LEN(prompt));
    terminal_write_str("\n");

    int running = 1;
    int cancelled = 0;

    while (running) {
        /* Draw options */
        for (int i = 0; i < num_options; i++) {
            terminal_write_str("  ");
            if (i == selected) {
                terminal_write_str("\x1b[36m● ");  /* Cyan bullet */
            } else {
                terminal_write_str("○ ");
            }
            terminal_write(opt_strings[i], opt_lens[i]);
            if (i == selected) {
                terminal_write_str("  ←\x1b[0m");
            }
            terminal_write_str("\n");
        }
        terminal_flush_buffer();

        /* Read key */
        char key_char[8];
        size_t char_len;
        char key_name[32];

        int result = terminal_read_key(-1, key_char, &char_len, key_name, sizeof(key_name));

        if (result > 0) {
            if (strcmp(key_name, "up") == 0) {
                selected = (selected - 1 + num_options) % num_options;
            } else if (strcmp(key_name, "down") == 0) {
                selected = (selected + 1) % num_options;
            } else if (strcmp(key_name, "enter") == 0) {
                running = 0;
            } else if (strcmp(key_name, "esc") == 0 || strcmp(key_name, "ctrl+c") == 0) {
                running = 0;
                cancelled = 1;
            }
        }

        /* Move cursor back up to redraw */
        if (running) {
            terminal_cursor_up(num_options);
        }
    }

    terminal_cursor_show(1);

    /* Return result */
    zend_string *result_str = NULL;
    if (!cancelled) {
        result_str = zend_string_init(opt_strings[selected], opt_lens[selected], 0);
    }

    /* Cleanup */
    for (int i = 0; i < num_options; i++) {
        efree(opt_strings[i]);
    }
    efree(opt_strings);
    efree(opt_lens);

    if (result_str) {
        RETURN_STR(result_str);
    }
    RETURN_NULL();
}
/* }}} */

/* {{{ proto ?array Terminal::multiSelect(string $prompt, array $options, array $defaults = [])
   Multi-select UI */
PHP_METHOD(Terminal, multiSelect)
{
    zend_string *prompt;
    zval *options;
    zval *defaults = NULL;
    terminal_state_t *state = TERM_STATE();

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_STR(prompt)
        Z_PARAM_ARRAY(options)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_OR_NULL(defaults)
    ZEND_PARSE_PARAMETERS_END();

    if (!state || !(state->state_flags & TERM_STATE_RAW)) {
        TERM_THROW("Terminal must be in raw mode for multiSelect()");
    }

    HashTable *opts_ht = Z_ARRVAL_P(options);
    int num_options = zend_hash_num_elements(opts_ht);

    if (num_options == 0) {
        array_init(return_value);
        return;
    }

    /* Build options array */
    char **opt_strings = ecalloc(num_options, sizeof(char *));
    size_t *opt_lens = ecalloc(num_options, sizeof(size_t));
    int *selected_flags = ecalloc(num_options, sizeof(int));

    {
        int i = 0;
        zval *opt;
        ZEND_HASH_FOREACH_VAL(opts_ht, opt) {
            zend_string *str = zval_get_string(opt);
            opt_strings[i] = estrndup(ZSTR_VAL(str), ZSTR_LEN(str));
            opt_lens[i] = ZSTR_LEN(str);
            zend_string_release(str);
            i++;
        } ZEND_HASH_FOREACH_END();
    }

    /* Process defaults */
    if (defaults && Z_TYPE_P(defaults) == IS_ARRAY) {
        zval *def;
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(defaults), def) {
            zend_long idx = zval_get_long(def);
            if (idx >= 0 && idx < num_options) {
                selected_flags[idx] = 1;
            }
        } ZEND_HASH_FOREACH_END();
    }

    int cursor = 0;

    terminal_cursor_show(0);

    /* Draw prompt */
    terminal_write(ZSTR_VAL(prompt), ZSTR_LEN(prompt));
    terminal_write_str(" (space to toggle, enter to confirm)\n");

    int running = 1;
    int cancelled = 0;

    while (running) {
        /* Draw options */
        for (int i = 0; i < num_options; i++) {
            terminal_write_str("  ");
            if (selected_flags[i]) {
                terminal_write_str("\x1b[32m☑\x1b[0m ");  /* Green checked */
            } else {
                terminal_write_str("☐ ");
            }
            if (i == cursor) {
                terminal_write_str("\x1b[4m");  /* Underline */
            }
            terminal_write(opt_strings[i], opt_lens[i]);
            if (i == cursor) {
                terminal_write_str("\x1b[0m ←");
            }
            terminal_write_str("\n");
        }
        terminal_flush_buffer();

        /* Read key */
        char key_char[8];
        size_t char_len;
        char key_name[32];

        int result = terminal_read_key(-1, key_char, &char_len, key_name, sizeof(key_name));

        if (result > 0) {
            if (strcmp(key_name, "up") == 0) {
                cursor = (cursor - 1 + num_options) % num_options;
            } else if (strcmp(key_name, "down") == 0) {
                cursor = (cursor + 1) % num_options;
            } else if (strcmp(key_name, "char") == 0 && key_char[0] == ' ') {
                selected_flags[cursor] = !selected_flags[cursor];
            } else if (strcmp(key_name, "enter") == 0) {
                running = 0;
            } else if (strcmp(key_name, "esc") == 0 || strcmp(key_name, "ctrl+c") == 0) {
                running = 0;
                cancelled = 1;
            }
        }

        /* Move cursor back up to redraw */
        if (running) {
            terminal_cursor_up(num_options);
        }
    }

    terminal_cursor_show(1);

    if (cancelled) {
        /* Cleanup */
        for (int i = 0; i < num_options; i++) {
            efree(opt_strings[i]);
        }
        efree(opt_strings);
        efree(opt_lens);
        efree(selected_flags);
        RETURN_NULL();
    }

    /* Build result array */
    array_init(return_value);
    for (int i = 0; i < num_options; i++) {
        if (selected_flags[i]) {
            add_next_index_stringl(return_value, opt_strings[i], opt_lens[i]);
        }
    }

    /* Cleanup */
    for (int i = 0; i < num_options; i++) {
        efree(opt_strings[i]);
    }
    efree(opt_strings);
    efree(opt_lens);
    efree(selected_flags);
}
/* }}} */

/* {{{ proto ProgressBar Terminal::progressBar(int $total, string $label = '')
   Create a progress bar */
PHP_METHOD(Terminal, progressBar)
{
    zend_long total;
    zend_string *label = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(total)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR_OR_NULL(label)
    ZEND_PARSE_PARAMETERS_END();

    object_init_ex(return_value, terminal_progress_bar_ce);
    progress_bar_t *bar = progress_bar_from_zval(return_value);

    bar->total = (int)total;
    if (label && ZSTR_LEN(label) > 0) {
        bar->label = estrndup(ZSTR_VAL(label), ZSTR_LEN(label));
        bar->label_len = ZSTR_LEN(label);
    }

    /* Initial render */
    progress_bar_render(bar);
}
/* }}} */

/* {{{ proto void ProgressBar::advance(int $step = 1)
   Advance the progress bar */
PHP_METHOD(ProgressBar, advance)
{
    zend_long step = 1;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(step)
    ZEND_PARSE_PARAMETERS_END();

    progress_bar_t *bar = progress_bar_from_zval(ZEND_THIS);

    if (bar->finished) return;

    bar->current += (int)step;
    if (bar->current > bar->total) {
        bar->current = bar->total;
    }

    progress_bar_render(bar);
}
/* }}} */

/* {{{ proto void ProgressBar::set(int $current)
   Set progress bar position */
PHP_METHOD(ProgressBar, set)
{
    zend_long current;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(current)
    ZEND_PARSE_PARAMETERS_END();

    progress_bar_t *bar = progress_bar_from_zval(ZEND_THIS);

    if (bar->finished) return;

    bar->current = (int)current;
    if (bar->current < 0) bar->current = 0;
    if (bar->current > bar->total) bar->current = bar->total;

    progress_bar_render(bar);
}
/* }}} */

/* {{{ proto void ProgressBar::finish(?string $message = null)
   Finish the progress bar */
PHP_METHOD(ProgressBar, finish)
{
    zend_string *message = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR_OR_NULL(message)
    ZEND_PARSE_PARAMETERS_END();

    progress_bar_t *bar = progress_bar_from_zval(ZEND_THIS);

    if (bar->finished) return;

    bar->finished = 1;
    bar->current = bar->total;

    terminal_write_str("\r\x1b[K");

    if (message && ZSTR_LEN(message) > 0) {
        terminal_write_str("\x1b[32m✓\x1b[0m ");  /* Green checkmark */
        terminal_write(ZSTR_VAL(message), ZSTR_LEN(message));
    } else if (bar->label && bar->label_len > 0) {
        terminal_write_str("\x1b[32m✓\x1b[0m ");
        terminal_write(bar->label, bar->label_len);
        terminal_write_str(" - Done!");
    } else {
        terminal_write_str("\x1b[32m✓\x1b[0m Done!");
    }
    terminal_write_str("\n");
    terminal_flush_buffer();
}
/* }}} */

/* {{{ proto Loader Terminal::loader(string $message = '', string $style = 'dots')
   Create a spinner/loader */
PHP_METHOD(Terminal, loader)
{
    zend_string *message = NULL;
    zend_string *style = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR_OR_NULL(message)
        Z_PARAM_STR_OR_NULL(style)
    ZEND_PARSE_PARAMETERS_END();

    object_init_ex(return_value, terminal_loader_ce);
    loader_t *loader = loader_from_zval(return_value);

    if (message && ZSTR_LEN(message) > 0) {
        loader->message = estrndup(ZSTR_VAL(message), ZSTR_LEN(message));
        loader->message_len = ZSTR_LEN(message);
    }

    if (style) {
        loader->style = estrndup(ZSTR_VAL(style), ZSTR_LEN(style));
    } else {
        loader->style = "dots";
    }
}
/* }}} */

/* {{{ proto void Loader::start()
   Start the loader animation */
PHP_METHOD(Loader, start)
{
    ZEND_PARSE_PARAMETERS_NONE();

    loader_t *loader = loader_from_zval(ZEND_THIS);

    if (loader->running) return;

    loader->running = 1;
    loader->frame = 0;
    terminal_cursor_show(0);
    clock_gettime(CLOCK_MONOTONIC, &loader->last_frame);
    loader_render_frame(loader);
}
/* }}} */

/* {{{ proto void Loader::text(string $message)
   Update loader message */
PHP_METHOD(Loader, text)
{
    zend_string *message;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(message)
    ZEND_PARSE_PARAMETERS_END();

    loader_t *loader = loader_from_zval(ZEND_THIS);

    if (loader->message) {
        efree(loader->message);
    }
    loader->message = estrndup(ZSTR_VAL(message), ZSTR_LEN(message));
    loader->message_len = ZSTR_LEN(message);

    if (loader->running) {
        loader_render_frame(loader);
    }
}
/* }}} */

/* {{{ proto void Loader::tick()
   Advance the spinner by one frame - call this in your loop */
PHP_METHOD(Loader, tick)
{
    ZEND_PARSE_PARAMETERS_NONE();

    loader_t *loader = loader_from_zval(ZEND_THIS);

    if (!loader->running) return;

    /* Check if enough time has passed */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    int64_t elapsed_us = (now.tv_sec - loader->last_frame.tv_sec) * 1000000 +
                         (now.tv_nsec - loader->last_frame.tv_nsec) / 1000;

    if (elapsed_us >= SPINNER_FRAME_TIME) {
        loader_render_frame(loader);
        loader->last_frame = now;
    }
}
/* }}} */

/* {{{ proto void Loader::stop(?string $message = null)
   Stop the loader */
PHP_METHOD(Loader, stop)
{
    zend_string *message = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR_OR_NULL(message)
    ZEND_PARSE_PARAMETERS_END();

    loader_t *loader = loader_from_zval(ZEND_THIS);

    if (!loader->running) return;

    loader->running = 0;

    terminal_write_str("\r\x1b[K");

    if (message && ZSTR_LEN(message) > 0) {
        terminal_write_str("\x1b[32m✓\x1b[0m ");
        terminal_write(ZSTR_VAL(message), ZSTR_LEN(message));
        terminal_write_str("\n");
    }

    terminal_cursor_show(1);
    terminal_flush_buffer();
}
/* }}} */

/* ============================================================================
 * METHOD REGISTRATION
 * ============================================================================ */

ZEND_BEGIN_ARG_INFO_EX(arginfo_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_terminal_size, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_terminal_supports, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_terminal_bool, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, enable, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_terminal_cursor_to, 0, 0, 2)
    ZEND_ARG_TYPE_INFO(0, col, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, row, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_terminal_cursor_move, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, n, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_terminal_cursor_position, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_terminal_on_resize, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_terminal_style, 0, 2, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, text, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, styles, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_terminal_table, 0, 0, 2)
    ZEND_ARG_TYPE_INFO(0, headers, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, rows, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, options, IS_ARRAY, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_terminal_read_key, 0, 0, IS_ARRAY, 1)
    ZEND_ARG_TYPE_INFO(0, timeout, IS_DOUBLE, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_terminal_select, 0, 2, IS_STRING, 1)
    ZEND_ARG_TYPE_INFO(0, prompt, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, options, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, default, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_terminal_multi_select, 0, 2, IS_ARRAY, 1)
    ZEND_ARG_TYPE_INFO(0, prompt, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, options, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, defaults, IS_ARRAY, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_terminal_progress_bar, 0, 1, Signalforge\\Terminal\\ProgressBar, 0)
    ZEND_ARG_TYPE_INFO(0, total, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, label, IS_STRING, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_terminal_loader, 0, 0, Signalforge\\Terminal\\Loader, 0)
    ZEND_ARG_TYPE_INFO(0, message, IS_STRING, 1)
    ZEND_ARG_TYPE_INFO(0, style, IS_STRING, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_progress_advance, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, step, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_progress_set, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, current, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_progress_finish, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, message, IS_STRING, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_loader_text, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, message, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_loader_stop, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, message, IS_STRING, 1)
ZEND_END_ARG_INFO()

static const zend_function_entry terminal_methods[] = {
    PHP_ME(Terminal, enter, arginfo_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, exit, arginfo_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, size, arginfo_terminal_size, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, supportsColor, arginfo_terminal_supports, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, supports256Color, arginfo_terminal_supports, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, supportsTrueColor, arginfo_terminal_supports, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, clear, arginfo_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, clearLine, arginfo_void, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, alternateScreen, arginfo_terminal_bool, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, cursor, arginfo_terminal_bool, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, cursorTo, arginfo_terminal_cursor_to, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, cursorUp, arginfo_terminal_cursor_move, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, cursorDown, arginfo_terminal_cursor_move, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, cursorForward, arginfo_terminal_cursor_move, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, cursorBack, arginfo_terminal_cursor_move, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, cursorPosition, arginfo_terminal_cursor_position, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, onResize, arginfo_terminal_on_resize, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, style, arginfo_terminal_style, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, table, arginfo_terminal_table, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, readKey, arginfo_terminal_read_key, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, select, arginfo_terminal_select, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, multiSelect, arginfo_terminal_multi_select, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, progressBar, arginfo_terminal_progress_bar, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Terminal, loader, arginfo_terminal_loader, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

static const zend_function_entry progress_bar_methods[] = {
    PHP_ME(ProgressBar, advance, arginfo_progress_advance, ZEND_ACC_PUBLIC)
    PHP_ME(ProgressBar, set, arginfo_progress_set, ZEND_ACC_PUBLIC)
    PHP_ME(ProgressBar, finish, arginfo_progress_finish, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry loader_methods[] = {
    PHP_ME(Loader, start, arginfo_void, ZEND_ACC_PUBLIC)
    PHP_ME(Loader, text, arginfo_loader_text, ZEND_ACC_PUBLIC)
    PHP_ME(Loader, tick, arginfo_void, ZEND_ACC_PUBLIC)
    PHP_ME(Loader, stop, arginfo_loader_stop, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* ============================================================================
 * MODULE INITIALIZATION
 * ============================================================================ */

static void php_terminal_globals_ctor(zend_terminal_globals *globals)
{
    globals->state = NULL;
}

PHP_MINIT_FUNCTION(terminal)
{
    zend_class_entry ce;

    /* Initialize globals */
    ZEND_INIT_MODULE_GLOBALS(terminal, php_terminal_globals_ctor, NULL);

    /* Register Terminal class */
    INIT_NS_CLASS_ENTRY(ce, "Signalforge\\Terminal", "Terminal", terminal_methods);
    zend_register_internal_class(&ce);

    /* Register TerminalException */
    INIT_NS_CLASS_ENTRY(ce, "Signalforge\\Terminal", "TerminalException", NULL);
    terminal_exception_ce = zend_register_internal_class_ex(&ce, zend_ce_exception);
    TERMINAL_G(exception_ce) = terminal_exception_ce;

    /* Register ProgressBar class */
    INIT_NS_CLASS_ENTRY(ce, "Signalforge\\Terminal", "ProgressBar", progress_bar_methods);
    terminal_progress_bar_ce = zend_register_internal_class(&ce);
    terminal_progress_bar_ce->create_object = progress_bar_create;
    terminal_progress_bar_ce->ce_flags |= ZEND_ACC_FINAL;
    memcpy(&progress_bar_handlers, &std_object_handlers, sizeof(zend_object_handlers));
    progress_bar_handlers.offset = XtOffsetOf(progress_bar_t, std);
    progress_bar_handlers.free_obj = progress_bar_free;
    TERMINAL_G(progress_bar_ce) = terminal_progress_bar_ce;

    /* Register Loader class */
    INIT_NS_CLASS_ENTRY(ce, "Signalforge\\Terminal", "Loader", loader_methods);
    terminal_loader_ce = zend_register_internal_class(&ce);
    terminal_loader_ce->create_object = loader_create;
    terminal_loader_ce->ce_flags |= ZEND_ACC_FINAL;
    memcpy(&loader_handlers, &std_object_handlers, sizeof(zend_object_handlers));
    loader_handlers.offset = XtOffsetOf(loader_t, std);
    loader_handlers.free_obj = loader_free;
    TERMINAL_G(loader_ce) = terminal_loader_ce;

    /* Initialize terminal state */
    terminal_init_state();

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(terminal)
{
    /* Ensure terminal is restored */
    terminal_exit_raw();
    terminal_free_state();

    return SUCCESS;
}

PHP_RINIT_FUNCTION(terminal)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(terminal)
{
    /* Restore terminal on request end */
    terminal_exit_raw();

    return SUCCESS;
}

PHP_MINFO_FUNCTION(terminal)
{
    terminal_state_t *state = TERM_STATE();

    php_info_print_table_start();
    php_info_print_table_header(2, "Terminal Extension", "enabled");
    php_info_print_table_row(2, "Version", PHP_TERMINAL_VERSION);
    php_info_print_table_row(2, "Author", "Signalforge");

    if (state) {
        char size_str[32];
        snprintf(size_str, sizeof(size_str), "%d x %d", state->cols, state->rows);
        php_info_print_table_row(2, "Terminal Size", size_str);

        const char *color_str;
        switch (state->color_support) {
            case COLOR_TRUECOLOR: color_str = "True Color (24-bit)"; break;
            case COLOR_256: color_str = "256 Colors"; break;
            case COLOR_16: color_str = "16 Colors"; break;
            default: color_str = "None"; break;
        }
        php_info_print_table_row(2, "Color Support", color_str);
    }

    php_info_print_table_end();
}

zend_module_entry terminal_module_entry = {
    STANDARD_MODULE_HEADER,
    "terminal",
    NULL, /* No global functions */
    PHP_MINIT(terminal),
    PHP_MSHUTDOWN(terminal),
    PHP_RINIT(terminal),
    PHP_RSHUTDOWN(terminal),
    PHP_MINFO(terminal),
    PHP_TERMINAL_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_TERMINAL
ZEND_GET_MODULE(terminal)
#endif
