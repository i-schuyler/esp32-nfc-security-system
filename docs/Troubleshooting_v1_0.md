ARTIFACT — troubleshooting.md (Troubleshooting v1.0)

# Troubleshooting (V1)

This doc defines how the system should *explain itself* when something goes wrong.

## 1) UI Health Indicators (Minimum)

- SD: OK / Missing / Error
- RTC: OK / Missing / Time Invalid
- NFC Reader: OK / Disconnected
- Sensors: OK / No data / Disconnected
- Network: STA/AP + IP + RSSI (if STA)

## 2) Common Faults + Expected Messages

- SD missing: “SD missing — logging in fallback buffer”
- RTC missing: “RTC missing — timestamps may be wrong”
- NFC disconnected: “NFC reader disconnected — security boundary degraded”
- Sensor disconnected: “Motion sensor not responding”
- Too many invalid scans: “NFC lockout active (…s)”

## 3) Recovery Steps

Each fault must list:
- what it means
- what still works
- what is degraded
- how to recover

(Exact wording is pending UI copy approval.)
