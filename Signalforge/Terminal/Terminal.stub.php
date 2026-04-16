<?php
/**
 * Signalforge Terminal Extension
 * Terminal.stub.php - IDE stub for the Terminal class
 *
 * Terminal is a static facade providing raw-mode terminal control,
 * cursor movement, color/style detection, simple table rendering,
 * single-key input, and select / multi-select pickers.
 *
 * All methods are static — there is nothing to instantiate.
 *
 * @package Signalforge\Terminal
 */

declare(strict_types=1);

namespace Signalforge\Terminal;

/**
 * Static façade for terminal control.
 *
 * Methods that mutate terminal state (raw mode, cursor visibility,
 * alternate screen) require the controlling fd to be a TTY and will
 * throw {@see TerminalException} otherwise.
 *
 * @example
 * Terminal::enter();
 * try {
 *     Terminal::clear();
 *     Terminal::cursorTo(0, 0);
 *     echo Terminal::style('Ready', ['color' => Color::GREEN, 'bold' => true]);
 *     $key = Terminal::readKey(5.0);
 * } finally {
 *     Terminal::exit();
 * }
 */
final class Terminal
{
    /**
     * Switch the controlling terminal into raw mode.
     *
     * Disables canonical mode and echo so individual keystrokes can be read
     * via {@see readKey()}, {@see select()}, and {@see multiSelect()}.
     * Pair with {@see exit()} (RSHUTDOWN also restores the terminal).
     *
     * @throws TerminalException If stdin is not a TTY
     */
    public static function enter(): void {}

    /**
     * Restore the terminal to its previous (cooked) settings.
     *
     * @throws TerminalException If termios cannot be restored
     */
    public static function exit(): void {}

    /**
     * Get the current terminal dimensions.
     *
     * Falls back to 80x24 if the size cannot be detected. If a resize was
     * pending, refreshes the cached size and invokes any registered
     * {@see onResize()} callback first.
     *
     * @return array{cols: int, rows: int}
     */
    public static function size(): array {}

    /**
     * Whether the terminal supports at least 16 ANSI colors.
     *
     * @return bool
     */
    public static function supportsColor(): bool {}

    /**
     * Whether the terminal supports the 256-color palette.
     *
     * @return bool
     */
    public static function supports256Color(): bool {}

    /**
     * Whether the terminal supports 24-bit (true color) escape sequences.
     *
     * @return bool
     */
    public static function supportsTrueColor(): bool {}

    /**
     * Clear the entire screen and home the cursor.
     *
     * @return void
     */
    public static function clear(): void {}

    /**
     * Clear the current line.
     *
     * @return void
     */
    public static function clearLine(): void {}

    /**
     * Enter or leave the alternate screen buffer.
     *
     * @param bool $enable True to switch to the alternate buffer, false to restore
     * @return void
     */
    public static function alternateScreen(bool $enable): void {}

    /**
     * Show or hide the cursor.
     *
     * @param bool $visible
     * @return void
     */
    public static function cursor(bool $visible): void {}

    /**
     * Move the cursor to an absolute position (0-indexed).
     *
     * @param int $col Column
     * @param int $row Row
     * @return void
     */
    public static function cursorTo(int $col, int $row): void {}

    /**
     * Move the cursor up by `$n` rows.
     *
     * @param int $n Defaults to 1
     * @return void
     */
    public static function cursorUp(int $n = 1): void {}

    /**
     * Move the cursor down by `$n` rows.
     *
     * @param int $n Defaults to 1
     * @return void
     */
    public static function cursorDown(int $n = 1): void {}

    /**
     * Move the cursor forward (right) by `$n` columns.
     *
     * @param int $n Defaults to 1
     * @return void
     */
    public static function cursorForward(int $n = 1): void {}

    /**
     * Move the cursor back (left) by `$n` columns.
     *
     * @param int $n Defaults to 1
     * @return void
     */
    public static function cursorBack(int $n = 1): void {}

    /**
     * Query the current cursor position.
     *
     * @return array{col: int, row: int}
     * @throws TerminalException If the position cannot be read (e.g. not a TTY)
     */
    public static function cursorPosition(): array {}

    /**
     * Register a callback fired whenever the terminal is resized.
     *
     * The callback is dispatched the next time {@see size()} is called
     * after a SIGWINCH is received.
     *
     * @param callable $callback Receives no arguments
     * @return void
     * @throws TerminalException If the terminal is not initialized or callback is not callable
     */
    public static function onResize(callable $callback): void {}

    /**
     * Wrap text in ANSI escape codes derived from a styles map.
     *
     * Style array keys are extension-defined (color, bg, bold, italic,
     * underline, etc.). Values typically come from {@see Color} constants
     * or are booleans for attributes.
     *
     * @param string $text Text to style
     * @param array<string, mixed> $styles Style options
     * @return string Styled text
     */
    public static function style(string $text, array $styles): string {}

    /**
     * Render a simple ASCII/Unicode table.
     *
     * @param array<int, string> $headers Header cells
     * @param array<int, array<int, string>> $rows Row data (each row is an array of cells)
     * @param array<string, mixed>|null $options Reserved for table styling options
     * @return void
     */
    public static function table(array $headers, array $rows, ?array $options = null): void {}

    /**
     * Read a single keypress (UTF-8 character or named key).
     *
     * Returns an array describing the key, or null on timeout. The shape
     * is `['char' => '<utf8>', 'key' => '<name>']`; `char` is omitted for
     * pure named keys (arrows, esc, enter, etc.).
     *
     * @param float|null $timeout Seconds to wait, null for blocking read
     * @return array{char?: string, key: string}|null
     * @throws TerminalException If the read fails
     */
    public static function readKey(?float $timeout = null): ?array {}

    /**
     * Interactive single-select picker (requires raw mode).
     *
     * Renders the prompt followed by the option list with arrow-key
     * navigation. Returns the chosen option as a string, or null if the
     * user cancelled with Esc / Ctrl+C.
     *
     * @param string $prompt Prompt text shown above the choices
     * @param array<int, string> $options Option labels in display order
     * @param int $default Index of the initially highlighted option
     * @return string|null Selected option, or null on cancel
     * @throws TerminalException If the terminal is not in raw mode
     */
    public static function select(string $prompt, array $options, int $default = 0): ?string {}

    /**
     * Interactive multi-select picker (requires raw mode).
     *
     * Space toggles the current row, Enter confirms, Esc / Ctrl+C cancels.
     * Returns the list of selected option labels, or null on cancel.
     *
     * @param string $prompt Prompt text shown above the choices
     * @param array<int, string> $options Option labels in display order
     * @param array<int, int>|null $defaults Indices to pre-check
     * @return array<int, string>|null Selected labels, or null on cancel
     * @throws TerminalException If the terminal is not in raw mode
     */
    public static function multiSelect(string $prompt, array $options, ?array $defaults = null): ?array {}

    /**
     * Create and immediately render a {@see ProgressBar}.
     *
     * @param int $total Maximum value (100% mark)
     * @param string|null $label Optional label shown next to the bar
     * @return ProgressBar
     */
    public static function progressBar(int $total, ?string $label = null): ProgressBar {}

    /**
     * Create a spinner / loader.
     *
     * The loader is not started automatically — call {@see Loader::start()}
     * and drive it from your event loop with {@see Loader::tick()}.
     *
     * @param string|null $message Initial message
     * @param string|null $style Spinner style identifier (default "dots")
     * @return Loader
     */
    public static function loader(?string $message = null, ?string $style = null): Loader {}
}
