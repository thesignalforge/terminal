#!/usr/bin/env php
<?php
/**
 * File Search Command Example
 *
 * Demonstrates recursive file searching with pattern matching,
 * content search, and formatted results.
 *
 * Usage:
 *   ./file-search.php "TODO" src/
 *   ./file-search.php "function \w+" --ext=php,js
 *   ./file-search.php "password" --ignore=vendor,node_modules
 *   ./file-search.php "deprecated" --context=2 --max-results=50
 */

use Signalforge\Terminal\Command;
use Signalforge\Terminal\Terminal;
use Signalforge\Terminal\Color;

class FileSearchCommand extends Command
{
    private int $matchCount = 0;
    private int $fileCount = 0;
    private int $filesWithMatches = 0;

    public function configure(): void
    {
        $this->setName('search')
             ->setDescription('Search for patterns in files recursively')
             ->addArgument('pattern', 'Search pattern (regex supported)', true)
             ->addArgument('path', 'Directory or file to search', false, '.')
             ->addOption('ext', 'e', 'File extensions to include (comma-separated)', true)
             ->addOption('ignore', 'i', 'Directories to ignore (comma-separated)', true, 'vendor,node_modules,.git')
             ->addOption('context', 'C', 'Lines of context around matches', true, '0')
             ->addOption('max-results', 'm', 'Maximum results to show', true, '100')
             ->addOption('case-sensitive', 's', 'Case-sensitive search')
             ->addOption('files-only', 'l', 'Only show filenames with matches');
    }

    public function execute(): int
    {
        $pattern = $this->getArgument('pattern');
        $path = $this->getArgument('path');
        $extensions = $this->getOption('ext');
        $ignore = $this->getOption('ignore');
        $context = (int) $this->getOption('context');
        $maxResults = (int) $this->getOption('max-results');
        $caseSensitive = $this->getOption('case-sensitive');
        $filesOnly = $this->getOption('files-only');

        // Build regex
        $flags = $caseSensitive ? '' : 'i';
        $escapedPattern = $pattern;
        $regex = "/{$escapedPattern}/{$flags}";

        // Validate regex
        if (@preg_match($regex, '') === false) {
            $this->error("Invalid regex pattern: {$pattern}");
            return 1;
        }

        // Parse options
        $extList = $extensions ? explode(',', $extensions) : null;
        $ignoreList = $ignore ? explode(',', $ignore) : [];

        if (!file_exists($path)) {
            $this->error("Path not found: {$path}");
            return 1;
        }

        $this->info(Terminal::style(" FILE SEARCH ", ['bg' => Color::BLUE, 'fg' => Color::WHITE, 'bold' => true]));
        $this->newLine();
        $this->comment("  Pattern: {$pattern}");
        $this->comment("  Path: " . realpath($path));
        if ($extList) {
            $this->comment("  Extensions: " . implode(', ', $extList));
        }
        $this->newLine();

        // Collect files
        $this->comment("  Scanning files...");
        $files = $this->collectFiles($path, $extList, $ignoreList);
        $this->info("  Found " . count($files) . " files");

        if (empty($files)) {
            $this->warning("No files to search");
            return 0;
        }

        $this->newLine();

        // Search files
        $results = [];
        $bar = Terminal::progressBar(count($files), 'Searching');

        foreach ($files as $file) {
            $this->fileCount++;
            $fileMatches = $this->searchFile($file, $regex, $context);

            if (!empty($fileMatches)) {
                $this->filesWithMatches++;
                $results[$file] = $fileMatches;
                $this->matchCount += count($fileMatches);
            }

            $bar->advance();

            if (!$filesOnly && $this->matchCount >= $maxResults) {
                break;
            }
        }

        $bar->finish("Searched {$this->fileCount} files");
        $this->newLine();

        // Output results
        if ($filesOnly) {
            $this->printFiles(array_keys($results));
        } else {
            $this->printResults($results, $maxResults);
        }

        return $this->matchCount > 0 ? 0 : 1;
    }

    private function collectFiles(string $path, ?array $extensions, array $ignore): array
    {
        $files = [];

        if (is_file($path)) {
            return [$path];
        }

        $dirIterator = new RecursiveDirectoryIterator(
            $path,
            RecursiveDirectoryIterator::SKIP_DOTS
        );

        $iterator = new RecursiveIteratorIterator($dirIterator);

        foreach ($iterator as $file) {
            if (!$file->isFile()) {
                continue;
            }

            $filePath = $file->getPathname();

            // Check ignored directories
            foreach ($ignore as $ig) {
                if (strpos($filePath, DIRECTORY_SEPARATOR . $ig . DIRECTORY_SEPARATOR) !== false) {
                    continue 2;
                }
            }

            // Check extension
            if ($extensions !== null) {
                $ext = pathinfo($filePath, PATHINFO_EXTENSION);
                if (!in_array($ext, $extensions)) {
                    continue;
                }
            }

            // Skip binary files (simple check)
            $finfo = finfo_open(FILEINFO_MIME_TYPE);
            $mime = finfo_file($finfo, $filePath);
            finfo_close($finfo);

            if (strpos($mime, 'text/') !== 0 && $mime !== 'application/json') {
                continue;
            }

            $files[] = $filePath;
        }

        return $files;
    }

    private function searchFile(string $file, string $regex, int $context): array
    {
        $lines = @file($file, FILE_IGNORE_NEW_LINES);
        if ($lines === false) {
            return [];
        }

        $matches = [];
        $lineCount = count($lines);

        foreach ($lines as $lineNum => $line) {
            if (preg_match($regex, $line)) {
                $start = max(0, $lineNum - $context);
                $end = min($lineCount - 1, $lineNum + $context);

                $contextLines = [];
                for ($i = $start; $i <= $end; $i++) {
                    $contextLines[] = [
                        'num' => $i + 1,
                        'text' => $lines[$i],
                        'match' => ($i === $lineNum),
                    ];
                }

                $matches[] = [
                    'line' => $lineNum + 1,
                    'context' => $contextLines,
                ];
            }
        }

        return $matches;
    }

    private function printResults(array $results, int $maxResults): void
    {
        $shown = 0;

        foreach ($results as $file => $matches) {
            $relativePath = $this->relativePath($file);
            echo Terminal::style($relativePath, ['fg' => Color::MAGENTA, 'bold' => true]) . "\n";

            foreach ($matches as $match) {
                if ($shown >= $maxResults) {
                    break 2;
                }

                foreach ($match['context'] as $ctx) {
                    $lineNum = str_pad($ctx['num'], 4, ' ', STR_PAD_LEFT);

                    if ($ctx['match']) {
                        echo Terminal::style($lineNum, ['fg' => Color::GREEN]) . ": ";
                        echo $ctx['text'] . "\n";
                    } else {
                        echo Terminal::style($lineNum, ['fg' => Color::BRIGHT_BLACK]) . "  ";
                        echo Terminal::style($ctx['text'], ['fg' => Color::BRIGHT_BLACK]) . "\n";
                    }
                }

                $shown++;

                if (count($match['context']) > 1) {
                    echo Terminal::style("  ---", ['fg' => Color::BRIGHT_BLACK]) . "\n";
                }
            }

            echo "\n";
        }

        $this->printSummary($maxResults);
    }

    private function printFiles(array $files): void
    {
        foreach ($files as $file) {
            echo Terminal::style($this->relativePath($file), ['fg' => Color::MAGENTA]) . "\n";
        }
        $this->newLine();
        $this->printSummary(PHP_INT_MAX);
    }

    private function printSummary(int $maxResults): void
    {
        if ($this->matchCount === 0) {
            $this->warning("No matches found");
            return;
        }

        $truncated = $this->matchCount > $maxResults;
        $shown = $truncated ? $maxResults : $this->matchCount;

        $this->success(
            "Found {$this->matchCount} matches in {$this->filesWithMatches} files" .
            ($truncated ? " (showing first {$shown})" : "")
        );
    }

    private function relativePath(string $file): string
    {
        $cwd = getcwd();
        if (strpos($file, $cwd) === 0) {
            return '.' . substr($file, strlen($cwd));
        }
        return $file;
    }
}

// Run the command
$cmd = new FileSearchCommand();
exit($cmd->run());
