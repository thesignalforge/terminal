<?php
/**
 * Signalforge Terminal Extension
 * Color.stub.php - IDE stub for the Color class constants
 *
 * @package Signalforge\Terminal
 */

declare(strict_types=1);

namespace Signalforge\Terminal;

/**
 * String-named color constants used by {@see Terminal::style()}.
 *
 * The values are short identifiers (e.g. "red", "bright_blue") that the
 * extension translates into ANSI escape sequences appropriate for the
 * detected terminal capability.
 *
 * @example
 * echo Terminal::style('Warning', ['color' => Color::YELLOW]);
 * echo Terminal::style('Error', ['color' => Color::BRIGHT_RED, 'bold' => true]);
 */
final class Color
{
    public const BLACK = 'black';
    public const RED = 'red';
    public const GREEN = 'green';
    public const YELLOW = 'yellow';
    public const BLUE = 'blue';
    public const MAGENTA = 'magenta';
    public const CYAN = 'cyan';
    public const WHITE = 'white';

    public const BRIGHT_BLACK = 'bright_black';
    public const BRIGHT_RED = 'bright_red';
    public const BRIGHT_GREEN = 'bright_green';
    public const BRIGHT_YELLOW = 'bright_yellow';
    public const BRIGHT_BLUE = 'bright_blue';
    public const BRIGHT_MAGENTA = 'bright_magenta';
    public const BRIGHT_CYAN = 'bright_cyan';
    public const BRIGHT_WHITE = 'bright_white';

    public const DEFAULT_COLOR = 'default';
}
