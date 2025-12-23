--TEST--
Terminal extension - ProgressBar class
--SKIPIF--
<?php
if (!extension_loaded('terminal')) die('skip terminal extension not loaded');
?>
--FILE--
<?php
use Signalforge\Terminal\Terminal;
use Signalforge\Terminal\ProgressBar;

// Create progress bar
$bar = Terminal::progressBar(100, 'Test');
var_dump($bar instanceof ProgressBar);

// Test advance - just verify it doesn't throw
$bar->advance(10);

// Test set
$bar->set(50);

// Test finish
$bar->finish("Complete");

// Create another bar without label
$bar2 = Terminal::progressBar(50);
var_dump($bar2 instanceof ProgressBar);
$bar2->finish();

echo "\nDone\n";
?>
--EXPECTF--
%abool(true)
%aComplete
%abool(true)
%aDone
