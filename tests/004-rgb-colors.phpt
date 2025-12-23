--TEST--
Terminal extension - RGB and Hex color support
--SKIPIF--
<?php
if (!extension_loaded('terminal')) die('skip terminal extension not loaded');
?>
--FILE--
<?php
use Signalforge\Terminal\Terminal;

// Test hex color
$styled = Terminal::style("Hex", ['fg' => '#ff0000']);
var_dump(strpos($styled, "\x1b[") !== false);
var_dump(strpos($styled, "Hex") !== false);

// Test RGB array
$styled = Terminal::style("RGB", ['fg' => [0, 255, 0]]);
var_dump(strpos($styled, "\x1b[") !== false);
var_dump(strpos($styled, "RGB") !== false);

// Test short hex
$styled = Terminal::style("Short", ['fg' => '#f00']);
var_dump(strpos($styled, "\x1b[") !== false);
var_dump(strpos($styled, "Short") !== false);

// Test background hex
$styled = Terminal::style("BgHex", ['bg' => '#0000ff']);
var_dump(strpos($styled, "\x1b[") !== false);
var_dump(strpos($styled, "BgHex") !== false);

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
Done
