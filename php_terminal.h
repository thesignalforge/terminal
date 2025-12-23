/*
 * php_terminal.h - PHP Terminal Extension Header
 *
 * High-performance terminal control for PHP CLI applications.
 * Provides raw mode, styling, tables, progress bars, and interactive input.
 *
 * Copyright (c) 2024 Signalforge
 */

#ifndef PHP_TERMINAL_H
#define PHP_TERMINAL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

/* Extension version */
#define PHP_TERMINAL_VERSION "1.0.0"

/* Module entry */
extern zend_module_entry terminal_module_entry;
#define phpext_terminal_ptr &terminal_module_entry

/* Terminal state flags */
#define TERM_STATE_NONE         0x00
#define TERM_STATE_RAW          0x01
#define TERM_STATE_ALT_SCREEN   0x02
#define TERM_STATE_CURSOR_HIDDEN 0x04

/* Color support levels */
#define COLOR_NONE      0
#define COLOR_16        1
#define COLOR_256       2
#define COLOR_TRUECOLOR 3

/* Border styles for tables */
#define BORDER_NONE     0
#define BORDER_ASCII    1
#define BORDER_SINGLE   2
#define BORDER_DOUBLE   3
#define BORDER_ROUNDED  4

/* Key types */
#define KEY_CHAR        0
#define KEY_SPECIAL     1
#define KEY_CTRL        2
#define KEY_FUNCTION    3

/* Maximum input buffer size */
#define INPUT_BUFFER_SIZE 32

/* Write buffer size for batched output */
#define WRITE_BUFFER_SIZE 8192

/* Spinner frame time in microseconds */
#define SPINNER_FRAME_TIME 60000

/* Box drawing characters - single line */
#define BOX_HORIZONTAL      "─"
#define BOX_VERTICAL        "│"
#define BOX_TOP_LEFT        "┌"
#define BOX_TOP_RIGHT       "┐"
#define BOX_BOTTOM_LEFT     "└"
#define BOX_BOTTOM_RIGHT    "┘"
#define BOX_LEFT_T          "├"
#define BOX_RIGHT_T         "┤"
#define BOX_TOP_T           "┬"
#define BOX_BOTTOM_T        "┴"
#define BOX_CROSS           "┼"

/* Box drawing characters - double line */
#define BOX_HORIZONTAL_D    "═"
#define BOX_VERTICAL_D      "║"
#define BOX_TOP_LEFT_D      "╔"
#define BOX_TOP_RIGHT_D     "╗"
#define BOX_BOTTOM_LEFT_D   "╚"
#define BOX_BOTTOM_RIGHT_D  "╝"
#define BOX_LEFT_T_D        "╠"
#define BOX_RIGHT_T_D       "╣"
#define BOX_TOP_T_D         "╦"
#define BOX_BOTTOM_T_D      "╩"
#define BOX_CROSS_D         "╬"

/* Box drawing characters - rounded */
#define BOX_TOP_LEFT_R      "╭"
#define BOX_TOP_RIGHT_R     "╮"
#define BOX_BOTTOM_LEFT_R   "╰"
#define BOX_BOTTOM_RIGHT_R  "╯"

/* ASCII fallback */
#define BOX_HORIZONTAL_A    "-"
#define BOX_VERTICAL_A      "|"
#define BOX_CORNER_A        "+"

/* Spinner styles */
#define SPINNER_DOTS        "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏"
#define SPINNER_LINE        "-\\|/"
#define SPINNER_ARROW       "←↖↑↗→↘↓↙"

/* Terminal state structure */
typedef struct {
    struct termios original_termios;    /* Original terminal settings */
    struct termios raw_termios;         /* Raw mode settings */
    int state_flags;                    /* Current state flags */
    int color_support;                  /* Color support level */
    int cols;                           /* Terminal columns */
    int rows;                           /* Terminal rows */
    int tty_fd;                         /* TTY file descriptor */
    zval resize_callback;               /* PHP callback for resize events */
    char write_buffer[WRITE_BUFFER_SIZE]; /* Batched write buffer */
    size_t write_buffer_pos;            /* Current position in write buffer */
    volatile sig_atomic_t resize_pending; /* Flag for pending resize */
} terminal_state_t;

/* Progress bar object structure */
typedef struct {
    zend_object std;
    int total;
    int current;
    char *label;
    size_t label_len;
    struct timespec start_time;
    int finished;
    int last_rendered_width;
} progress_bar_t;

/* Loader/spinner object structure */
typedef struct {
    zend_object std;
    char *message;
    size_t message_len;
    const char *style;          /* Pointer to spinner pattern */
    int frame;                  /* Current frame index */
    int running;
    int frame_count;
    struct timespec last_frame;
} loader_t;

/* Global state */
ZEND_BEGIN_MODULE_GLOBALS(terminal)
    terminal_state_t *state;
    zend_class_entry *exception_ce;
    zend_class_entry *progress_bar_ce;
    zend_class_entry *loader_ce;
ZEND_END_MODULE_GLOBALS(terminal)

ZEND_EXTERN_MODULE_GLOBALS(terminal)

#define TERMINAL_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(terminal, v)

/* Exception class entry */
extern zend_class_entry *terminal_exception_ce;
extern zend_class_entry *terminal_progress_bar_ce;
extern zend_class_entry *terminal_loader_ce;

/* Object handlers */
extern zend_object_handlers progress_bar_handlers;
extern zend_object_handlers loader_handlers;

/* Utility macros */
#define TERM_STATE() (TERMINAL_G(state))
#define TERM_IS_RAW() (TERM_STATE() && (TERM_STATE()->state_flags & TERM_STATE_RAW))

/* Throw terminal exception */
#define TERM_THROW(msg) do { \
    zend_throw_exception(terminal_exception_ce, msg, 0); \
    return; \
} while(0)

#define TERM_THROW_RET(msg, ret) do { \
    zend_throw_exception(terminal_exception_ce, msg, 0); \
    return ret; \
} while(0)

/* Function declarations - Terminal core */
void terminal_init_state(void);
void terminal_free_state(void);
int terminal_enter_raw(void);
int terminal_exit_raw(void);
void terminal_restore_on_shutdown(void);
void terminal_update_size(void);
int terminal_detect_color_support(void);

/* Function declarations - Output */
void terminal_flush_buffer(void);
void terminal_write(const char *str, size_t len);
void terminal_write_str(const char *str);
void terminal_printf(const char *fmt, ...);

/* Function declarations - Cursor and screen */
void terminal_clear_screen(void);
void terminal_clear_line(void);
void terminal_cursor_to(int col, int row);
void terminal_cursor_up(int n);
void terminal_cursor_down(int n);
void terminal_cursor_forward(int n);
void terminal_cursor_back(int n);
void terminal_cursor_show(int visible);
void terminal_alternate_screen(int enable);
int terminal_get_cursor_position(int *col, int *row);

/* Function declarations - Styling */
void terminal_apply_style(const char *text, size_t len, zval *styles, zend_string **result);
int terminal_parse_color(zval *color, int is_bg, char *buf, size_t buf_size);
void terminal_strip_ansi(const char *text, size_t len, char **result, size_t *result_len);

/* Function declarations - UTF-8 */
int terminal_utf8_char_width(const unsigned char *str, size_t len, size_t *bytes);
size_t terminal_display_width(const char *str, size_t len);
size_t terminal_utf8_strlen(const char *str, size_t len);

/* Function declarations - Table */
void terminal_render_table(zval *headers, zval *rows, zval *options);

/* Function declarations - Input */
int terminal_read_key(double timeout, char *key_char, size_t *char_len, char *key_name, size_t name_size);

/* Function declarations - Signal handling */
void terminal_setup_signal_handlers(void);
void terminal_restore_signal_handlers(void);
void terminal_handle_sigwinch(int sig);
void terminal_handle_sigint(int sig);
void terminal_handle_sigtstp(int sig);
void terminal_handle_sigcont(int sig);

/* Helper inline functions */
static inline int64_t timespec_to_ms(struct timespec *ts) {
    return (int64_t)ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
}

static inline void get_monotonic_time(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

#endif /* PHP_TERMINAL_H */
