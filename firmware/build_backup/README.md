# Backup tools

| File | What | When |
|---|---|---|
| `e32_config.py` | Read/set the ground E32 LoRa-UART module from a Mac (replaces Windows "RF Setting") | Ground station setup |
| `revC_no_heater.patch` | The 24 Oct source change that switched the heater off. **Already applied** in fw 1.2; kept for history | — |

There are no prebuilt firmware files here on purpose: build from source so the code can be debugged. See `docs/Software_Setup.md` (sections 4–6) for building, flashing and debugging.

The old 24 Oct binaries (fw 1.0 heater-on, 1.1 no-heater) were removed. They don't know the new sensors, and the heater-on one drives PC6 as a PWM output, which now fights the DHT-11.
