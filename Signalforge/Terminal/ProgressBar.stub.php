<?php
/**
 * Signalforge Terminal Extension
 * ProgressBar.stub.php - IDE stub for the ProgressBar class
 *
 * @package Signalforge\Terminal
 */

declare(strict_types=1);

namespace Signalforge\Terminal;

/**
 * In-place progress bar created by {@see Terminal::progressBar()}.
 *
 * Once finished (manually or by reaching `total`), further updates are
 * ignored.
 *
 * @example
 * $bar = Terminal::progressBar(100, 'Processing');
 * foreach ($items as $i => $item) {
 *     // ... do work ...
 *     $bar->advance();
 * }
 * $bar->finish('All items processed');
 */
final class ProgressBar
{
    /**
     * Advance the bar by `$step` units (clamped to total).
     *
     * @param int $step Defaults to 1
     * @return void
     */
    public function advance(int $step = 1): void {}

    /**
     * Set the bar to a specific position (clamped to `[0, total]`).
     *
     * @param int $current
     * @return void
     */
    public function set(int $current): void {}

    /**
     * Mark the bar as complete and clear/redraw the line.
     *
     * If `$message` is provided, it replaces the bar with a green
     * checkmark and that message; otherwise the label (if any) is shown
     * with " - Done!" appended.
     *
     * @param string|null $message
     * @return void
     */
    public function finish(?string $message = null): void {}
}
