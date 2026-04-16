<?php
/**
 * Signalforge Terminal Extension
 * TerminalException.stub.php - IDE stub for the TerminalException class
 *
 * @package Signalforge\Terminal
 */

declare(strict_types=1);

namespace Signalforge\Terminal;

/**
 * Thrown by {@see Terminal} when terminal manipulation fails.
 *
 * Common causes: stdin/stdout is not a TTY, raw mode could not be entered
 * or restored, the cursor position cannot be queried, callback is not
 * callable, or an interactive picker was invoked outside raw mode.
 */
class TerminalException extends \Exception
{
}
