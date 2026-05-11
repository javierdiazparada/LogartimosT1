#!/usr/bin/env sh
set -eu

out="${1:-results/system_info.txt}"
out_dir=$(dirname "$out")
if [ "$out_dir" != "." ]; then
  mkdir -p "$out_dir"
fi

{
  echo "=== date ==="
  date || true
  echo

  echo "=== uname ==="
  uname -a || true
  echo

  echo "=== cpu ==="
  if command -v lscpu >/dev/null 2>&1; then
    lscpu || true
  elif command -v sysctl >/dev/null 2>&1; then
    sysctl -a 2>/dev/null | grep -Ei 'cpu|cache|hw.mem' || true
  elif command -v powershell.exe >/dev/null 2>&1; then
    powershell.exe -NoProfile -Command "Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors,L2CacheSize,L3CacheSize | Format-List" || true
  else
    echo "No CPU information command found."
  fi
  echo

  echo "=== memory ==="
  if command -v free >/dev/null 2>&1; then
    free -h || true
  elif command -v powershell.exe >/dev/null 2>&1; then
    powershell.exe -NoProfile -Command "Get-CimInstance Win32_ComputerSystem | Select-Object Manufacturer,Model,TotalPhysicalMemory | Format-List" || true
  else
    echo "No memory information command found."
  fi
  echo

  echo "=== compiler ==="
  if command -v c++ >/dev/null 2>&1; then
    c++ --version || true
  fi
  if command -v cl >/dev/null 2>&1; then
    cl 2>&1 | head -n 2 || true
  fi
  if command -v cmake >/dev/null 2>&1; then
    cmake --version || true
  fi
} > "$out"

echo "wrote $out"
