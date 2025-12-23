#!/usr/bin/env php
<?php
/**
 * Database Export Command Example
 *
 * Demonstrates loading data from a database in chunks with
 * progress tracking and memory-efficient processing.
 *
 * This example uses SQLite for simplicity, but the pattern
 * works with any PDO-compatible database.
 *
 * Usage:
 *   ./db-export.php database.sqlite users
 *   ./db-export.php database.sqlite orders --chunk-size=500 --output=orders.json
 *   ./db-export.php database.sqlite products --where="price > 100" --format=csv
 */

use Signalforge\Terminal\Command;
use Signalforge\Terminal\Terminal;
use Signalforge\Terminal\Color;

class DbExportCommand extends Command
{
    private ?PDO $pdo = null;

    public function configure(): void
    {
        $this->setName('db-export')
             ->setDescription('Export database table to file with chunked loading')
             ->addArgument('database', 'Database file path (SQLite)', true)
             ->addArgument('table', 'Table name to export', true)
             ->addOption('output', 'o', 'Output file path', true)
             ->addOption('format', 'f', 'Output format (json, csv, table)', true, 'table')
             ->addOption('chunk-size', 'c', 'Records per chunk', true, '100')
             ->addOption('where', 'w', 'WHERE clause filter', true)
             ->addOption('columns', null, 'Comma-separated column list', true)
             ->addOption('limit', 'l', 'Maximum records to export', true)
             ->addOption('verbose', 'v', 'Show detailed progress');
    }

    public function execute(): int
    {
        $database = $this->getArgument('database');
        $table = $this->getArgument('table');
        $output = $this->getOption('output');
        $format = $this->getOption('format');
        $chunkSize = (int) $this->getOption('chunk-size');
        $where = $this->getOption('where');
        $columns = $this->getOption('columns');
        $limit = $this->getOption('limit') ? (int) $this->getOption('limit') : null;
        $verbose = $this->getOption('verbose');

        $this->info(Terminal::style(" DATABASE EXPORT ", ['bg' => Color::BLUE, 'fg' => Color::WHITE, 'bold' => true]));
        $this->newLine();

        // Connect to database
        $this->comment("  Connecting to database...");

        try {
            $this->pdo = new PDO("sqlite:{$database}", null, null, [
                PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            ]);
            $this->info("  Connected");
        } catch (PDOException $e) {
            $this->error("Connection failed: " . $e->getMessage());
            return 1;
        }

        // Validate table exists
        if (!$this->tableExists($table)) {
            $this->error("Table '{$table}' does not exist");
            $this->showAvailableTables();
            return 1;
        }

        // Get total count
        $total = $this->getRowCount($table, $where);

        if ($total === 0) {
            $this->warning("No records found in table '{$table}'");
            return 0;
        }

        if ($limit !== null && $limit < $total) {
            $total = $limit;
        }

        if ($verbose) {
            $this->comment("  Table: {$table}");
            $this->comment("  Total records: {$total}");
            $this->comment("  Chunk size: {$chunkSize}");
            if ($where) {
                $this->comment("  Filter: WHERE {$where}");
            }
            $this->newLine();
        }

        // Export data in chunks
        $data = $this->exportInChunks($table, $columns, $where, $chunkSize, $total, $limit, $verbose);

        if ($data === null) {
            return 1;
        }

        $this->newLine();

        // Output results
        if ($output) {
            return $this->saveToFile($data, $output, $format);
        }

        return $this->displayData($data, $format);
    }

    private function tableExists(string $table): bool
    {
        $stmt = $this->pdo->query("SELECT name FROM sqlite_master WHERE type='table' AND name=" . $this->pdo->quote($table));
        return $stmt->fetch() !== false;
    }

    private function showAvailableTables(): void
    {
        $stmt = $this->pdo->query("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
        $tables = $stmt->fetchAll(PDO::FETCH_COLUMN);

        if (count($tables) > 0) {
            $this->info("Available tables: " . implode(', ', $tables));
        }
    }

    private function getRowCount(string $table, ?string $where): int
    {
        $sql = "SELECT COUNT(*) FROM {$table}";
        if ($where) {
            $sql .= " WHERE {$where}";
        }

        $stmt = $this->pdo->query($sql);
        return (int) $stmt->fetchColumn();
    }

    private function exportInChunks(
        string $table,
        ?string $columns,
        ?string $where,
        int $chunkSize,
        int $total,
        ?int $limit,
        bool $verbose
    ): ?array {
        $data = [];
        $offset = 0;
        $exported = 0;

        $cols = $columns ?: '*';
        $baseSql = "SELECT {$cols} FROM {$table}";
        if ($where) {
            $baseSql .= " WHERE {$where}";
        }

        $bar = Terminal::progressBar($total, 'Exporting');
        $startTime = microtime(true);
        $startMemory = memory_get_usage(true);

        while ($exported < $total) {
            $remaining = $limit !== null ? min($chunkSize, $limit - $exported) : $chunkSize;
            $sql = "{$baseSql} LIMIT {$remaining} OFFSET {$offset}";

            try {
                $stmt = $this->pdo->query($sql);
                $chunk = $stmt->fetchAll(PDO::FETCH_ASSOC);
            } catch (PDOException $e) {
                $bar->finish(Terminal::style("Error", ['fg' => Color::RED]));
                $this->error("Query failed: " . $e->getMessage());
                return null;
            }

            if (empty($chunk)) {
                break;
            }

            foreach ($chunk as $row) {
                $data[] = $row;
                $exported++;
                $bar->advance();

                if ($limit !== null && $exported >= $limit) {
                    break 2;
                }
            }

            $offset += count($chunk);

            if ($verbose && $exported % ($chunkSize * 5) === 0) {
                $memUsage = round((memory_get_usage(true) - $startMemory) / 1024 / 1024, 1);
                // Memory usage shown via progress bar
            }
        }

        $elapsed = round(microtime(true) - $startTime, 2);
        $bar->finish("Exported {$exported} records in {$elapsed}s");

        return $data;
    }

    private function displayData(array $data, string $format): int
    {
        if (empty($data)) {
            $this->warning("No data to display");
            return 0;
        }

        $this->newLine();

        switch ($format) {
            case 'json':
                echo json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE) . "\n";
                break;

            case 'csv':
                $headers = array_keys($data[0]);
                echo implode(',', array_map(fn($h) => '"' . $h . '"', $headers)) . "\n";
                foreach ($data as $row) {
                    $values = array_map(fn($v) => '"' . str_replace('"', '""', (string)$v) . '"', array_values($row));
                    echo implode(',', $values) . "\n";
                }
                break;

            case 'table':
            default:
                $headers = array_keys($data[0]);
                $rows = array_map(fn($row) => array_values($row), $data);

                // Limit display to first 50 rows
                if (count($rows) > 50) {
                    $rows = array_slice($rows, 0, 50);
                    Terminal::table($headers, $rows, ['border' => 'rounded', 'headerStyle' => ['bold' => true, 'fg' => Color::CYAN]]);
                    $this->comment("  ... and " . (count($data) - 50) . " more rows");
                } else {
                    Terminal::table($headers, $rows, ['border' => 'rounded', 'headerStyle' => ['bold' => true, 'fg' => Color::CYAN]]);
                }
                break;
        }

        $this->newLine();
        $this->success("Displayed " . count($data) . " records");

        return 0;
    }

    private function saveToFile(array $data, string $file, string $format): int
    {
        $this->comment("  Writing to file...");

        $content = match($format) {
            'csv' => $this->toCsv($data),
            default => json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE),
        };

        if (file_put_contents($file, $content) === false) {
            $this->error("Failed to write to {$file}");
            return 1;
        }

        $size = strlen($content);
        $sizeStr = $size > 1048576
            ? round($size / 1048576, 1) . " MB"
            : ($size > 1024 ? round($size / 1024, 1) . " KB" : "{$size} bytes");

        $this->newLine();
        $this->success("Saved " . count($data) . " records to {$file} ({$sizeStr})");

        return 0;
    }

    private function toCsv(array $data): string
    {
        if (empty($data)) {
            return '';
        }

        $headers = array_keys($data[0]);
        $output = implode(',', array_map(fn($h) => '"' . $h . '"', $headers)) . "\n";

        foreach ($data as $row) {
            $values = array_map(fn($v) => '"' . str_replace('"', '""', (string)$v) . '"', array_values($row));
            $output .= implode(',', $values) . "\n";
        }

        return $output;
    }
}

// Run the command
$cmd = new DbExportCommand();
exit($cmd->run());
