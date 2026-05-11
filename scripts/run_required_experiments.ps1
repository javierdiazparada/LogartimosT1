param(
  [string]$Cli = "build\rtree_cli.exe",
  [string]$RandomDataset = "data\random.bin",
  [string]$EuropaDataset = "data\europa.bin",
  [string]$ResultsDir = "results",
  [string]$TreesDir = "trees",
  [int]$MinExponent = 15,
  [int]$MaxExponent = 24,
  [int]$Queries = 100,
  [string[]]$Sides = @("0.0025", "0.005", "0.01", "0.025", "0.05"),
  [switch]$Fresh,
  [switch]$SkipPlots,
  [string]$Log = ""
)

$ErrorActionPreference = "Stop"

$Sides = @($Sides | ForEach-Object { $_ -split "," } | Where-Object { $_ -ne "" })

function Write-Log {
  param([string]$Message)
  $line = "$(Get-Date -Format o) $Message"
  Write-Host $line
  if ($Log) {
    Add-Content -Path $Log -Value $line
  }
}

function Quote-Arg {
  param([string]$Value)
  if ($Value -match '[\s"]') {
    return '"' + ($Value -replace '"', '\"') + '"'
  }
  return $Value
}

function Invoke-Step {
  param(
    [string]$FilePath,
    [string[]]$Arguments,
    [string]$Activity,
    [string]$Status,
    [double]$PercentComplete
  )

  $stdout = [System.IO.Path]::GetTempFileName()
  $stderr = [System.IO.Path]::GetTempFileName()
  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = $FilePath
  $psi.Arguments = ($Arguments | ForEach-Object { Quote-Arg $_ }) -join " "
  $psi.UseShellExecute = $false
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError = $true

  $process = New-Object System.Diagnostics.Process
  $process.StartInfo = $psi
  Write-Log "START $Status"
  [void]$process.Start()
  $timer = [System.Diagnostics.Stopwatch]::StartNew()

  while (-not $process.HasExited) {
    $elapsed = $timer.Elapsed.ToString("hh\:mm\:ss")
    Write-Progress -Activity $Activity `
      -Status "$Status running for $elapsed" `
      -PercentComplete ([Math]::Min(99, [Math]::Max(0, $PercentComplete)))
    Start-Sleep -Seconds 1
  }

  $process.WaitForExit()
  $timer.Stop()
  $out = $process.StandardOutput.ReadToEnd()
  $err = $process.StandardError.ReadToEnd()
  Remove-Item -Force -ErrorAction SilentlyContinue $stdout, $stderr

  if ($out.Trim()) {
    Write-Log ("STDOUT " + $out.Trim())
  }
  if ($err.Trim()) {
    Write-Log ("STDERR " + $err.Trim())
  }
  if ($process.ExitCode -ne 0) {
    throw "$Status failed with exit code $($process.ExitCode)"
  }

  Write-Progress -Activity $Activity -Status "$Status done" `
    -PercentComplete ([Math]::Min(100, [Math]::Max(0, $PercentComplete)))
  Write-Log "DONE $Status in $($timer.Elapsed.ToString('hh\:mm\:ss'))"
}

$buildCsv = Join-Path $ResultsDir "build_results.csv"
$queryCsv = Join-Path $ResultsDir "query_results.csv"
$summaryCsv = Join-Path $ResultsDir "query_summary.csv"
$systemInfo = Join-Path $ResultsDir "system_info.txt"

New-Item -ItemType Directory -Force -Path $ResultsDir, $TreesDir | Out-Null
if ($Log) {
  $logParent = Split-Path -Parent $Log
  if ($logParent) {
    New-Item -ItemType Directory -Force -Path $logParent | Out-Null
  }
  Remove-Item -Force -ErrorAction SilentlyContinue $Log
}

if ($Fresh) {
  Remove-Item -Force -ErrorAction SilentlyContinue $buildCsv, $queryCsv, $summaryCsv
  Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $ResultsDir "*.png")
  Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $TreesDir "*.tree")
}

$datasets = @(
  @{ Name = "random"; Path = $RandomDataset },
  @{ Name = "europa"; Path = $EuropaDataset }
)
$methods = @("nearestx", "str")
$exponents = $MinExponent..$MaxExponent

$buildSteps = $datasets.Count * $methods.Count * $exponents.Count
$querySteps = $datasets.Count * $methods.Count * $Sides.Count
$plotSteps = if ($SkipPlots) { 0 } else { 2 }
$totalSteps = $buildSteps + $querySteps + $plotSteps + 1
$step = 0

Write-Log "Running required build grid: N=2^$MinExponent..2^$MaxExponent"
foreach ($dataset in $datasets) {
  foreach ($method in $methods) {
    foreach ($exp in $exponents) {
      $n = [int64][Math]::Pow(2, $exp)
      $tree = Join-Path $TreesDir ("{0}_{1}_{2}.tree" -f $dataset.Name, $method, $n)
      $step += 1
      $percent = 100.0 * $step / $totalSteps
      Invoke-Step -FilePath $Cli `
        -Arguments @(
          "experiment-build",
          "--dataset", $dataset.Path,
          "--dataset-name", $dataset.Name,
          "--method", $method,
          "--n", "$n",
          "--csv", $buildCsv,
          "--out", $tree
        ) `
        -Activity "Required R-tree experiments" `
        -Status "build $($dataset.Name) $method N=$n" `
        -PercentComplete $percent
    }
  }
}

Write-Log "Running required query grid: N=2^$MaxExponent, queries=$Queries"
$queryN = [int64][Math]::Pow(2, $MaxExponent)
foreach ($dataset in $datasets) {
  foreach ($method in $methods) {
    $tree = Join-Path $TreesDir ("{0}_{1}_{2}.tree" -f $dataset.Name, $method, $queryN)
    if (-not (Test-Path $tree)) {
      throw "Missing query tree: $tree"
    }
    foreach ($side in $Sides) {
      $step += 1
      $percent = 100.0 * $step / $totalSteps
      Invoke-Step -FilePath $Cli `
        -Arguments @(
          "experiment-query",
          "--dataset", $dataset.Path,
          "--tree", $tree,
          "--dataset-name", $dataset.Name,
          "--method", $method,
          "--queries", "$Queries",
          "--side", "$side",
          "--csv", $queryCsv,
          "--summary", $summaryCsv
        ) `
        -Activity "Required R-tree experiments" `
        -Status "query $($dataset.Name) $method side=$side" `
        -PercentComplete $percent
    }
  }
}

if (-not $SkipPlots) {
  $step += 1
  Invoke-Step -FilePath "uv" `
    -Arguments @(
      "run", "--python", "3.12", "--with", "matplotlib", "python",
      "scripts\plot_build_times.py",
      "--csv", $buildCsv,
      "--out-dir", $ResultsDir
    ) `
    -Activity "Required R-tree experiments" `
    -Status "plot build times" `
    -PercentComplete (100.0 * $step / $totalSteps)

  $step += 1
  Invoke-Step -FilePath "uv" `
    -Arguments @(
      "run", "--python", "3.12", "--with", "matplotlib", "python",
      "scripts\plot_query_summary.py",
      "--csv", $summaryCsv,
      "--out-dir", $ResultsDir
    ) `
    -Activity "Required R-tree experiments" `
    -Status "plot query summaries" `
    -PercentComplete (100.0 * $step / $totalSteps)
}

$step += 1
Write-Progress -Activity "Required R-tree experiments" `
  -Status "collecting system info" `
  -PercentComplete (100.0 * $step / $totalSteps)
& powershell -NoProfile -ExecutionPolicy Bypass -File "scripts\system_info.ps1" $systemInfo
Write-Log "DONE system info"

Write-Progress -Activity "Required R-tree experiments" -Completed
Write-Log "All required experiments completed"
