--TEST--
Terminal extension - Color class constants
--SKIPIF--
<?php
if (!extension_loaded('terminal')) die('skip terminal extension not loaded');
?>
--FILE--
<?php
use Signalforge\Terminal\Color;
use Signalforge\Terminal\Terminal;

// Test that Color class exists
var_dump(class_exists(Color::class));

// Test color constants
echo "BLACK=" . Color::BLACK . "\n";
echo "RED=" . Color::RED . "\n";
echo "GREEN=" . Color::GREEN . "\n";
echo "YELLOW=" . Color::YELLOW . "\n";
echo "BLUE=" . Color::BLUE . "\n";
echo "MAGENTA=" . Color::MAGENTA . "\n";
echo "CYAN=" . Color::CYAN . "\n";
echo "WHITE=" . Color::WHITE . "\n";
echo "BRIGHT_BLACK=" . Color::BRIGHT_BLACK . "\n";
echo "BRIGHT_WHITE=" . Color::BRIGHT_WHITE . "\n";
echo "DEFAULT_COLOR=" . Color::DEFAULT_COLOR . "\n";

// Test using Color with Terminal::style()
$styled = Terminal::style("Test", ['fg' => Color::RED]);
var_dump(strpos($styled, "\033[31m") !== false);

echo "\nDone\n";
?>
--EXPECT--
bool(true)
BLACK=black
RED=red
GREEN=green
YELLOW=yellow
BLUE=blue
MAGENTA=magenta
CYAN=cyan
WHITE=white
BRIGHT_BLACK=bright_black
BRIGHT_WHITE=bright_white
DEFAULT_COLOR=default
bool(true)

Done
