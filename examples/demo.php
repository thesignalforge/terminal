#!/usr/bin/env php
<?php
/**
 * Terminal Extension Demo
 *
 * This script demonstrates all features of the terminal extension.
 * Run: php demo.php
 */

use Signalforge\Terminal\Terminal;
use Signalforge\Terminal\TerminalException;

// Ensure extension is loaded
if (!extension_loaded('terminal')) {
    echo "Error: terminal extension not loaded\n";
    echo "Build and install with:\n";
    echo "  phpize && ./configure --enable-terminal && make && sudo make install\n";
    exit(1);
}

echo "=== Terminal Extension Demo ===\n\n";

// Show terminal info before entering raw mode
echo "Terminal Information:\n";
$size = Terminal::size();
echo "  Size: {$size['cols']} x {$size['rows']}\n";
echo "  Color support: " . (Terminal::supportsColor() ? "Yes" : "No") . "\n";
echo "  256 colors: " . (Terminal::supports256Color() ? "Yes" : "No") . "\n";
echo "  True color: " . (Terminal::supportsTrueColor() ? "Yes" : "No") . "\n\n";

// Styling demo (works without raw mode)
echo "=== Styling Demo ===\n";
echo Terminal::style("Bold text", ['bold' => true]) . "\n";
echo Terminal::style("Italic text", ['italic' => true]) . "\n";
echo Terminal::style("Underlined text", ['underline' => true]) . "\n";
echo Terminal::style("Red text", ['fg' => 'red']) . "\n";
echo Terminal::style("Green on black", ['fg' => 'green', 'bg' => 'black']) . "\n";
echo Terminal::style("Cyan bold", ['fg' => 'cyan', 'bold' => true]) . "\n";

if (Terminal::supportsTrueColor()) {
    echo Terminal::style("True color (orange)", ['fg' => '#ff8800']) . "\n";
    echo Terminal::style("RGB color", ['fg' => [100, 150, 255]]) . "\n";
}
echo "\n";

// Table demo
echo "=== Table Demo ===\n\n";

// Simple table
Terminal::table(
    ['Name', 'Role', 'Status'],
    [
        ['Alice Johnson', 'Developer', 'Active'],
        ['Bob Smith', 'Designer', 'Away'],
        ['Charlie Brown', 'Manager', 'Active'],
    ],
    [
        'headerStyle' => ['bold' => true, 'fg' => 'cyan'],
    ]
);
echo "\n";

// Table with alignment
Terminal::table(
    ['ID', 'Product', 'Price', 'Qty'],
    [
        ['001', 'Widget', '$19.99', '50'],
        ['002', 'Gadget', '$49.99', '25'],
        ['003', 'Gizmo', '$99.99', '10'],
    ],
    [
        'border' => 'rounded',
        'align' => ['left', 'left', 'right', 'right'],
        'headerStyle' => ['bold' => true],
    ]
);
echo "\n";

// ASCII border for compatibility
Terminal::table(
    ['A', 'B', 'C'],
    [['1', '2', '3'], ['4', '5', '6']],
    ['border' => 'ascii']
);
echo "\n";

// Progress bar demo
echo "=== Progress Bar Demo ===\n";

$bar = Terminal::progressBar(50, 'Processing');
for ($i = 0; $i < 50; $i++) {
    $bar->advance();
    usleep(30000); // 30ms
}
$bar->finish("Processing complete!");
echo "\n";

// Another progress bar
$bar = Terminal::progressBar(100, 'Downloading');
for ($i = 0; $i <= 100; $i += 5) {
    $bar->set($i);
    usleep(50000);
}
$bar->finish();
echo "\n";

// Loader demo
echo "=== Loader Demo ===\n";

$loader = Terminal::loader('Loading data...', 'dots');
$loader->start();
for ($i = 0; $i < 30; $i++) {
    $loader->tick();
    usleep(60000);
}
$loader->stop("Data loaded!");

$loader = Terminal::loader('Syncing...', 'arrow');
$loader->start();
for ($i = 0; $i < 20; $i++) {
    $loader->tick();
    usleep(80000);
}
$loader->stop("Sync complete!");

echo "\n";

// Interactive features require raw mode
echo "=== Interactive Demo ===\n";
echo "The following features require raw terminal mode.\n";
echo "Press Enter to continue, or Ctrl+C to skip...\n";

// Wait for Enter
$line = fgets(STDIN);

if ($line !== false) {
    try {
        Terminal::enter();

        // Clear screen and show demo
        Terminal::alternateScreen(true);
        Terminal::clear();
        Terminal::cursorTo(0, 0);

        echo Terminal::style("=== Interactive Terminal Demo ===\n\n", ['bold' => true, 'fg' => 'cyan']);

        // Select demo
        echo "Single Select:\n";
        $choice = Terminal::select(
            "Choose your environment:",
            ["Development", "Staging", "Production"],
            0
        );
        echo "\nYou selected: " . ($choice ?? "(cancelled)") . "\n\n";

        // Multi-select demo
        echo "Multi Select:\n";
        $features = Terminal::multiSelect(
            "Enable features:",
            ["Caching", "Logging", "Metrics", "Tracing"],
            [0, 1] // Default: Caching and Logging enabled
        );
        if ($features) {
            echo "\nEnabled: " . implode(", ", $features) . "\n\n";
        } else {
            echo "\n(cancelled)\n\n";
        }

        // Key reading demo
        echo "Press any key (or 'q' to quit)...\n";
        while (true) {
            $key = Terminal::readKey(5.0); // 5 second timeout
            if ($key === null) {
                echo "Timeout!\n";
                break;
            }
            echo "Key: " . $key['key'];
            if (isset($key['char'])) {
                echo " (char: " . $key['char'] . ")";
            }
            echo "\n";
            if (isset($key['char']) && $key['char'] === 'q') {
                break;
            }
        }

        echo "\nPress any key to exit demo...\n";
        Terminal::readKey();

        // Restore screen
        Terminal::alternateScreen(false);
        Terminal::exit();

    } catch (TerminalException $e) {
        Terminal::exit();
        echo "Terminal error: " . $e->getMessage() . "\n";
    }
}

echo "\n=== Demo Complete ===\n";
echo "Thanks for trying the Terminal extension!\n";
