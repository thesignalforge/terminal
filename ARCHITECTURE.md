# Terminal Extension Architecture

This document describes the internal architecture and design decisions of the PHP Terminal extension.

## Overview

The extension is structured as a low-level terminal control library that provides primitives for building CLI applications. It prioritizes:

1. **Performance** - Minimal overhead, batched output, optimized rendering
2. **Safety** - Terminal always restored, even on crashes
3. **Correctness** - Proper UTF-8 handling, signal management
4. **Simplicity** - Direct POSIX calls, no ncurses dependency

## File Structure

```
ext/terminal/
├── config.m4           # Autoconf build configuration
├── php_terminal.h      # Header with structures, macros, declarations
├── terminal.c          # Main implementation
├── tests/              # PHPT test files
├── examples/           # Demo scripts
├── README.md           # User documentation
└── ARCHITECTURE.md     # This file
```

## Core Components

### Terminal State (`terminal_state_t`)

The extension maintains a global state structure allocated on module init:

```c
typedef struct {
    struct termios original_termios;    // Saved original settings
    struct termios raw_termios;         // Raw mode settings
    int state_flags;                    // Current state (raw, alt screen, etc.)
    int color_support;                  // Detected color level
    int cols, rows;                     // Cached terminal size
    int tty_fd;                         // TTY file descriptor
    zval resize_callback;               // PHP callback for SIGWINCH
    char write_buffer[8192];            // Output buffer
    size_t write_buffer_pos;            // Buffer position
    volatile sig_atomic_t resize_pending; // Async-safe resize flag
} terminal_state_t;
```

### Memory Strategy

- **Persistent memory** (`pecalloc`): Terminal state struct, survives requests
- **Request memory** (`emalloc`): Strings, arrays, temporary buffers
- **Zend strings** (`zend_string_init`): All PHP-visible strings

### Object Lifecycle

```
MINIT (module init)
  └─> Allocate terminal state
  └─> Register classes
  └─> Detect color support

RINIT (request init)
  └─> Nothing special

RSHUTDOWN (request shutdown)
  └─> Exit raw mode if active

MSHUTDOWN (module shutdown)
  └─> Exit raw mode
  └─> Free terminal state
```

## Terminal Control

### Raw Mode

Raw mode is implemented via `termios`:

```c
// Disable:
// - BRKINT: Break generates interrupt
// - ICRNL: CR->NL translation
// - INPCK: Parity checking
// - ISTRIP: Strip 8th bit
// - IXON: XON/XOFF flow control
// - OPOST: Output processing
// - ECHO: Echo input
// - ICANON: Canonical mode (line buffering)
// - IEXTEN: Extended functions
// - ISIG: Signal generation (we handle signals ourselves)

// Enable:
// - CS8: 8-bit characters

// Control:
// - VMIN=0, VTIME=0: Non-blocking read
```

### Signal Handling

Signals are handled in C with async-signal-safe code:

| Signal | Action |
|--------|--------|
| SIGWINCH | Set `resize_pending` flag, update size on next API call |
| SIGINT | Restore terminal, re-raise with default handler |
| SIGTERM | Same as SIGINT |
| SIGTSTP | Restore terminal, re-raise (suspend process) |
| SIGCONT | Re-apply raw mode, reinstall SIGTSTP handler |

The SIGWINCH handler only sets a flag (async-signal-safe). Size update and PHP callback execution happen synchronously in `Terminal::size()`.

### Shutdown Safety

Multiple layers ensure terminal restoration:

1. `RSHUTDOWN` - Called on request end
2. `MSHUTDOWN` - Called on module unload
3. Signal handlers - Restore before termination
4. PHP shutdown function - Registered on `Terminal::enter()`

## Output Buffering

All output goes through a write buffer to minimize syscalls:

```c
void terminal_write(const char *str, size_t len) {
    while (len > 0) {
        size_t available = BUFFER_SIZE - pos;
        size_t to_copy = min(len, available);

        memcpy(buffer + pos, str, to_copy);
        pos += to_copy;
        str += to_copy;
        len -= to_copy;

        if (pos >= BUFFER_SIZE) {
            flush(); // write() syscall
        }
    }
}
```

Flush is called:
- Before reading input
- After completing table/progress rendering
- On `Terminal::exit()`
- When buffer is full

## UTF-8 Width Calculation

Display width calculation accounts for:

1. **ASCII** (0x00-0x7F): Width 1 (except control chars = 0)
2. **Combining marks**: Width 0 (e.g., diacritics)
3. **Zero-width chars**: Width 0 (ZWNJ, ZWJ, etc.)
4. **CJK ideographs**: Width 2
5. **Emoji**: Width 2 (most)
6. **Other**: Width 1

Implementation uses a lookup table approach:

```c
int terminal_utf8_char_width(const unsigned char *str, size_t len, size_t *bytes) {
    // Decode UTF-8 to codepoint
    uint32_t cp = decode_utf8(str, len, bytes);

    // Width lookup
    return unicode_char_width(cp);
}

size_t terminal_display_width(const char *str, size_t len) {
    size_t width = 0;
    size_t pos = 0;

    while (pos < len) {
        size_t bytes;
        width += terminal_utf8_char_width(str + pos, len - pos, &bytes);
        pos += bytes;
    }

    return width;
}
```

## Color System

### Detection

Color support is detected from environment:

```c
int terminal_detect_color_support() {
    const char *colorterm = getenv("COLORTERM");
    const char *term = getenv("TERM");

    if (colorterm && (strcmp(colorterm, "truecolor") == 0 ||
                      strcmp(colorterm, "24bit") == 0)) {
        return COLOR_TRUECOLOR;
    }

    if (term && strstr(term, "256color")) {
        return COLOR_256;
    }

    if (term && (strstr(term, "color") || strstr(term, "xterm"))) {
        return COLOR_16;
    }

    return isatty(STDOUT_FILENO) ? COLOR_16 : COLOR_NONE;
}
```

### Fallback

RGB colors degrade gracefully:

```
True Color → 24-bit ANSI (38;2;R;G;B)
256 Color  → Closest palette entry (38;5;N)
16 Color   → Closest basic color (30-37, 90-97)
No Color   → No ANSI codes
```

## Table Rendering

Tables use a two-pass algorithm:

### Pass 1: Calculate Column Widths

```c
for each column:
    width[col] = max(
        display_width(header[col]),
        max(display_width(row[i][col]) for all rows)
    )

if total_width > terminal_width and truncate:
    while excess > 0:
        find widest column
        reduce by 1
        excess--
```

### Pass 2: Render

```
┌─────┬─────┬─────┐   <- top border
│ HDR │ HDR │ HDR │   <- headers with padding
├─────┼─────┼─────┤   <- header separator
│ val │ val │ val │   <- data rows
│ val │ val │ val │
└─────┴─────┴─────┘   <- bottom border
```

Box-drawing characters are selected based on border style:

- `single`: Unicode single-line (─│┌┐└┘├┤┬┴┼)
- `double`: Unicode double-line (═║╔╗╚╝╠╣╦╩╬)
- `rounded`: Single with rounded corners (╭╮╰╯)
- `ascii`: ASCII only (-|+)
- `none`: No borders

## Input Handling

### Key Reading

```c
int terminal_read_key(double timeout, ...) {
    fd_set fds;
    FD_SET(STDIN_FILENO, &fds);

    if (select(..., timeout) > 0) {
        read(STDIN_FILENO, buf, sizeof(buf));
        return parse_key(buf);
    }

    return 0; // timeout
}
```

### Escape Sequence Parsing

```
ESC → standalone escape
ESC [ A → up arrow
ESC [ B → down arrow
ESC [ C → right arrow
ESC [ D → left arrow
ESC [ 1 ~ → home
ESC [ 3 ~ → delete
ESC O P → F1
...etc
```

## Progress Bar

Progress bars track:
- Total and current values
- Start time (for rate/ETA calculation)
- Label

Rendering:

```
[====>     ] 42% (42/100) 5.2/s ETA: 00:12
 |     |      |     |        |      |
 |     |      |     |        |      └─ Estimated time remaining
 |     |      |     |        └─ Items per second
 |     |      |     └─ Current/total
 |     |      └─ Percentage
 |     └─ Empty space
 └─ Filled portion (with arrow head)
```

## Loader/Spinner

Spinners use frame-based animation:

```c
const char *spinner_dots_frames[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏", NULL
};

void loader_render_frame(loader_t *loader) {
    // Check time elapsed
    if (elapsed_us >= 60000) { // 60ms per frame
        write frame[frame_index % frame_count]
        frame_index++
        last_frame_time = now
    }
}
```

The `tick()` method is called from PHP's event loop, not a separate thread, keeping the design simple and avoiding ZTS issues.

## PHP Classes

### Terminal (Static)

All methods are static - there's only one terminal:

```c
ZEND_ACC_PUBLIC | ZEND_ACC_STATIC
```

### ProgressBar / Loader (Objects)

These use custom object handlers:

```c
zend_object *progress_bar_create(zend_class_entry *ce) {
    progress_bar_t *bar = zend_object_alloc(sizeof(progress_bar_t), ce);
    // Initialize...
    bar->std.handlers = &progress_bar_handlers;
    return &bar->std;
}

void progress_bar_free(zend_object *object) {
    progress_bar_t *bar = get_bar_from_object(object);
    if (bar->label) efree(bar->label);
    zend_object_std_dtor(&bar->std);
}
```

Object memory layout uses `XtOffsetOf` for the standard object at the end:

```c
typedef struct {
    int total;
    int current;
    // ...other fields...
    zend_object std;  // Must be last
} progress_bar_t;

#define get_bar(zv) ((progress_bar_t *)((char *)Z_OBJ_P(zv) - XtOffsetOf(progress_bar_t, std)))
```

## Error Handling

Errors throw `TerminalException`:

```c
#define TERM_THROW(msg) do { \
    zend_throw_exception(terminal_exception_ce, msg, 0); \
    return; \
} while(0)
```

Exception class extends `Exception`:

```c
INIT_NS_CLASS_ENTRY(ce, "Signalforge\\Terminal", "TerminalException", NULL);
terminal_exception_ce = zend_register_internal_class_ex(&ce, zend_ce_exception);
```

## Performance Considerations

1. **Batched writes**: Output buffer reduces syscalls
2. **Cached size**: Terminal size cached, updated on SIGWINCH
3. **Pre-computed widths**: Table column widths calculated once
4. **Minimal ANSI**: Only emit escape codes that change state
5. **Direct termios**: No ncurses overhead

## Future Improvements

Potential enhancements (not implemented):

1. **Double buffering**: Diff current vs previous screen, only update changes
2. **Mouse support**: Parse mouse escape sequences
3. **Clipboard**: OSC 52 for clipboard access
4. **Bracketed paste**: Detect paste vs typing
5. **Alternate character sets**: For line-drawing on legacy terminals

## Testing

Tests use PHP's PHPT format:

```
--TEST--
Description
--SKIPIF--
<?php if (!extension_loaded('terminal')) die('skip'); ?>
--FILE--
<?php /* test code */ ?>
--EXPECT--
expected output
```

Run tests:

```bash
make test
```

## Debugging

Useful techniques:

1. **stderr**: Write debug output to stderr (bypasses terminal handling)
2. **strace**: Trace syscalls to see termios changes
3. **Valgrind**: Memory leak detection

```bash
# Check for memory leaks
USE_ZEND_ALLOC=0 valgrind --leak-check=full php examples/demo.php
```
