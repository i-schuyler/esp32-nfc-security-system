# Platform alternative: ESP-IDF (note)

ESP-IDF is a strong choice when the project requires:
- deeper control of Wi‑Fi/SoftAP/STA behavior and memory usage
- tighter security primitives and TLS handling
- long-term product hardening and performance constraints

Trade-off (V1):
- More scaffolding and slower iteration during hardware bring-up
- More work to integrate third-party Arduino-focused libraries (PN532 stacks vary)

V1 recommendation remains PlatformIO + Arduino unless an IDF-only requirement emerges.
