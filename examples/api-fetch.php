#!/usr/bin/env php
<?php
/**
 * API Fetch Command Example
 *
 * Demonstrates fetching data from a REST API with pagination,
 * progress tracking, and formatted output.
 *
 * Usage:
 *   ./api-fetch.php users
 *   ./api-fetch.php posts --limit=50
 *   ./api-fetch.php comments --format=table --limit=10
 *   ./api-fetch.php users --output=users.json
 */

use Signalforge\Terminal\Command;
use Signalforge\Terminal\Terminal;
use Signalforge\Terminal\Color;

class ApiFetchCommand extends Command
{
    // Using JSONPlaceholder as example API
    private const BASE_URL = 'https://jsonplaceholder.typicode.com';

    private const ENDPOINTS = [
        'users' => ['fields' => ['id', 'name', 'email', 'company.name']],
        'posts' => ['fields' => ['id', 'title', 'userId']],
        'comments' => ['fields' => ['id', 'name', 'email']],
        'albums' => ['fields' => ['id', 'title', 'userId']],
        'todos' => ['fields' => ['id', 'title', 'completed']],
        'photos' => ['fields' => ['id', 'title', 'albumId']],
    ];

    public function configure(): void
    {
        $endpoints = implode(', ', array_keys(self::ENDPOINTS));

        $this->setName('api-fetch')
             ->setDescription("Fetch data from JSONPlaceholder API ({$endpoints})")
             ->addArgument('endpoint', 'API endpoint to fetch', true)
             ->addOption('limit', 'l', 'Maximum number of records', true, '20')
             ->addOption('format', 'f', 'Output format (table, json, csv)', true, 'table')
             ->addOption('output', 'o', 'Save to file instead of stdout', true)
             ->addOption('verbose', 'v', 'Show request details');
    }

    public function execute(): int
    {
        $endpoint = $this->getArgument('endpoint');
        $limit = (int) $this->getOption('limit');
        $format = $this->getOption('format');
        $output = $this->getOption('output');
        $verbose = $this->getOption('verbose');

        // Validate endpoint
        if (!isset(self::ENDPOINTS[$endpoint])) {
            $this->error("Unknown endpoint: {$endpoint}");
            $this->info("Available: " . implode(', ', array_keys(self::ENDPOINTS)));
            return 1;
        }

        $url = self::BASE_URL . "/{$endpoint}";

        $this->info(Terminal::style(" API FETCH ", ['bg' => Color::BLUE, 'fg' => Color::WHITE, 'bold' => true]));
        $this->newLine();

        if ($verbose) {
            $this->comment("  Endpoint: {$url}");
            $this->comment("  Limit: {$limit}");
            $this->comment("  Format: {$format}");
            $this->newLine();
        }

        // Fetch data
        $this->comment("  Fetching {$endpoint}...");

        $startTime = microtime(true);
        $data = $this->fetchData($url);
        $elapsed = round((microtime(true) - $startTime) * 1000);

        if ($data === null) {
            $this->error("Failed to fetch data");
            return 1;
        }

        $this->info("  Fetched " . count($data) . " records ({$elapsed}ms)");
        $this->newLine();

        // Apply limit
        if ($limit > 0 && count($data) > $limit) {
            $data = array_slice($data, 0, $limit);
            if ($verbose) {
                $this->comment("  Limited to {$limit} records");
            }
        }

        // Process and output
        $fields = self::ENDPOINTS[$endpoint]['fields'];
        $processed = $this->processData($data, $fields);

        if ($output) {
            return $this->saveToFile($processed, $output, $format, $fields);
        }

        return $this->displayData($processed, $format, $fields);
    }

    private function fetchData(string $url): ?array
    {
        $context = stream_context_create([
            'http' => [
                'timeout' => 30,
                'header' => "Accept: application/json\r\n"
            ]
        ]);

        $response = @file_get_contents($url, false, $context);

        if ($response === false) {
            return null;
        }

        $data = json_decode($response, true);

        if (!is_array($data)) {
            return null;
        }

        return $data;
    }

    private function processData(array $data, array $fields): array
    {
        $processed = [];

        $bar = Terminal::progressBar(count($data), 'Processing');

        foreach ($data as $item) {
            $row = [];
            foreach ($fields as $field) {
                $row[$field] = $this->getNestedValue($item, $field);
            }
            $processed[] = $row;
            $bar->advance();
        }

        $bar->finish('Data processed');

        return $processed;
    }

    private function getNestedValue(array $data, string $path): mixed
    {
        $keys = explode('.', $path);
        $value = $data;

        foreach ($keys as $key) {
            if (!isset($value[$key])) {
                return '';
            }
            $value = $value[$key];
        }

        if (is_bool($value)) {
            return $value ? 'Yes' : 'No';
        }

        return $value;
    }

    private function displayData(array $data, string $format, array $fields): int
    {
        $this->newLine();

        switch ($format) {
            case 'json':
                echo json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE) . "\n";
                break;

            case 'csv':
                // Header
                echo implode(',', array_map(fn($f) => '"' . $f . '"', $fields)) . "\n";
                // Rows
                foreach ($data as $row) {
                    $values = array_map(fn($v) => '"' . str_replace('"', '""', $v) . '"', array_values($row));
                    echo implode(',', $values) . "\n";
                }
                break;

            case 'table':
            default:
                // Format headers nicely
                $headers = array_map(fn($f) => ucwords(str_replace(['.', '_'], ' ', $f)), $fields);

                // Prepare rows
                $rows = array_map(fn($row) => array_values($row), $data);

                Terminal::table($headers, $rows, [
                    'border' => 'rounded',
                    'headerStyle' => ['bold' => true, 'fg' => Color::CYAN],
                ]);
                break;
        }

        $this->newLine();
        $this->success("Displayed " . count($data) . " records");

        return 0;
    }

    private function saveToFile(array $data, string $file, string $format, array $fields): int
    {
        $this->newLine();

        $content = match($format) {
            'json' => json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE),
            'csv' => $this->toCsv($data, $fields),
            default => json_encode($data, JSON_PRETTY_PRINT),
        };

        if (file_put_contents($file, $content) === false) {
            $this->error("Failed to write to {$file}");
            return 1;
        }

        $size = strlen($content);
        $sizeStr = $size > 1024 ? round($size / 1024, 1) . " KB" : "{$size} bytes";

        $this->success("Saved " . count($data) . " records to {$file} ({$sizeStr})");

        return 0;
    }

    private function toCsv(array $data, array $fields): string
    {
        $output = implode(',', array_map(fn($f) => '"' . $f . '"', $fields)) . "\n";

        foreach ($data as $row) {
            $values = array_map(fn($v) => '"' . str_replace('"', '""', (string)$v) . '"', array_values($row));
            $output .= implode(',', $values) . "\n";
        }

        return $output;
    }
}

// Run the command
$cmd = new ApiFetchCommand();
exit($cmd->run());
