<?php
/**
 * Signalforge Terminal Extension
 * Command.stub.php - IDE stub for the abstract Command class
 *
 * @package Signalforge\Terminal
 */

declare(strict_types=1);

namespace Signalforge\Terminal;

/**
 * Abstract base for CLI commands.
 *
 * Subclasses implement {@see configure()} (declare arguments and options)
 * and {@see execute()} (the body of the command). {@see run()} parses
 * argv, validates required arguments, dispatches `--help`, and finally
 * invokes `execute()`.
 *
 * @example
 * final class GreetCommand extends Command
 * {
 *     protected function configure(): void
 *     {
 *         $this->setName('greet')
 *              ->setDescription('Say hello to someone')
 *              ->addArgument('name', 'Person to greet', true)
 *              ->addOption('shout', 's', 'Uppercase the greeting', false);
 *     }
 *
 *     protected function execute(): int
 *     {
 *         $name = $this->getArgument('name');
 *         $msg = "Hello, $name!";
 *         if ($this->getOption('shout')) {
 *             $msg = strtoupper($msg);
 *         }
 *         $this->success($msg);
 *         return 0;
 *     }
 * }
 *
 * (new GreetCommand())->run($argv);
 */
abstract class Command
{
    /**
     * Initialize internal storage for arguments / options.
     */
    public function __construct() {}

    /**
     * Set the command name used in help output and usage strings.
     *
     * @param string $name
     * @return Command Fluent self
     */
    public function setName(string $name): Command {}

    /**
     * Set the command description shown by `--help`.
     *
     * @param string $description
     * @return Command Fluent self
     */
    public function setDescription(string $description): Command {}

    /**
     * Declare a positional argument.
     *
     * @param string $name Argument name (used as the lookup key)
     * @param string|null $description Help text
     * @param bool $required Whether the argument is mandatory
     * @param string|null $default Default value when omitted
     * @return Command Fluent self
     */
    public function addArgument(
        string $name,
        ?string $description = null,
        bool $required = true,
        ?string $default = null,
    ): Command {}

    /**
     * Declare a `--name[=value]` option.
     *
     * @param string $name Long option name (without leading dashes)
     * @param string|null $shortcut Single-letter short form (without dash)
     * @param string|null $description Help text
     * @param bool $requiresValue If true, the option must be given a value
     * @param string|null $default Default value when option is omitted
     * @return Command Fluent self
     */
    public function addOption(
        string $name,
        ?string $shortcut = null,
        ?string $description = null,
        bool $requiresValue = false,
        ?string $default = null,
    ): Command {}

    /**
     * Read a parsed argument by name.
     *
     * @param string $name
     * @return mixed Stored value or null if absent
     */
    public function getArgument(string $name): mixed {}

    /**
     * Read a parsed option by name.
     *
     * @param string $name
     * @return mixed Stored value or null if absent
     */
    public function getOption(string $name): mixed {}

    /**
     * Print an informational message (no styling).
     *
     * @param string $message
     * @return void
     */
    public function info(string $message): void {}

    /**
     * Print a success message in green.
     *
     * @param string $message
     * @return void
     */
    public function success(string $message): void {}

    /**
     * Print an error message in red.
     *
     * @param string $message
     * @return void
     */
    public function error(string $message): void {}

    /**
     * Print a warning message in yellow.
     *
     * @param string $message
     * @return void
     */
    public function warning(string $message): void {}

    /**
     * Print a dim/comment message.
     *
     * @param string $message
     * @return void
     */
    public function comment(string $message): void {}

    /**
     * Emit `$count` blank lines to stdout.
     *
     * @param int $count Defaults to 1
     * @return void
     */
    public function newLine(int $count = 1): void {}

    /**
     * Print the auto-generated help screen for this command.
     *
     * @return void
     */
    public function showHelp(): void {}

    /**
     * Parse argv, validate required arguments, dispatch help, then call
     * {@see execute()}.
     *
     * Pass `null` to use the script's `$argv` (if available).
     *
     * @param array<int, string>|null $argv
     * @return int The exit code returned by execute() (0 on success)
     */
    public function run(?array $argv = null): int {}

    /**
     * Configure the command — declare name, description, arguments, options.
     *
     * Implementations typically chain calls to {@see setName()},
     * {@see setDescription()}, {@see addArgument()}, {@see addOption()}.
     */
    abstract protected function configure(): void;

    /**
     * Execute the command.
     *
     * @return int Exit code (0 = success)
     */
    abstract protected function execute(): int;
}
