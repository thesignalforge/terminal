#!/usr/bin/env php
<?php
/**
 * Simple Terminal Extension Example
 *
 * Demonstrates non-interactive features that don't require raw mode.
 */

use Signalforge\Terminal\Terminal;

if (!extension_loaded('terminal')) {
    echo "Error: terminal extension not loaded\n";
    exit(1);
}

// Terminal size
$size = Terminal::size();
echo "Terminal: {$size['cols']}x{$size['rows']}\n\n";

// Styling
echo Terminal::style("Welcome to Terminal Extension", ['bold' => true, 'fg' => 'green']) . "\n\n";

// Color palette demo
$colors = ['black', 'red', 'green', 'yellow', 'blue', 'magenta', 'cyan', 'white'];
echo "Basic colors:\n";
foreach ($colors as $color) {
    echo Terminal::style("  $color  ", ['bg' => $color, 'fg' => $color === 'black' ? 'white' : 'black']);
}
echo "\n\n";

// Bright colors
echo "Bright colors:\n";
foreach ($colors as $color) {
    echo Terminal::style("  $color  ", ['bg' => "bright_$color", 'fg' => 'black']);
}
echo "\n\n";

// Table
Terminal::table(
    ['Feature', 'Status', 'Notes'],
    [
        ['Styling', Terminal::style('OK', ['fg' => 'green']), 'Full ANSI support'],
        ['Tables', Terminal::style('OK', ['fg' => 'green']), 'Unicode borders'],
        ['Progress', Terminal::style('OK', ['fg' => 'green']), 'With ETA'],
        ['Spinner', Terminal::style('OK', ['fg' => 'green']), 'Multiple styles'],
        ['Input', Terminal::style('OK', ['fg' => 'green']), 'Raw key reading'],
        ['Select', Terminal::style('OK', ['fg' => 'green']), 'Single & multi'],
    ],
    [
        'border' => 'rounded',
        'headerStyle' => ['bold' => true, 'fg' => 'cyan'],
    ]
);
echo "\n";

// Progress bar
echo "Processing:\n";
$bar = Terminal::progressBar(20, 'Items');
for ($i = 0; $i < 20; $i++) {
    $bar->advance();
    usleep(50000);
}
$bar->finish("All items processed!");
echo "\n";

// Loader
echo "Loading:\n";
$loader = Terminal::loader('Please wait...', 'dots');
$loader->start();
for ($i = 0; $i < 15; $i++) {
    $loader->tick();
    usleep(80000);
}
$loader->stop("Done!");
echo "\n";
