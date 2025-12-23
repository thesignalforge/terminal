--TEST--
Terminal extension - Command abstract class
--SKIPIF--
<?php
if (!extension_loaded('terminal')) die('skip terminal extension not loaded');
?>
--FILE--
<?php
use Signalforge\Terminal\Command;

// Test that Command class exists and is abstract
var_dump(class_exists(Command::class));

$reflection = new ReflectionClass(Command::class);
var_dump($reflection->isAbstract());

// Create a concrete implementation
class TestCommand extends Command
{
    public function configure(): void
    {
        $this->setName('test')
             ->setDescription('A test command')
             ->addArgument('name', 'The name argument', true)
             ->addOption('flag', 'f', 'A flag option', false);
    }

    public function execute(): int
    {
        $name = $this->getArgument('name');
        $flag = $this->getOption('flag');

        echo "name=" . $name . "\n";
        echo "flag=" . ($flag ? "true" : "false") . "\n";

        return 0;
    }
}

$cmd = new TestCommand();

// Test with arguments
$result = $cmd->run(['test', 'John']);
echo "result=" . $result . "\n";

// Test with flag
$result = $cmd->run(['test', 'Jane', '--flag']);
echo "result=" . $result . "\n";

echo "\nDone\n";
?>
--EXPECT--
bool(true)
bool(true)
name=John
flag=false
result=0
name=Jane
flag=true
result=0

Done
