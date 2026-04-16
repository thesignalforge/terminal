<?php
/**
 * Signalforge Terminal Extension
 * Loader.stub.php - IDE stub for the Loader class
 *
 * @package Signalforge\Terminal
 */

declare(strict_types=1);

namespace Signalforge\Terminal;

/**
 * Cooperative spinner / loader created by {@see Terminal::loader()}.
 *
 * The loader does not run on a thread — you call {@see tick()} from your
 * own event loop and the C extension throttles redraw to roughly the
 * spinner frame interval.
 *
 * @example
 * $loader = Terminal::loader('Working…');
 * $loader->start();
 * while (!$done) {
 *     // do a slice of work…
 *     $loader->tick();
 * }
 * $loader->stop('Finished');
 */
final class Loader
{
    /**
     * Start the spinner.
     *
     * Hides the cursor and draws the first frame. Subsequent calls while
     * already running are no-ops.
     *
     * @return void
     */
    public function start(): void {}

    /**
     * Update the loader's message text.
     *
     * If the loader is currently running the change is rendered immediately,
     * otherwise the new message will be shown next time {@see start()} is
     * called.
     *
     * @param string $message
     * @return void
     */
    public function text(string $message): void {}

    /**
     * Advance to the next frame if enough time has elapsed.
     *
     * Safe to call frequently — the extension internally limits redraws to
     * the spinner's frame interval.
     *
     * @return void
     */
    public function tick(): void {}

    /**
     * Stop the spinner and (optionally) print a final success line.
     *
     * If `$message` is provided, it is printed prefixed with a green
     * checkmark on the cleared line. The cursor is restored.
     *
     * @param string|null $message
     * @return void
     */
    public function stop(?string $message = null): void {}
}
