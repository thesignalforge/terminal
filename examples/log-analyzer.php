#!/usr/bin/env php
<?php
/**
 * Log Analyzer Command Example
 *
 * Demonstrates analyzing log files with pattern matching,
 * statistics, and formatted output.
 *
 * Usage:
 *   ./log-analyzer.php /var/log/app.log
 *   ./log-analyzer.php access.log --level=error
 *   ./log-analyzer.php app.log --since="1 hour ago" --stats
 *   ./log-analyzer.php *.log --pattern="timeout|error" --tail=50
 */

use Signalforge\Terminal\Command;
use Signalforge\Terminal\Terminal;
use Signalforge\Terminal\Color;

class LogAnalyzerCommand extends Command
{
    private array $stats = [
        'total' => 0,
        'debug' => 0,
        'info' => 0,
        'warning' => 0,
        'error' => 0,
        'critical' => 0,
    ];

    public function configure(): void
    {
        $this->setName('log-analyzer')
             ->setDescription('Analyze and filter log files with statistics')
             ->addArgument('file', 'Log file path (supports wildcards)', true)
             ->addOption('level', 'l', 'Filter by log level (debug, info, warning, error, critical)', true)
             ->addOption('pattern', 'p', 'Regex pattern to match', true)
             ->addOption('since', 's', 'Show entries since (e.g., "1 hour ago", "2024-01-01")', true)
             ->addOption('tail', 't', 'Show last N entries', true)
             ->addOption('stats', null, 'Show statistics only')
             ->addOption('verbose', 'v', 'Show all matched entries');
    }

    public function execute(): int
    {
        $filePattern = $this->getArgument('file');
        $level = $this->getOption('level');
        $pattern = $this->getOption('pattern');
        $since = $this->getOption('since');
        $tail = $this->getOption('tail') ? (int) $this->getOption('tail') : null;
        $statsOnly = $this->getOption('stats');
        $verbose = $this->getOption('verbose');

        $this->info(Terminal::style(" LOG ANALYZER ", ['bg' => Color::MAGENTA, 'fg' => Color::WHITE, 'bold' => true]));
        $this->newLine();

        // Expand file pattern
        $files = glob($filePattern);

        if (empty($files)) {
            $this->error("No files found matching: {$filePattern}");
            return 1;
        }

        $this->comment("  Files: " . implode(', ', array_map('basename', $files)));

        // Parse since timestamp
        $sinceTs = null;
        if ($since) {
            $sinceTs = strtotime($since);
            if ($sinceTs === false) {
                $this->error("Invalid date format: {$since}");
                return 1;
            }
            $this->comment("  Since: " . date('Y-m-d H:i:s', $sinceTs));
        }

        if ($level) {
            $this->comment("  Level filter: {$level}");
        }
        if ($pattern) {
            $this->comment("  Pattern: {$pattern}");
        }

        $this->newLine();

        // Analyze files
        $entries = [];
        foreach ($files as $file) {
            $fileEntries = $this->analyzeFile($file, $level, $pattern, $sinceTs, $verbose);
            $entries = array_merge($entries, $fileEntries);
        }

        // Apply tail limit
        if ($tail !== null && count($entries) > $tail) {
            $entries = array_slice($entries, -$tail);
        }

        $this->newLine();

        // Show statistics
        $this->showStatistics();

        // Show entries if not stats-only
        if (!$statsOnly && !empty($entries)) {
            $this->newLine();
            $this->showEntries($entries, $verbose);
        }

        return 0;
    }

    private function analyzeFile(string $file, ?string $levelFilter, ?string $pattern, ?int $sinceTs, bool $verbose): array
    {
        if (!is_readable($file)) {
            $this->warning("Cannot read: {$file}");
            return [];
        }

        $entries = [];
        $lines = file($file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
        $total = count($lines);

        $bar = Terminal::progressBar($total, basename($file));

        foreach ($lines as $line) {
            $bar->advance();

            $entry = $this->parseLine($line);
            if ($entry === null) {
                continue;
            }

            $this->stats['total']++;
            $this->stats[$entry['level']]++;

            // Apply filters
            if ($levelFilter && $entry['level'] !== strtolower($levelFilter)) {
                continue;
            }

            if ($sinceTs && $entry['timestamp'] < $sinceTs) {
                continue;
            }

            if ($pattern && !preg_match("/{$pattern}/i", $entry['message'])) {
                continue;
            }

            $entries[] = $entry;
        }

        $bar->finish("Processed " . count($lines) . " lines");

        return $entries;
    }

    private function parseLine(string $line): ?array
    {
        // Common log format: [2024-01-15 10:30:45] app.ERROR: Message here {"context":"data"}
        if (preg_match('/^\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\]\s+\w+\.(\w+):\s+(.+)$/', $line, $matches)) {
            return [
                'timestamp' => strtotime($matches[1]),
                'datetime' => $matches[1],
                'level' => strtolower($matches[2]),
                'message' => $matches[3],
                'raw' => $line,
            ];
        }

        // Apache/Nginx access log format
        if (preg_match('/^(\S+) .* \[([^\]]+)\] "([^"]+)" (\d+)/', $line, $matches)) {
            $status = (int) $matches[4];
            $level = $status >= 500 ? 'error' : ($status >= 400 ? 'warning' : 'info');

            return [
                'timestamp' => strtotime($matches[2]),
                'datetime' => $matches[2],
                'level' => $level,
                'message' => $matches[3] . " ({$status})",
                'raw' => $line,
            ];
        }

        // Simple format: [LEVEL] message
        if (preg_match('/^\[?(DEBUG|INFO|WARNING|ERROR|CRITICAL)\]?\s*:?\s*(.+)$/i', $line, $matches)) {
            return [
                'timestamp' => time(),
                'datetime' => date('Y-m-d H:i:s'),
                'level' => strtolower($matches[1]),
                'message' => $matches[2],
                'raw' => $line,
            ];
        }

        return null;
    }

    private function showStatistics(): void
    {
        $this->info(Terminal::style(" STATISTICS ", ['bg' => Color::CYAN, 'fg' => Color::BLACK, 'bold' => true]));
        $this->newLine();

        $rows = [
            ['Total Entries', $this->stats['total'], ''],
            ['Debug', $this->stats['debug'], $this->getBar($this->stats['debug'], $this->stats['total'], Color::BRIGHT_BLACK)],
            ['Info', $this->stats['info'], $this->getBar($this->stats['info'], $this->stats['total'], Color::BLUE)],
            ['Warning', $this->stats['warning'], $this->getBar($this->stats['warning'], $this->stats['total'], Color::YELLOW)],
            ['Error', $this->stats['error'], $this->getBar($this->stats['error'], $this->stats['total'], Color::RED)],
            ['Critical', $this->stats['critical'], $this->getBar($this->stats['critical'], $this->stats['total'], Color::BRIGHT_RED)],
        ];

        Terminal::table(['Level', 'Count', 'Distribution'], $rows, [
            'border' => 'rounded',
            'headerStyle' => ['bold' => true, 'fg' => Color::CYAN],
        ]);
    }

    private function getBar(int $count, int $total, string $color): string
    {
        if ($total === 0) return '';

        $percent = ($count / $total) * 100;
        $width = 20;
        $filled = (int) round(($count / max($total, 1)) * $width);

        $bar = str_repeat('█', $filled) . str_repeat('░', $width - $filled);

        return Terminal::style($bar, ['fg' => $color]) . sprintf(' %.1f%%', $percent);
    }

    private function showEntries(array $entries, bool $verbose): void
    {
        $this->info(Terminal::style(" MATCHED ENTRIES ", ['bg' => Color::GREEN, 'fg' => Color::BLACK, 'bold' => true]));
        $this->newLine();

        $limit = $verbose ? count($entries) : min(20, count($entries));

        for ($i = 0; $i < $limit; $i++) {
            $entry = $entries[$i];
            $levelColor = match($entry['level']) {
                'debug' => Color::BRIGHT_BLACK,
                'info' => Color::BLUE,
                'warning' => Color::YELLOW,
                'error' => Color::RED,
                'critical' => Color::BRIGHT_RED,
                default => Color::WHITE,
            };

            $levelBadge = Terminal::style(strtoupper(str_pad($entry['level'], 8)), [
                'fg' => Color::WHITE,
                'bg' => $levelColor,
                'bold' => true,
            ]);

            $datetime = Terminal::style($entry['datetime'], ['fg' => Color::BRIGHT_BLACK]);
            $message = strlen($entry['message']) > 80
                ? substr($entry['message'], 0, 77) . '...'
                : $entry['message'];

            echo "  {$datetime} {$levelBadge} {$message}\n";
        }

        if (count($entries) > $limit) {
            $this->newLine();
            $this->comment("  ... and " . (count($entries) - $limit) . " more entries (use --verbose to see all)");
        }

        $this->newLine();
        $this->success("Found " . count($entries) . " matching entries");
    }
}

// Run the command
$cmd = new LogAnalyzerCommand();
exit($cmd->run());
