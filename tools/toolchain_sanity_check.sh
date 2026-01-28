#!/data/data/com.termux/files/usr/bin/bash
# toolchain_sanity_check.sh
set -euo pipefail

echo "== toolchain sanity check =="
echo "pwd: $(pwd)"
echo

echo "-- git --"
command -v git
git --version
echo

echo "-- gh --"
command -v gh
gh --version
echo

echo "-- platformio (pio) --"
if command -v pio >/dev/null 2>&1; then
  pio --version
else
  echo "pio not found (expected if PlatformIO not installed locally)"
fi
echo

echo "-- python --"
command -v python || true
python --version || true
echo

echo "OK"
# toolchain_sanity_check.sh EOF
