--TEST--
Terminal extension - Table rendering
--SKIPIF--
<?php
if (!extension_loaded('terminal')) die('skip terminal extension not loaded');
?>
--FILE--
<?php
use Signalforge\Terminal\Terminal;

// Basic table - just verify it runs without error
Terminal::table(
    ['Name', 'Age', 'City'],
    [
        ['Alice', '30', 'New York'],
        ['Bob', '25', 'Los Angeles'],
    ]
);
echo "Table 1 OK\n";

// Table with options
Terminal::table(
    ['ID', 'Value'],
    [
        ['1', 'Test'],
        ['2', 'Data'],
    ],
    [
        'border' => 'double',
        'padding' => 2,
    ]
);
echo "Table 2 OK\n";

// Table with ASCII border
Terminal::table(
    ['A', 'B'],
    [['x', 'y']],
    ['border' => 'ascii']
);
echo "Table 3 OK\n";

// Empty rows table
Terminal::table(['Header'], []);
echo "Table 4 OK\n";

echo "Done\n";
?>
--EXPECTF--
%aTable 1 OK
%aTable 2 OK
%aTable 3 OK
%aTable 4 OK
Done
