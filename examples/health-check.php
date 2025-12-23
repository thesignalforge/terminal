#!/usr/bin/env php
<?php
/**
 * Health Check Command Example
 *
 * Demonstrates a system health check with multiple service checks,
 * status indicators, and summary reporting.
 *
 * Usage:
 *   ./health-check.php
 *   ./health-check.php --services=web,db,cache
 *   ./health-check.php --json --quiet
 *   ./health-check.php --watch --interval=30
 */

use Signalforge\Terminal\Command;
use Signalforge\Terminal\Terminal;
use Signalforge\Terminal\Color;

class HealthCheckCommand extends Command
{
    private const SERVICES = [
        'web' => ['name' => 'Web Server', 'check' => 'checkWeb'],
        'db' => ['name' => 'Database', 'check' => 'checkDatabase'],
        'cache' => ['name' => 'Cache (Redis)', 'check' => 'checkCache'],
        'queue' => ['name' => 'Queue Worker', 'check' => 'checkQueue'],
        'storage' => ['name' => 'Storage', 'check' => 'checkStorage'],
        'mail' => ['name' => 'Mail Service', 'check' => 'checkMail'],
        'search' => ['name' => 'Search Index', 'check' => 'checkSearch'],
    ];

    private array $results = [];

    public function configure(): void
    {
        $this->setName('health-check')
             ->setDescription('Check health status of application services')
             ->addOption('services', 's', 'Comma-separated list of services to check', true)
             ->addOption('json', null, 'Output results as JSON')
             ->addOption('quiet', 'q', 'Only output on failure')
             ->addOption('watch', 'w', 'Continuously monitor (Ctrl+C to stop)')
             ->addOption('interval', 'i', 'Watch interval in seconds', true, '10');
    }

    public function execute(): int
    {
        $serviceList = $this->getOption('services');
        $jsonOutput = $this->getOption('json');
        $quiet = $this->getOption('quiet');
        $watch = $this->getOption('watch');
        $interval = (int) $this->getOption('interval');

        // Parse services
        $services = $serviceList
            ? array_intersect_key(self::SERVICES, array_flip(explode(',', $serviceList)))
            : self::SERVICES;

        if (empty($services)) {
            $this->error("No valid services specified");
            $this->info("Available: " . implode(', ', array_keys(self::SERVICES)));
            return 1;
        }

        if ($watch) {
            return $this->watchMode($services, $interval, $jsonOutput, $quiet);
        }

        return $this->runChecks($services, $jsonOutput, $quiet);
    }

    private function runChecks(array $services, bool $jsonOutput, bool $quiet): int
    {
        $this->results = [];
        $allHealthy = true;

        if (!$jsonOutput && !$quiet) {
            $this->printHeader();
        }

        foreach ($services as $key => $service) {
            $check = $service['check'];
            $result = $this->$check();

            $this->results[$key] = [
                'name' => $service['name'],
                'status' => $result['status'],
                'message' => $result['message'] ?? '',
                'latency' => $result['latency'] ?? null,
                'details' => $result['details'] ?? [],
            ];

            if ($result['status'] !== 'healthy') {
                $allHealthy = false;
            }
        }

        if ($jsonOutput) {
            echo json_encode([
                'timestamp' => date('c'),
                'healthy' => $allHealthy,
                'services' => $this->results,
            ], JSON_PRETTY_PRINT) . "\n";
        } elseif (!$quiet || !$allHealthy) {
            $this->printResults();
        }

        return $allHealthy ? 0 : 1;
    }

    private function watchMode(array $services, int $interval, bool $jsonOutput, bool $quiet): int
    {
        $this->info("Starting health check monitor (interval: {$interval}s)");
        $this->comment("Press Ctrl+C to stop");
        $this->newLine();

        Terminal::enter();
        Terminal::alternateScreen(true);

        try {
            while (true) {
                Terminal::clear();
                Terminal::cursorTo(0, 0);

                $this->runChecks($services, $jsonOutput, $quiet);

                echo "\n";
                $this->comment("  Last check: " . date('H:i:s') . " | Next in {$interval}s | Ctrl+C to exit");

                sleep($interval);
            }
        } finally {
            Terminal::alternateScreen(false);
            Terminal::exit();
        }

        return 0;
    }

    private function printHeader(): void
    {
        $this->newLine();
        $this->info(Terminal::style(" HEALTH CHECK ", ['bg' => Color::BLUE, 'fg' => Color::WHITE, 'bold' => true]));
        $this->comment("  " . date('Y-m-d H:i:s'));
        $this->newLine();
    }

    private function printResults(): void
    {
        $rows = [];
        $healthy = 0;
        $degraded = 0;
        $unhealthy = 0;

        foreach ($this->results as $result) {
            $statusIcon = match($result['status']) {
                'healthy' => Terminal::style('●', ['fg' => Color::GREEN]),
                'degraded' => Terminal::style('●', ['fg' => Color::YELLOW]),
                default => Terminal::style('●', ['fg' => Color::RED]),
            };

            $statusText = match($result['status']) {
                'healthy' => Terminal::style('Healthy', ['fg' => Color::GREEN]),
                'degraded' => Terminal::style('Degraded', ['fg' => Color::YELLOW]),
                default => Terminal::style('Unhealthy', ['fg' => Color::RED]),
            };

            $latency = $result['latency'] !== null
                ? ($result['latency'] < 100
                    ? Terminal::style("{$result['latency']}ms", ['fg' => Color::GREEN])
                    : ($result['latency'] < 500
                        ? Terminal::style("{$result['latency']}ms", ['fg' => Color::YELLOW])
                        : Terminal::style("{$result['latency']}ms", ['fg' => Color::RED])))
                : '-';

            $rows[] = [
                $statusIcon . ' ' . $result['name'],
                $statusText,
                $latency,
                $result['message'],
            ];

            match($result['status']) {
                'healthy' => $healthy++,
                'degraded' => $degraded++,
                default => $unhealthy++,
            };
        }

        Terminal::table(
            ['Service', 'Status', 'Latency', 'Details'],
            $rows,
            ['border' => 'rounded', 'headerStyle' => ['bold' => true, 'fg' => Color::CYAN]]
        );

        $this->newLine();

        // Summary
        $total = count($this->results);
        $summary = [];

        if ($healthy > 0) {
            $summary[] = Terminal::style("{$healthy} healthy", ['fg' => Color::GREEN]);
        }
        if ($degraded > 0) {
            $summary[] = Terminal::style("{$degraded} degraded", ['fg' => Color::YELLOW]);
        }
        if ($unhealthy > 0) {
            $summary[] = Terminal::style("{$unhealthy} unhealthy", ['fg' => Color::RED]);
        }

        echo "  " . implode(' | ', $summary) . " (total: {$total})\n";
        $this->newLine();

        if ($unhealthy > 0) {
            $this->error("System is experiencing issues!");
        } elseif ($degraded > 0) {
            $this->warning("System is degraded but operational");
        } else {
            $this->success("All systems operational");
        }
    }

    // Simulated health checks
    private function checkWeb(): array
    {
        usleep(rand(50000, 200000));
        return [
            'status' => 'healthy',
            'latency' => rand(10, 80),
            'message' => 'Responding normally',
        ];
    }

    private function checkDatabase(): array
    {
        usleep(rand(100000, 300000));
        return [
            'status' => 'healthy',
            'latency' => rand(5, 30),
            'message' => '12 active connections',
            'details' => ['pool_size' => 20, 'active' => 12],
        ];
    }

    private function checkCache(): array
    {
        usleep(rand(50000, 150000));
        $hitRate = rand(85, 99);
        return [
            'status' => $hitRate > 90 ? 'healthy' : 'degraded',
            'latency' => rand(1, 10),
            'message' => "Hit rate: {$hitRate}%",
            'details' => ['hit_rate' => $hitRate],
        ];
    }

    private function checkQueue(): array
    {
        usleep(rand(50000, 100000));
        $pending = rand(0, 500);
        $status = $pending < 100 ? 'healthy' : ($pending < 300 ? 'degraded' : 'unhealthy');
        return [
            'status' => $status,
            'latency' => rand(2, 15),
            'message' => "{$pending} jobs pending",
            'details' => ['pending' => $pending, 'workers' => 4],
        ];
    }

    private function checkStorage(): array
    {
        usleep(rand(100000, 200000));
        $used = rand(40, 85);
        return [
            'status' => $used < 80 ? 'healthy' : 'degraded',
            'latency' => rand(20, 100),
            'message' => "{$used}% used",
            'details' => ['used_percent' => $used],
        ];
    }

    private function checkMail(): array
    {
        usleep(rand(200000, 500000));
        return [
            'status' => 'healthy',
            'latency' => rand(150, 400),
            'message' => 'SMTP connected',
        ];
    }

    private function checkSearch(): array
    {
        usleep(rand(100000, 300000));
        $docs = rand(100000, 500000);
        return [
            'status' => 'healthy',
            'latency' => rand(30, 80),
            'message' => number_format($docs) . ' docs indexed',
            'details' => ['documents' => $docs],
        ];
    }
}

// Run the command
$cmd = new HealthCheckCommand();
exit($cmd->run());
