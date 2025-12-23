--TEST--
Terminal extension - Loader class
--SKIPIF--
<?php
if (!extension_loaded('terminal')) die('skip terminal extension not loaded');
?>
--FILE--
<?php
use Signalforge\Terminal\Terminal;
use Signalforge\Terminal\Loader;

// Create loader with default style
$loader = Terminal::loader('Loading...');
var_dump($loader instanceof Loader);

// Create loader with different style
$loader2 = Terminal::loader('Processing', 'line');
var_dump($loader2 instanceof Loader);

// Create loader with arrow style
$loader3 = Terminal::loader('Syncing', 'arrow');
var_dump($loader3 instanceof Loader);

// Test text method (without starting)
$loader->text('New message');
var_dump(true);

echo "Done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
Done
