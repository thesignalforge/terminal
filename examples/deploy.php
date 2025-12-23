#!/usr/bin/env php
<?php
/**
 * Deploy Command Example
 *
 * Demonstrates a deployment workflow with multiple stages,
 * confirmations, and rollback capability.
 *
 * Usage:
 *   ./deploy.php production
 *   ./deploy.php staging --skip-tests
 *   ./deploy.php production --dry-run
 *   ./deploy.php production --force --no-backup
 */

use Signalforge\Terminal\Command;
use Signalforge\Terminal\Terminal;
use Signalforge\Terminal\Color;

class DeployCommand extends Command
{
    private const ENVIRONMENTS = [
        'staging' => [
            'server' => 'staging.example.com',
            'path' => '/var/www/staging',
            'branch' => 'develop',
        ],
        'production' => [
            'server' => 'prod.example.com',
            'path' => '/var/www/production',
            'branch' => 'main',
        ],
    ];

    private array $steps = [];
    private bool $dryRun = false;

    public function configure(): void
    {
        $this->setName('deploy')
             ->setDescription('Deploy application to staging or production environment')
             ->addArgument('environment', 'Target environment (staging, production)', true)
             ->addOption('skip-tests', null, 'Skip running tests before deploy')
             ->addOption('no-backup', null, 'Skip creating backup before deploy')
             ->addOption('force', 'f', 'Skip confirmation prompts')
             ->addOption('dry-run', null, 'Simulate deployment without making changes')
             ->addOption('verbose', 'v', 'Show detailed output');
    }

    public function execute(): int
    {
        $env = $this->getArgument('environment');
        $skipTests = $this->getOption('skip-tests');
        $noBackup = $this->getOption('no-backup');
        $force = $this->getOption('force');
        $this->dryRun = $this->getOption('dry-run');
        $verbose = $this->getOption('verbose');

        // Validate environment
        if (!isset(self::ENVIRONMENTS[$env])) {
            $this->error("Unknown environment: {$env}");
            $this->info("Available: " . implode(', ', array_keys(self::ENVIRONMENTS)));
            return 1;
        }

        $config = self::ENVIRONMENTS[$env];

        // Header
        $this->printHeader($env, $config);

        if ($this->dryRun) {
            $this->warning("DRY RUN MODE - No changes will be made");
            $this->newLine();
        }

        // Confirmation for production
        if ($env === 'production' && !$force && !$this->dryRun) {
            if (!$this->confirmDeploy($env)) {
                $this->info("Deployment cancelled");
                return 0;
            }
        }

        // Build deployment steps
        $this->steps = $this->buildSteps($skipTests, $noBackup);
        $totalSteps = count($this->steps);

        $this->newLine();
        $this->info(Terminal::style(" DEPLOYMENT STEPS ", ['bg' => Color::BLUE, 'fg' => Color::WHITE, 'bold' => true]));
        $this->newLine();

        // Execute each step
        $currentStep = 0;
        $failed = false;

        foreach ($this->steps as $step) {
            $currentStep++;
            $stepNum = str_pad($currentStep, 2, '0', STR_PAD_LEFT);

            echo Terminal::style("  [{$stepNum}/{$totalSteps}] ", ['fg' => Color::CYAN, 'bold' => true]);
            echo $step['name'];

            $loader = Terminal::loader('', 'dots');
            $loader->start();

            $success = $this->executeStep($step, $verbose);

            if ($success) {
                $loader->stop(Terminal::style(' OK', ['fg' => Color::GREEN, 'bold' => true]));
            } else {
                $loader->stop(Terminal::style(' FAILED', ['fg' => Color::RED, 'bold' => true]));
                $failed = true;
                break;
            }
        }

        $this->newLine();

        // Summary
        if ($failed) {
            $this->printFailure($currentStep, $totalSteps);
            return 1;
        }

        $this->printSuccess($env, $config);
        return 0;
    }

    private function printHeader(string $env, array $config): void
    {
        $envColor = $env === 'production' ? Color::RED : Color::YELLOW;

        $this->newLine();
        echo Terminal::style("  ╔════════════════════════════════════════╗\n", ['fg' => $envColor]);
        echo Terminal::style("  ║", ['fg' => $envColor]);
        echo Terminal::style("        DEPLOYMENT SYSTEM               ", ['bold' => true]);
        echo Terminal::style("║\n", ['fg' => $envColor]);
        echo Terminal::style("  ╚════════════════════════════════════════╝\n", ['fg' => $envColor]);
        $this->newLine();

        Terminal::table(
            ['Setting', 'Value'],
            [
                ['Environment', Terminal::style(strtoupper($env), ['fg' => $envColor, 'bold' => true])],
                ['Server', $config['server']],
                ['Path', $config['path']],
                ['Branch', $config['branch']],
                ['Time', date('Y-m-d H:i:s')],
            ],
            ['border' => 'single']
        );
    }

    private function confirmDeploy(string $env): bool
    {
        $this->newLine();
        $this->warning("You are about to deploy to " . strtoupper($env));
        $this->newLine();

        echo Terminal::style("  Type ", ['fg' => Color::WHITE]);
        echo Terminal::style(strtoupper($env), ['fg' => Color::RED, 'bold' => true]);
        echo Terminal::style(" to confirm: ", ['fg' => Color::WHITE]);

        // Simulate confirmation (in real usage, would read input)
        // For demo purposes, we'll auto-confirm
        echo Terminal::style($env . "\n", ['fg' => Color::GREEN]);

        return true;
    }

    private function buildSteps(bool $skipTests, bool $noBackup): array
    {
        $steps = [];

        $steps[] = ['name' => 'Check git status', 'action' => 'git_status', 'duration' => 500];
        $steps[] = ['name' => 'Pull latest changes', 'action' => 'git_pull', 'duration' => 1500];
        $steps[] = ['name' => 'Install dependencies', 'action' => 'composer_install', 'duration' => 3000];

        if (!$skipTests) {
            $steps[] = ['name' => 'Run test suite', 'action' => 'run_tests', 'duration' => 5000];
        }

        $steps[] = ['name' => 'Build assets', 'action' => 'build_assets', 'duration' => 2000];

        if (!$noBackup) {
            $steps[] = ['name' => 'Create backup', 'action' => 'backup', 'duration' => 2000];
        }

        $steps[] = ['name' => 'Run migrations', 'action' => 'migrate', 'duration' => 1000];
        $steps[] = ['name' => 'Clear caches', 'action' => 'clear_cache', 'duration' => 500];
        $steps[] = ['name' => 'Warm up caches', 'action' => 'warm_cache', 'duration' => 1500];
        $steps[] = ['name' => 'Restart services', 'action' => 'restart', 'duration' => 1000];
        $steps[] = ['name' => 'Health check', 'action' => 'health_check', 'duration' => 500];

        return $steps;
    }

    private function executeStep(array $step, bool $verbose): bool
    {
        // Simulate step execution
        $duration = $this->dryRun ? 100 : $step['duration'];

        // Simulate work with small delays
        $ticks = 10;
        for ($i = 0; $i < $ticks; $i++) {
            usleep($duration * 100);
        }

        // Simulate occasional failures (for demo, always succeed)
        return true;
    }

    private function printSuccess(string $env, array $config): void
    {
        echo Terminal::style("  ╔════════════════════════════════════════╗\n", ['fg' => Color::GREEN]);
        echo Terminal::style("  ║", ['fg' => Color::GREEN]);
        echo Terminal::style("      DEPLOYMENT SUCCESSFUL!            ", ['fg' => Color::GREEN, 'bold' => true]);
        echo Terminal::style("║\n", ['fg' => Color::GREEN]);
        echo Terminal::style("  ╚════════════════════════════════════════╝\n", ['fg' => Color::GREEN]);
        $this->newLine();

        if ($this->dryRun) {
            $this->comment("  This was a dry run. No changes were made.");
        } else {
            $this->success("  Application deployed to {$config['server']}");
            $this->info("  URL: https://{$config['server']}/");
        }
        $this->newLine();
    }

    private function printFailure(int $failedStep, int $totalSteps): void
    {
        echo Terminal::style("  ╔════════════════════════════════════════╗\n", ['fg' => Color::RED]);
        echo Terminal::style("  ║", ['fg' => Color::RED]);
        echo Terminal::style("        DEPLOYMENT FAILED!              ", ['fg' => Color::RED, 'bold' => true]);
        echo Terminal::style("║\n", ['fg' => Color::RED]);
        echo Terminal::style("  ╚════════════════════════════════════════╝\n", ['fg' => Color::RED]);
        $this->newLine();

        $this->error("  Failed at step {$failedStep}/{$totalSteps}");
        $this->info("  Run with --verbose for more details");
        $this->info("  Consider rolling back with: ./deploy.php rollback");
        $this->newLine();
    }
}

// Run the command
$cmd = new DeployCommand();
exit($cmd->run());
