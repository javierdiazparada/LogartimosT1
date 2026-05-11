param(
  [string]$Out = "results/system_info.txt"
)

$parent = Split-Path -Parent $Out
if ($parent) {
  New-Item -ItemType Directory -Force -Path $parent | Out-Null
}

$lines = New-Object System.Collections.Generic.List[string]

function Add-Section {
  param([string]$Name)
  $lines.Add("=== $Name ===")
}

function Add-Command {
  param([scriptblock]$Command)
  try {
    $output = & $Command 2>&1 | Out-String
    $lines.Add($output.TrimEnd())
  } catch {
    $lines.Add($_.Exception.Message)
  }
  $lines.Add("")
}

Add-Section "date"
$lines.Add((Get-Date).ToString("o"))
$lines.Add("")

Add-Section "computer"
Add-Command {
  Get-CimInstance Win32_ComputerSystem |
    Select-Object Manufacturer, Model, TotalPhysicalMemory |
    Format-List
}

Add-Section "cpu"
Add-Command {
  Get-CimInstance Win32_Processor |
    Select-Object Name, NumberOfCores, NumberOfLogicalProcessors,
      L2CacheSize, L3CacheSize |
    Format-List
}

Add-Section "os"
Add-Command {
  Get-CimInstance Win32_OperatingSystem |
    Select-Object Caption, Version, OSArchitecture |
    Format-List
}

Add-Section "cmake"
Add-Command { cmake --version }

Set-Content -Path $Out -Value $lines -Encoding UTF8
Write-Output "wrote $Out"
