--TEST--
Terminal extension - Color support detection
--SKIPIF--
<?php
if (!extension_loaded('terminal')) die('skip terminal extension not loaded');
?>
--FILE--
<?php
use Signalforge\Terminal\Terminal;

// These should return booleans
var_dump(is_bool(Terminal::supportsColor()));
var_dump(is_bool(Terminal::supports256Color()));
var_dump(is_bool(Terminal::supportsTrueColor()));

// If true color is supported, 256 should also be supported
$trueColor = Terminal::supportsTrueColor();
$color256 = Terminal::supports256Color();
$color16 = Terminal::supportsColor();

if ($trueColor) {
    var_dump($color256 === true);
    var_dump($color16 === true);
} else {
    var_dump(true); // Skip
    var_dump(true); // Skip
}

if ($color256) {
    var_dump($color16 === true);
} else {
    var_dump(true); // Skip
}

echo "Done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Done
