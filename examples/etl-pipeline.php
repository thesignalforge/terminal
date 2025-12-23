#!/usr/bin/env php
<?php
/**
 * ETL Pipeline Command Example
 *
 * Demonstrates a data pipeline that extracts, transforms, and loads data
 * with progress tracking and detailed logging.
 *
 * Usage:
 *   ./etl-pipeline.php input.csv output.json
 *   ./etl-pipeline.php input.csv output.json --transform=uppercase --dry-run
 *   ./etl-pipeline.php input.csv output.json -v --batch-size=100
 */

use Signalforge\Terminal\Command;
use Signalforge\Terminal\Terminal;
use Signalforge\Terminal\Color;

class EtlPipelineCommand extends Command
{
    private array $stats = [
        'extracted' => 0,
        'transformed' => 0,
        'loaded' => 0,
        'errors' => 0,
    ];

    public function configure(): void
    {
        $this->setName('etl')
             ->setDescription('Extract, Transform, and Load data from CSV to JSON')
             ->addArgument('input', 'Input CSV file path', true)
             ->addArgument('output', 'Output JSON file path', true)
             ->addOption('transform', 't', 'Transformation to apply (uppercase, lowercase, trim)', true, 'trim')
             ->addOption('batch-size', 'b', 'Number of records per batch', true, '50')
             ->addOption('dry-run', null, 'Simulate without writing output')
             ->addOption('verbose', 'v', 'Show detailed progress');
    }

    public function execute(): int
    {
        $input = $this->getArgument('input');
        $output = $this->getArgument('output');
        $transform = $this->getOption('transform');
        $batchSize = (int) $this->getOption('batch-size');
        $dryRun = $this->getOption('dry-run');
        $verbose = $this->getOption('verbose');

        // Validate input file
        if (!file_exists($input)) {
            $this->error("Input file not found: {$input}");
            return 1;
        }

        $this->info("ETL Pipeline Started");
        $this->newLine();

        if ($dryRun) {
            $this->warning("DRY RUN MODE - No data will be written");
            $this->newLine();
        }

        // Phase 1: Extract
        $this->info(Terminal::style(" EXTRACT ", ['bg' => Color::BLUE, 'fg' => Color::WHITE, 'bold' => true]));
        $records = $this->extract($input, $verbose);

        if ($records === null) {
            return 1;
        }

        $this->newLine();

        // Phase 2: Transform
        $this->info(Terminal::style(" TRANSFORM ", ['bg' => Color::MAGENTA, 'fg' => Color::WHITE, 'bold' => true]));
        $transformed = $this->transform($records, $transform, $batchSize, $verbose);
        $this->newLine();

        // Phase 3: Load
        $this->info(Terminal::style(" LOAD ", ['bg' => Color::GREEN, 'fg' => Color::WHITE, 'bold' => true]));

        if (!$dryRun) {
            $this->load($transformed, $output, $verbose);
        } else {
            $this->comment("  Skipping write (dry-run mode)");
            $this->stats['loaded'] = count($transformed);
        }

        $this->newLine();

        // Summary
        $this->printSummary($dryRun);

        return $this->stats['errors'] > 0 ? 1 : 0;
    }

    private function extract(string $file, bool $verbose): ?array
    {
        $handle = fopen($file, 'r');
        if (!$handle) {
            $this->error("Failed to open file: {$file}");
            return null;
        }

        $records = [];
        $headers = null;
        $lineNum = 0;

        // Count lines for progress
        $totalLines = count(file($file)) - 1; // Exclude header
        $bar = Terminal::progressBar($totalLines, 'Reading');

        while (($row = fgetcsv($handle)) !== false) {
            $lineNum++;

            if ($headers === null) {
                $headers = $row;
                continue;
            }

            if (count($row) !== count($headers)) {
                $this->stats['errors']++;
                if ($verbose) {
                    $this->warning("  Line {$lineNum}: Column count mismatch, skipping");
                }
                $bar->advance();
                continue;
            }

            $records[] = array_combine($headers, $row);
            $this->stats['extracted']++;
            $bar->advance();
        }

        fclose($handle);
        $bar->finish("Extracted {$this->stats['extracted']} records");

        return $records;
    }

    private function transform(array $records, string $type, int $batchSize, bool $verbose): array
    {
        $transformed = [];
        $total = count($records);
        $bar = Terminal::progressBar($total, 'Transforming');

        $transformFn = match($type) {
            'uppercase' => fn($v) => is_string($v) ? strtoupper($v) : $v,
            'lowercase' => fn($v) => is_string($v) ? strtolower($v) : $v,
            'trim' => fn($v) => is_string($v) ? trim($v) : $v,
            default => fn($v) => $v,
        };

        foreach (array_chunk($records, $batchSize) as $batchNum => $batch) {
            foreach ($batch as $record) {
                $newRecord = [];
                foreach ($record as $key => $value) {
                    $newRecord[$key] = $transformFn($value);
                }
                $transformed[] = $newRecord;
                $this->stats['transformed']++;
                $bar->advance();
            }

            if ($verbose) {
                $processed = min(($batchNum + 1) * $batchSize, $total);
                // Batch processing is shown in progress bar
            }
        }

        $bar->finish("Transformed {$this->stats['transformed']} records ({$type})");

        return $transformed;
    }

    private function load(array $records, string $file, bool $verbose): void
    {
        $total = count($records);
        $bar = Terminal::progressBar($total, 'Writing');

        $json = "[\n";
        foreach ($records as $i => $record) {
            $json .= "  " . json_encode($record, JSON_UNESCAPED_UNICODE);
            if ($i < $total - 1) {
                $json .= ",";
            }
            $json .= "\n";
            $this->stats['loaded']++;
            $bar->advance();
        }
        $json .= "]\n";

        if (file_put_contents($file, $json) === false) {
            $this->error("Failed to write output file: {$file}");
            $this->stats['errors']++;
            return;
        }

        $size = strlen($json);
        $sizeStr = $size > 1024 ? round($size / 1024, 1) . " KB" : "{$size} bytes";
        $bar->finish("Wrote {$this->stats['loaded']} records ({$sizeStr})");
    }

    private function printSummary(bool $dryRun): void
    {
        $this->info(Terminal::style(" SUMMARY ", ['bg' => Color::CYAN, 'fg' => Color::BLACK, 'bold' => true]));
        $this->newLine();

        Terminal::table(
            ['Phase', 'Records', 'Status'],
            [
                ['Extract', $this->stats['extracted'], $this->formatStatus($this->stats['extracted'] > 0)],
                ['Transform', $this->stats['transformed'], $this->formatStatus($this->stats['transformed'] > 0)],
                ['Load', $this->stats['loaded'], $this->formatStatus($this->stats['loaded'] > 0 || $dryRun)],
                ['Errors', $this->stats['errors'], $this->formatStatus($this->stats['errors'] === 0, true)],
            ],
            ['border' => 'rounded']
        );

        $this->newLine();

        if ($this->stats['errors'] === 0) {
            $this->success("Pipeline completed successfully!");
        } else {
            $this->warning("Pipeline completed with {$this->stats['errors']} error(s)");
        }
    }

    private function formatStatus(bool $ok, bool $invert = false): string
    {
        if ($invert) {
            $ok = !$ok;
        }
        return $ok
            ? Terminal::style('OK', ['fg' => Color::GREEN, 'bold' => true])
            : Terminal::style('WARN', ['fg' => Color::YELLOW, 'bold' => true]);
    }
}

// Run the command
$cmd = new EtlPipelineCommand();
exit($cmd->run());
