--TEST--
Terminal extension - Basic functionality
--SKIPIF--
<?php
if (!extension_loaded('terminal')) die('skip terminal extension not loaded');
?>
--FILE--
<?php
use Signalforge\Terminal\Terminal;

// Test that extension is loaded
var_dump(extension_loaded('terminal'));

// Test class exists
var_dump(class_exists('Signalforge\Terminal\Terminal'));
var_dump(class_exists('Signalforge\Terminal\TerminalException'));
var_dump(class_exists('Signalforge\Terminal\ProgressBar'));
var_dump(class_exists('Signalforge\Terminal\Loader'));

// Test size returns array with correct keys
$size = Terminal::size();
var_dump(is_array($size));
var_dump(isset($size['cols']) && isset($size['rows']));
var_dump($size['cols'] > 0);
var_dump($size['rows'] > 0);

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
