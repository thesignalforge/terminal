--TEST--
Terminal extension - Style functionality
--SKIPIF--
<?php
if (!extension_loaded('terminal')) die('skip terminal extension not loaded');
?>
--FILE--
<?php
use Signalforge\Terminal\Terminal;

// Test style with bold
$styled = Terminal::style("Hello", ['bold' => true]);
var_dump(strpos($styled, "\x1b[") !== false); // Contains ANSI
var_dump(strpos($styled, "Hello") !== false);  // Contains text
var_dump(strpos($styled, "\x1b[0m") !== false); // Ends with reset

// Test style with color
$styled = Terminal::style("World", ['fg' => 'red']);
var_dump(strpos($styled, "\x1b[31m") !== false); // Red foreground

// Test style with background
$styled = Terminal::style("Test", ['bg' => 'blue']);
var_dump(strpos($styled, "\x1b[44m") !== false); // Blue background

// Test combined styles
$styled = Terminal::style("Combined", ['bold' => true, 'fg' => 'green', 'underline' => true]);
var_dump(strpos($styled, "1") !== false);  // Bold
var_dump(strpos($styled, "32") !== false); // Green
var_dump(strpos($styled, "4") !== false);  // Underline

// Test empty styles returns original
$styled = Terminal::style("Plain", []);
var_dump($styled === "Plain");

echo "Done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Done
