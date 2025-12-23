# Terminal Extension for PHP

A high-performance C extension that gives PHP direct control over the terminal. Build beautiful CLI applications with colors, tables, progress bars, spinners, and interactive menus — all without external dependencies.

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│   ████████╗███████╗██████╗ ███╗   ███╗██╗███╗   ██╗ █████╗ ██╗  │
│   ╚══██╔══╝██╔════╝██╔══██╗████╗ ████║██║████╗  ██║██╔══██╗██║  │
│      ██║   █████╗  ██████╔╝██╔████╔██║██║██╔██╗ ██║███████║██║  │
│      ██║   ██╔══╝  ██╔══██╗██║╚██╔╝██║██║██║╚██╗██║██╔══██║██║  │
│      ██║   ███████╗██║  ██║██║ ╚═╝ ██║██║██║ ╚████║██║  ██║███████╗
│      ╚═╝   ╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝╚═╝╚═╝  ╚═══╝╚═╝  ╚═╝╚══════╝
│                                                                 │
│                  PHP Terminal Control Extension                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Why This Extension?

PHP's built-in terminal support is limited. You can `echo` text, but that's about it. Want colors? You're manually writing ANSI escape codes. Want a progress bar? You're pulling in a Composer package. Want interactive menus? Good luck.

This extension changes that. It's written in C for maximum performance and provides:

- **Zero dependencies** — No Composer packages, no external libraries
- **Native performance** — C code, not PHP parsing ANSI sequences
- **Proper terminal handling** — Raw mode, signal handling, cleanup on crash
- **UTF-8 aware** — Correct width calculation for emoji, CJK characters
- **Interactive input** — Arrow key navigation, multi-select menus
- **Beautiful output** — Tables with Unicode borders, progress bars with ETA

---

## Installation

### Requirements

- PHP 8.1 or higher (CLI, non-thread-safe build)
- Linux or macOS
- Standard build tools (gcc, make, phpize)

### Building from Source

```bash
cd ext/terminal

# Prepare the build system
phpize

# Configure
./configure --enable-terminal

# Compile
make

# Run tests (optional but recommended)
make test

# Install system-wide
sudo make install
```

### Enabling the Extension

Add to your PHP CLI configuration:

```bash
# Find your PHP CLI ini directory
php --ini

# Add the extension (adjust path as needed)
echo "extension=terminal.so" | sudo tee /etc/php/8.3/cli/conf.d/20-terminal.ini
```

Verify installation:

```bash
php -m | grep terminal
# Output: terminal

php --ri terminal
# Output:
# terminal
# Terminal Extension => enabled
# Version => 1.0.0
```

---

## Quick Start

```php
<?php
use Signalforge\Terminal\Terminal;

// Colors and styling work immediately
echo Terminal::style("Hello, World!", ['bold' => true, 'fg' => 'green']);
echo "\n";

// Render a table
Terminal::table(
    ['Name', 'Status'],
    [
        ['Web Server', 'Running'],
        ['Database', 'Running'],
        ['Cache', 'Stopped'],
    ]
);

// Show a progress bar
$bar = Terminal::progressBar(100, 'Downloading');
for ($i = 0; $i < 100; $i++) {
    $bar->advance();
    usleep(50000);
}
$bar->finish('Download complete!');
```

---

## Features in Detail

### 1. Text Styling

Add colors, bold, italic, underline, and more to your text.

```php
use Signalforge\Terminal\Terminal;

// Basic styling
echo Terminal::style("Error!", ['fg' => 'red', 'bold' => true]);
echo Terminal::style("Warning", ['fg' => 'yellow']);
echo Terminal::style("Success", ['fg' => 'green']);
echo Terminal::style("Info", ['fg' => 'cyan']);

// Background colors
echo Terminal::style(" PASS ", ['bg' => 'green', 'fg' => 'black', 'bold' => true]);
echo Terminal::style(" FAIL ", ['bg' => 'red', 'fg' => 'white', 'bold' => true]);

// Text decorations
echo Terminal::style("Important", ['underline' => true]);
echo Terminal::style("Subtle", ['dim' => true]);
echo Terminal::style("Fancy", ['italic' => true]);
```

**Output Preview:**

```
┌────────────────────────────────────────────────────────────────┐
│                                                                │
│   Error!   Warning   Success   Info                            │
│   ^^^^^^   ^^^^^^^   ^^^^^^^   ^^^^                            │
│   (red)   (yellow)  (green)   (cyan)                           │
│                                                                │
│   ┌──────┐ ┌──────┐                                            │
│   │ PASS │ │ FAIL │   <- Badges with background colors         │
│   └──────┘ └──────┘                                            │
│                                                                │
│   Important   Subtle   Fancy                                   │
│   _________   (dim)   (italic)                                 │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

#### Available Colors

| Basic Colors | Bright Variants |
|--------------|-----------------|
| `black`      | `bright_black`  |
| `red`        | `bright_red`    |
| `green`      | `bright_green`  |
| `yellow`     | `bright_yellow` |
| `blue`       | `bright_blue`   |
| `magenta`    | `bright_magenta`|
| `cyan`       | `bright_cyan`   |
| `white`      | `bright_white`  |

#### RGB and Hex Colors

If your terminal supports true color (most modern terminals do):

```php
// Hex colors
echo Terminal::style("Orange", ['fg' => '#ff8800']);
echo Terminal::style("Purple", ['fg' => '#9933ff']);

// RGB arrays
echo Terminal::style("Custom", ['fg' => [100, 150, 255]]);

// Gradient effect
$colors = ['#ff0000', '#ff7f00', '#ffff00', '#00ff00', '#0000ff', '#8b00ff'];
foreach ($colors as $i => $color) {
    echo Terminal::style("█", ['fg' => $color]);
}
```

The extension automatically degrades colors based on terminal capabilities:
- True color terminals: Full 24-bit RGB
- 256-color terminals: Closest palette match
- Basic terminals: Closest 16-color match

#### Using the Color Class

Instead of magic strings, use the `Color` class for type-safe color constants:

```php
use Signalforge\Terminal\Color;
use Signalforge\Terminal\Terminal;

// Use Color constants instead of strings
echo Terminal::style("Error!", ['fg' => Color::RED, 'bold' => true]);
echo Terminal::style("Success!", ['fg' => Color::GREEN]);
echo Terminal::style("Warning!", ['fg' => Color::YELLOW]);
echo Terminal::style("Info", ['fg' => Color::CYAN]);

// Bright variants
echo Terminal::style("Highlighted", ['fg' => Color::BRIGHT_WHITE, 'bg' => Color::BLUE]);

// All available colors
echo Terminal::style("Text", ['fg' => Color::BLACK]);
echo Terminal::style("Text", ['fg' => Color::RED]);
echo Terminal::style("Text", ['fg' => Color::GREEN]);
echo Terminal::style("Text", ['fg' => Color::YELLOW]);
echo Terminal::style("Text", ['fg' => Color::BLUE]);
echo Terminal::style("Text", ['fg' => Color::MAGENTA]);
echo Terminal::style("Text", ['fg' => Color::CYAN]);
echo Terminal::style("Text", ['fg' => Color::WHITE]);
echo Terminal::style("Text", ['fg' => Color::BRIGHT_BLACK]);   // Gray
echo Terminal::style("Text", ['fg' => Color::BRIGHT_RED]);
echo Terminal::style("Text", ['fg' => Color::BRIGHT_GREEN]);
echo Terminal::style("Text", ['fg' => Color::BRIGHT_YELLOW]);
echo Terminal::style("Text", ['fg' => Color::BRIGHT_BLUE]);
echo Terminal::style("Text", ['fg' => Color::BRIGHT_MAGENTA]);
echo Terminal::style("Text", ['fg' => Color::BRIGHT_CYAN]);
echo Terminal::style("Text", ['fg' => Color::BRIGHT_WHITE]);
echo Terminal::style("Text", ['fg' => Color::DEFAULT_COLOR]);  // Reset to default
```

Using the `Color` class provides IDE autocompletion and prevents typos in color names.

---

### 2. Tables

Render beautiful data tables with automatic column sizing and Unicode borders.

```php
Terminal::table(
    ['ID', 'Name', 'Role', 'Status'],
    [
        ['1', 'Alice Johnson', 'Administrator', 'Active'],
        ['2', 'Bob Smith', 'Developer', 'Active'],
        ['3', 'Carol White', 'Designer', 'Away'],
        ['4', 'David Brown', 'Manager', 'Offline'],
    ],
    [
        'border' => 'rounded',
        'headerStyle' => ['bold' => true, 'fg' => 'cyan'],
    ]
);
```

**Output:**

```
╭────┬───────────────┬───────────────┬─────────╮
│ ID │ Name          │ Role          │ Status  │
├────┼───────────────┼───────────────┼─────────┤
│ 1  │ Alice Johnson │ Administrator │ Active  │
│ 2  │ Bob Smith     │ Developer     │ Active  │
│ 3  │ Carol White   │ Designer      │ Away    │
│ 4  │ David Brown   │ Manager       │ Offline │
╰────┴───────────────┴───────────────┴─────────╯
```

#### Border Styles

```
┌─ single ─┐   ╔═ double ═╗   ╭─ rounded ─╮   +- ascii -+   no border
│          │   ║          ║   │           │   |         |
└──────────┘   ╚══════════╝   ╰───────────╯   +---------+
```

#### Column Alignment

```php
Terminal::table(
    ['Product', 'Price', 'Qty', 'Total'],
    [
        ['Widget', '$19.99', '5', '$99.95'],
        ['Gadget', '$49.99', '2', '$99.98'],
        ['Gizmo', '$9.99', '10', '$99.90'],
    ],
    [
        'align' => ['left', 'right', 'center', 'right'],
        'border' => 'single',
    ]
);
```

**Output:**

```
┌─────────┬────────┬─────┬─────────┐
│ Product │  Price │ Qty │   Total │
├─────────┼────────┼─────┼─────────┤
│ Widget  │ $19.99 │  5  │  $99.95 │
│ Gadget  │ $49.99 │  2  │  $99.98 │
│ Gizmo   │  $9.99 │ 10  │  $99.90 │
└─────────┴────────┴─────┴─────────┘
```

#### Styled Cell Content

You can mix styled text inside table cells:

```php
$rows = [
    ['nginx', Terminal::style('● Running', ['fg' => 'green']), '2.4 MB'],
    ['mysql', Terminal::style('● Running', ['fg' => 'green']), '156 MB'],
    ['redis', Terminal::style('○ Stopped', ['fg' => 'red']), '0 MB'],
];

Terminal::table(['Service', 'Status', 'Memory'], $rows);
```

**Output:**

```
┌─────────┬───────────┬────────┐
│ Service │ Status    │ Memory │
├─────────┼───────────┼────────┤
│ nginx   │ ● Running │ 2.4 MB │  <- green
│ mysql   │ ● Running │ 156 MB │  <- green
│ redis   │ ○ Stopped │ 0 MB   │  <- red
└─────────┴───────────┴────────┘
```

---

### 3. Progress Bars

Show progress for long-running operations with automatic rate calculation and ETA.

```php
$bar = Terminal::progressBar(1000, 'Processing files');

foreach ($files as $file) {
    processFile($file);
    $bar->advance();
}

$bar->finish('All files processed!');
```

**Output (animated):**

```
Processing files [===================>          ] 65% (650/1000) 48.2/s ETA: 00:07
```

The progress bar shows:
- **Visual bar** — Fills as progress advances
- **Percentage** — Current completion percentage
- **Count** — Current/total items
- **Rate** — Items per second
- **ETA** — Estimated time remaining

#### Progress Bar Methods

```php
// Create with total count and optional label
$bar = Terminal::progressBar(100, 'Downloading');

// Advance by 1
$bar->advance();

// Advance by multiple
$bar->advance(10);

// Jump to specific position
$bar->set(50);

// Complete with message
$bar->finish('Download complete!');

// Complete with default message
$bar->finish();  // Shows "✓ Downloading - Done!"
```

#### Real-World Example: File Download

```php
function downloadWithProgress(string $url, string $dest): void
{
    $size = getRemoteFileSize($url);
    $bar = Terminal::progressBar($size, 'Downloading');

    $fp = fopen($dest, 'w');
    $ch = curl_init($url);

    curl_setopt($ch, CURLOPT_FILE, $fp);
    curl_setopt($ch, CURLOPT_PROGRESSFUNCTION, function($ch, $dlTotal, $dlNow) use ($bar) {
        $bar->set((int)$dlNow);
    });
    curl_setopt($ch, CURLOPT_NOPROGRESS, false);

    curl_exec($ch);
    curl_close($ch);
    fclose($fp);

    $bar->finish('Download complete!');
}
```

---

### 4. Spinners / Loaders

Show activity for operations with unknown duration.

```php
$loader = Terminal::loader('Connecting to database...', 'dots');
$loader->start();

// Do work, calling tick() periodically
while (!$connected) {
    attemptConnection();
    $loader->tick();
    usleep(100000);
}

$loader->stop('Connected!');
```

**Output (animated):**

```
Frame 1:  ⠋ Connecting to database...
Frame 2:  ⠙ Connecting to database...
Frame 3:  ⠹ Connecting to database...
Frame 4:  ⠸ Connecting to database...
   ...
Final:    ✓ Connected!
```

#### Spinner Styles

```php
// Dots (default) - smooth braille animation
Terminal::loader('Loading...', 'dots');
//  ⠋ ⠙ ⠹ ⠸ ⠼ ⠴ ⠦ ⠧ ⠇ ⠏

// Line - classic ASCII spinner
Terminal::loader('Loading...', 'line');
//  - \ | /

// Arrow - directional animation
Terminal::loader('Loading...', 'arrow');
//  ← ↖ ↑ ↗ → ↘ ↓ ↙
```

#### Updating the Message

```php
$loader = Terminal::loader('Step 1: Initializing...', 'dots');
$loader->start();

initialize();
$loader->text('Step 2: Loading config...');
$loader->tick();

loadConfig();
$loader->text('Step 3: Connecting...');
$loader->tick();

connect();
$loader->stop('All steps complete!');
```

---

### 5. Interactive Menus

Build interactive selection menus with keyboard navigation.

#### Single Select

```php
Terminal::enter();  // Enter raw mode for keyboard input

$choice = Terminal::select(
    'Choose your environment:',
    ['Development', 'Staging', 'Production'],
    0  // Default selection index
);

Terminal::exit();  // Always restore terminal

echo "You selected: $choice\n";
```

**Output:**

```
Choose your environment:
  ○ Development
  ● Staging      ←
  ○ Production

Controls: ↑↓ navigate, Enter confirm, Esc cancel
```

The selected item is highlighted, and the arrow indicator shows the current position.

#### Multi-Select

```php
Terminal::enter();

$features = Terminal::multiSelect(
    'Enable features:',
    ['Caching', 'Logging', 'Metrics', 'Tracing'],
    [0, 1]  // Pre-selected indices
);

Terminal::exit();

echo "Enabled: " . implode(', ', $features) . "\n";
```

**Output:**

```
Enable features: (space to toggle, enter to confirm)
  ☑ Caching
  ☑ Logging       ←
  ☐ Metrics
  ☐ Tracing

Controls: ↑↓ navigate, Space toggle, Enter confirm, Esc cancel
```

#### Practical Example: Deployment Script

```php
#!/usr/bin/env php
<?php
use Signalforge\Terminal\Terminal;

Terminal::enter();

try {
    // Choose environment
    $env = Terminal::select('Deploy to:', [
        'staging',
        'production',
    ]);

    if ($env === 'production') {
        echo Terminal::style("\n⚠ Production deployment!\n", ['fg' => 'yellow', 'bold' => true]);
    }

    // Choose what to deploy
    $components = Terminal::multiSelect('Deploy components:', [
        'Backend API',
        'Frontend App',
        'Database Migrations',
        'Cache Clear',
    ], [0, 1]);

    if (empty($components)) {
        echo "Deployment cancelled.\n";
        exit(1);
    }

    // Confirm
    echo "\nDeploying to $env:\n";
    foreach ($components as $c) {
        echo "  • $c\n";
    }

} finally {
    Terminal::exit();
}
```

---

### 6. Raw Input Handling

Read individual keypresses for custom interactions.

```php
Terminal::enter();

echo "Press any key (q to quit):\n";

while (true) {
    $key = Terminal::readKey();  // Blocks until keypress

    if ($key['key'] === 'char' && $key['char'] === 'q') {
        break;
    }

    echo "Key: {$key['key']}";
    if (isset($key['char'])) {
        echo " (char: {$key['char']})";
    }
    echo "\n";
}

Terminal::exit();
```

#### Key Names

| Key | `$key['key']` value |
|-----|---------------------|
| Arrow Up | `up` |
| Arrow Down | `down` |
| Arrow Left | `left` |
| Arrow Right | `right` |
| Enter | `enter` |
| Tab | `tab` |
| Escape | `esc` |
| Backspace | `backspace` |
| Delete | `delete` |
| Home | `home` |
| End | `end` |
| Page Up | `pageup` |
| Page Down | `pagedown` |
| F1-F12 | `f1`, `f2`, ... `f12` |
| Ctrl+A-Z | `ctrl+a`, `ctrl+b`, ... `ctrl+z` |
| Regular char | `char` (with `$key['char']` containing the character) |

#### Timeout

```php
// Wait up to 5 seconds for input
$key = Terminal::readKey(5.0);

if ($key === null) {
    echo "Timeout!\n";
} else {
    echo "Key pressed: {$key['key']}\n";
}
```

---

### 7. Terminal Control

Low-level terminal manipulation.

#### Screen Control

```php
Terminal::clear();           // Clear entire screen
Terminal::clearLine();       // Clear current line

// Alternate screen buffer (like vim, less, etc.)
Terminal::alternateScreen(true);   // Switch to alternate
// ... your app runs here ...
Terminal::alternateScreen(false);  // Restore original screen
```

#### Cursor Control

```php
Terminal::cursor(false);     // Hide cursor
Terminal::cursor(true);      // Show cursor

Terminal::cursorTo(10, 5);   // Move to column 10, row 5 (0-indexed)
Terminal::cursorUp(3);       // Move up 3 lines
Terminal::cursorDown(2);     // Move down 2 lines
Terminal::cursorForward(5);  // Move right 5 columns
Terminal::cursorBack(3);     // Move left 3 columns

$pos = Terminal::cursorPosition();  // Get current position
echo "Cursor at column {$pos['col']}, row {$pos['row']}\n";
```

#### Terminal Information

```php
$size = Terminal::size();
echo "Terminal is {$size['cols']} columns by {$size['rows']} rows\n";

// Color support detection
if (Terminal::supportsTrueColor()) {
    echo "Full 24-bit color support!\n";
} elseif (Terminal::supports256Color()) {
    echo "256 color palette available\n";
} elseif (Terminal::supportsColor()) {
    echo "Basic 16 colors available\n";
} else {
    echo "No color support\n";
}
```

#### Resize Handling

```php
Terminal::onResize(function() {
    $size = Terminal::size();
    // Redraw your UI for the new size
    redrawInterface($size['cols'], $size['rows']);
});
```

---

### 8. Building CLI Commands

The `Command` abstract class provides a structured way to build command-line applications with argument parsing, option handling, and automatic help generation.

#### Basic Command Structure

```php
#!/usr/bin/env php
<?php
use Signalforge\Terminal\Command;

class GreetCommand extends Command
{
    public function configure(): void
    {
        $this->setName('greet')
             ->setDescription('Greets a person with a friendly message')
             ->addArgument('name', 'The person to greet', required: true)
             ->addOption('yell', 'y', 'Yell the greeting');
    }

    public function execute(): int
    {
        $name = $this->getArgument('name');
        $greeting = "Hello, {$name}!";

        if ($this->getOption('yell')) {
            $greeting = strtoupper($greeting);
        }

        $this->success($greeting);
        return 0;
    }
}

// Run the command
$cmd = new GreetCommand();
exit($cmd->run());
```

Usage:

```
$ ./greet.php World
Hello, World!

$ ./greet.php World --yell
HELLO, WORLD!

$ ./greet.php --help
Description:
  Greets a person with a friendly message

Usage:
  greet [options] <name>

Arguments:
  name                 The person to greet

Options:
  -h, --help           Display this help message
  -y, --yell           Yell the greeting
```

#### Arguments and Options

**Arguments** are positional values passed to your command:

```php
public function configure(): void
{
    // Required argument
    $this->addArgument('filename', 'The file to process', required: true);

    // Optional argument with default
    $this->addArgument('output', 'Output file', required: false, default: 'output.txt');
}

public function execute(): int
{
    $input = $this->getArgument('filename');   // Always set (required)
    $output = $this->getArgument('output');    // 'output.txt' if not provided
    // ...
}
```

**Options** are flags with optional values:

```php
public function configure(): void
{
    // Boolean flag (--verbose or -v)
    $this->addOption('verbose', 'v', 'Enable verbose output');

    // Option that requires a value (--format=json or -f json)
    $this->addOption('format', 'f', 'Output format', requiresValue: true, default: 'text');

    // Long option only (--dry-run)
    $this->addOption('dry-run', null, 'Perform a dry run');
}

public function execute(): int
{
    if ($this->getOption('verbose')) {
        $this->comment('Verbose mode enabled');
    }

    $format = $this->getOption('format');  // 'text', 'json', etc.
    // ...
}
```

#### Output Helpers

Commands have built-in methods for styled output:

```php
public function execute(): int
{
    $this->info('Processing files...');       // Normal text
    $this->success('All files processed!');   // Green text
    $this->warning('Some files skipped');     // Yellow text
    $this->error('Failed to process foo.txt'); // Red text
    $this->comment('This is a comment');      // Dim/gray text
    $this->newLine(2);                        // Add blank lines

    return 0;
}
```

```
Processing files...
All files processed!
Some files skipped
Failed to process foo.txt
This is a comment
```

#### Complete Command Example

Here's a more complete example of a file processing command:

```php
#!/usr/bin/env php
<?php
use Signalforge\Terminal\Command;
use Signalforge\Terminal\Terminal;

class ProcessFilesCommand extends Command
{
    public function configure(): void
    {
        $this->setName('process')
             ->setDescription('Process files in a directory')
             ->addArgument('directory', 'Directory to process', true)
             ->addOption('recursive', 'r', 'Process subdirectories')
             ->addOption('pattern', 'p', 'File pattern to match', true, '*.txt')
             ->addOption('dry-run', null, 'Show what would be done')
             ->addOption('verbose', 'v', 'Show detailed output');
    }

    public function execute(): int
    {
        $dir = $this->getArgument('directory');
        $pattern = $this->getOption('pattern');
        $recursive = $this->getOption('recursive');
        $dryRun = $this->getOption('dry-run');
        $verbose = $this->getOption('verbose');

        if (!is_dir($dir)) {
            $this->error("Directory not found: {$dir}");
            return 1;
        }

        $this->info("Processing {$pattern} in {$dir}");
        if ($dryRun) {
            $this->warning('Dry run mode - no changes will be made');
        }

        $files = glob("{$dir}/{$pattern}");

        // Show progress
        $bar = Terminal::progressBar(count($files), 'Processing');

        foreach ($files as $file) {
            if ($verbose) {
                $this->comment("  Processing: {$file}");
            }

            if (!$dryRun) {
                // Actually process the file
                $this->processFile($file);
            }

            $bar->advance();
        }

        $bar->finish('Done!');
        $this->newLine();
        $this->success(sprintf('Processed %d files', count($files)));

        return 0;
    }

    private function processFile(string $file): void
    {
        // Your file processing logic here
    }
}

$cmd = new ProcessFilesCommand();
exit($cmd->run());
```

Usage:

```bash
# Process .txt files in the current directory
./process.php ./data

# Process recursively with verbose output
./process.php ./data -rv

# Process specific pattern, dry run
./process.php ./data --pattern="*.log" --dry-run

# Show help
./process.php --help
```

---

## Complete Example: System Monitor

Here's a complete example that combines multiple features:

```php
#!/usr/bin/env php
<?php
use Signalforge\Terminal\Terminal;

Terminal::enter();
Terminal::alternateScreen(true);
Terminal::cursor(false);

try {
    while (true) {
        Terminal::clear();
        Terminal::cursorTo(0, 0);

        // Header
        echo Terminal::style("  SYSTEM MONITOR  ", [
            'bg' => 'blue',
            'fg' => 'white',
            'bold' => true
        ]);
        echo "\n\n";

        // System info table
        $load = sys_getloadavg();
        Terminal::table(
            ['Metric', 'Value'],
            [
                ['Hostname', gethostname()],
                ['PHP Version', PHP_VERSION],
                ['Load (1m)', number_format($load[0], 2)],
                ['Load (5m)', number_format($load[1], 2)],
                ['Load (15m)', number_format($load[2], 2)],
                ['Memory', formatBytes(memory_get_usage(true))],
            ],
            ['border' => 'rounded', 'headerStyle' => ['bold' => true, 'fg' => 'cyan']]
        );

        echo "\n";

        // Process list
        $processes = getTopProcesses(5);
        Terminal::table(
            ['PID', 'User', 'CPU%', 'MEM%', 'Command'],
            $processes,
            ['border' => 'single']
        );

        echo "\n" . Terminal::style("Press 'q' to quit, 'r' to refresh", ['dim' => true]);

        // Check for input (non-blocking)
        $key = Terminal::readKey(2.0);  // Refresh every 2 seconds

        if ($key && $key['key'] === 'char' && $key['char'] === 'q') {
            break;
        }
    }
} finally {
    Terminal::cursor(true);
    Terminal::alternateScreen(false);
    Terminal::exit();
}
```

**Output:**

```
  SYSTEM MONITOR

╭─────────────┬──────────────────╮
│ Metric      │ Value            │
├─────────────┼──────────────────┤
│ Hostname    │ web-server-01    │
│ PHP Version │ 8.3.6            │
│ Load (1m)   │ 0.42             │
│ Load (5m)   │ 0.38             │
│ Load (15m)  │ 0.35             │
│ Memory      │ 2.4 MB           │
╰─────────────┴──────────────────╯

┌───────┬──────┬──────┬──────┬─────────────────────┐
│ PID   │ User │ CPU% │ MEM% │ Command             │
├───────┼──────┼──────┼──────┼─────────────────────┤
│ 1234  │ www  │ 12.5 │ 2.1  │ php-fpm             │
│ 5678  │ www  │ 8.3  │ 1.8  │ php-fpm             │
│ 9012  │ root │ 5.2  │ 0.5  │ nginx               │
│ 3456  │ mysql│ 3.1  │ 15.2 │ mysqld              │
│ 7890  │ root │ 1.2  │ 0.1  │ sshd                │
└───────┴──────┴──────┴──────┴─────────────────────┘

Press 'q' to quit, 'r' to refresh
```

---

## Safety & Terminal Restoration

The extension ensures your terminal is **always restored** to its original state, even if:

- Your script throws an exception
- Your script calls `exit()` or `die()`
- Your script receives SIGINT (Ctrl+C)
- Your script receives SIGTERM
- Your script is suspended with SIGTSTP (Ctrl+Z) and resumed
- PHP encounters a fatal error

### Best Practice: Use try/finally

Always wrap raw mode operations in try/finally:

```php
Terminal::enter();

try {
    // Your interactive code here
    $choice = Terminal::select('Choose:', $options);
    // ...
} finally {
    Terminal::exit();  // Always runs, even if exception is thrown
}
```

### What Happens on Suspend (Ctrl+Z)

When the user presses Ctrl+Z:
1. Terminal is restored to normal mode
2. Process is suspended
3. When resumed (with `fg`), raw mode is re-applied
4. Your application continues where it left off

---

## API Reference

### Terminal Class (Static Methods)

| Method | Description |
|--------|-------------|
| `enter()` | Enter raw terminal mode |
| `exit()` | Exit raw mode, restore terminal |
| `size(): array` | Get terminal size `['cols' => int, 'rows' => int]` |
| `supportsColor(): bool` | Check for basic color support |
| `supports256Color(): bool` | Check for 256 color support |
| `supportsTrueColor(): bool` | Check for 24-bit color support |
| `clear()` | Clear the screen |
| `clearLine()` | Clear the current line |
| `alternateScreen(bool)` | Switch to/from alternate screen |
| `cursor(bool)` | Show/hide cursor |
| `cursorTo(int $col, int $row)` | Move cursor to position |
| `cursorUp(int $n = 1)` | Move cursor up |
| `cursorDown(int $n = 1)` | Move cursor down |
| `cursorForward(int $n = 1)` | Move cursor right |
| `cursorBack(int $n = 1)` | Move cursor left |
| `cursorPosition(): array` | Get cursor position |
| `onResize(callable)` | Register resize callback |
| `style(string, array): string` | Apply styles to text |
| `table(array, array, array)` | Render a table |
| `readKey(?float): ?array` | Read a keypress |
| `select(string, array, int): ?string` | Single-select menu |
| `multiSelect(string, array, array): ?array` | Multi-select menu |
| `progressBar(int, string): ProgressBar` | Create progress bar |
| `loader(string, string): Loader` | Create spinner |

### ProgressBar Class

| Method | Description |
|--------|-------------|
| `advance(int $step = 1)` | Increment progress |
| `set(int $current)` | Set absolute position |
| `finish(?string $message)` | Complete the progress bar |

### Loader Class

| Method | Description |
|--------|-------------|
| `start()` | Begin animation |
| `tick()` | Advance frame (call in your loop) |
| `text(string)` | Update message |
| `stop(?string)` | Stop with optional final message |

### Color Class (Constants)

| Constant | Value |
|----------|-------|
| `Color::BLACK` | `"black"` |
| `Color::RED` | `"red"` |
| `Color::GREEN` | `"green"` |
| `Color::YELLOW` | `"yellow"` |
| `Color::BLUE` | `"blue"` |
| `Color::MAGENTA` | `"magenta"` |
| `Color::CYAN` | `"cyan"` |
| `Color::WHITE` | `"white"` |
| `Color::BRIGHT_BLACK` | `"bright_black"` |
| `Color::BRIGHT_RED` | `"bright_red"` |
| `Color::BRIGHT_GREEN` | `"bright_green"` |
| `Color::BRIGHT_YELLOW` | `"bright_yellow"` |
| `Color::BRIGHT_BLUE` | `"bright_blue"` |
| `Color::BRIGHT_MAGENTA` | `"bright_magenta"` |
| `Color::BRIGHT_CYAN` | `"bright_cyan"` |
| `Color::BRIGHT_WHITE` | `"bright_white"` |
| `Color::DEFAULT_COLOR` | `"default"` |

### Command Class (Abstract)

| Method | Description |
|--------|-------------|
| `setName(string): self` | Set command name |
| `setDescription(string): self` | Set command description |
| `addArgument(string, ?string, bool, ?string): self` | Add positional argument |
| `addOption(string, ?string, ?string, bool, ?string): self` | Add option flag |
| `getArgument(string): mixed` | Get argument value |
| `getOption(string): mixed` | Get option value |
| `info(string): void` | Output info message |
| `success(string): void` | Output green success message |
| `error(string): void` | Output red error message |
| `warning(string): void` | Output yellow warning message |
| `comment(string): void` | Output dim comment text |
| `newLine(int): void` | Output blank lines |
| `showHelp(): void` | Display help text |
| `run(?array): int` | Parse args and execute |
| `configure(): void` | **Abstract** - Define arguments/options |
| `execute(): int` | **Abstract** - Run command logic |

---

## Troubleshooting

### "Failed to enter raw mode: terminal may not be a TTY"

This error occurs when stdin is not a terminal (e.g., piped input, cron job, Docker without `-it`).

**Solution:** Check if you're in an interactive terminal:

```php
if (!posix_isatty(STDIN)) {
    echo "This script requires an interactive terminal.\n";
    exit(1);
}
```

### Colors not showing

1. Check terminal support: `echo $TERM`
2. Try setting: `export TERM=xterm-256color`
3. Check color detection: `var_dump(Terminal::supportsColor())`

### Progress bar looks broken

Ensure your terminal supports ANSI escape codes and the `\r` (carriage return) character for line rewriting.

### Interactive menus not working

Make sure you've called `Terminal::enter()` before using `select()`, `multiSelect()`, or `readKey()`.

---

## Platform Support

| Platform | Status |
|----------|--------|
| Linux (Ubuntu, Debian, etc.) | ✅ Full support |
| macOS | ✅ Full support |
| Windows | ❌ Not supported |
| WSL | ✅ Works (it's Linux) |

---

## License

MIT License — See LICENSE file for details.

---

## Credits

Built by Signalforge.

Inspired by:
- [Rich](https://github.com/Textualize/rich) (Python)
- [Bubble Tea](https://github.com/charmbracelet/bubbletea) (Go)
- [crossterm](https://github.com/crossterm-rs/crossterm) (Rust)
