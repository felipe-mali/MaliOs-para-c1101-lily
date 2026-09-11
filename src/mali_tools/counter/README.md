# MaliOS Counter Suite

Receive, detect and analyze environmental activity. Counter actions do not call transmit, jammer,
deauthentication, tag-writing or replay functions. NFC is **reader polling**, so it energizes the field
and requests ISO14443A identifiers; it is not a passive NFC sniffer.

## Entry points and controls

- Mali Tools > Counter Suite; the existing Mali Counter menu also opens the suite.
- Wi-Fi, BLE, Sub-GHz, NFC/RFID and Infrared each have their Counter entry.
- 2.4G Counter appears only after an SPI nRF24 answers the hardware probe. GPS/W5500 conflicts prevent probing.
- Dashboard lists the last collected status and its age, and opens each module. It is a snapshot, not simultaneous monitoring.
- Global Monitor gives each available module an eight-second slice, releases it, and proceeds to the next.
  An unavailable module gets a short status screen instead of aborting the entire session.
- Encoder turns change detail pages; Back exits. On IR, OK saves the received sample; on RF, OK resets Peak Hold.
- Text wraps onto additional pages according to display width. Layout uses runtime screen dimensions, including the
  T-Embed ST7789 in its normal landscape orientation (320 x 170).

## Existing facilities reused

| Function | Existing implementation |
| --- | --- |
| Wi-Fi passive scan | WiFi.scanNetworks async/passive, as in MaliCounter.cpp |
| Wi-Fi/BLE memory checks | core/radio_mem.h |
| BLE reception | NimBLEDevice/NimBLEScan, passive callbacks with duplicate filtering disabled |
| CC1101 and RF switch | modules/rf/rf_utils: initRfModule("rx"), setMHZ, deinitRfModule |
| nRF24 / shared SPI | modules/NRF24/nrf_common: nrf_start, NRFradio, bus HAL |
| NFC transport | modules/rfid/PN532; new readUidOnly() avoids read(), authentication and block reads |
| IR decoding | IRrecv/IRutils used by modules/ir/ir_read |
| SD | core/sd_functions and the existing SD mount |
| Display and encoder | drawMainBorderWithTitle, loopOptions and check(EscPress/NextPress/PrevPress/SelPress) |
| LEDs | MaliLedStateGuard and the existing status effect engine, with temporary purple signal states |

No implementation was added to main.cpp. The earlier MaliCounter implementation remains available to preserve
its boot-reset diagnostics; the suite is the new menu destination.

## Metrics and limits

### Wi-Fi

The scanner observes AP BSSIDs, including hidden SSIDs. AP count is not a count of unique network names.
The 14 graph bins show AP counts on channels 1..14; the busiest channel is the one with the most observed APs.
Packet, management, deauthentication/disassociation and transmitter MAC counters come from the promiscuous callback.
Control frames contribute to packet counts, but are not treated as transmitter identities.
The MAC table is capped at 64; its count is an observed lower bound when full. These identities can be randomized.

Scans run passively about every four seconds. RSSI mean/peak and AP appearance/disappearance compare scan results.
AP churn comparison is bounded to 64 BSSIDs and is not used for alarms when that table is full.
Sampling and channel hopping miss traffic; none of these numbers describes all traffic in the environment.
Wi-Fi Counter requires Wi-Fi off before entry so it does not replace an existing connection, AP or capture session.

Heuristics: >100 packets/s BUSY; >500 HIGH TRAFFIC; >3x an established moving baseline (>20 packets/s)
SUSPICIOUS; >=8 AP appearances/disappearances per comparison also SUSPICIOUS for five seconds.
Any deauth/disassoc in the latest one-second window takes priority as DEAUTH DETECTED. Such frames can be legitimate.

### BLE

Passive scan only; no connections or scan requests. Counts advertisements observed by this receiver, not all transmitted
advertisements. A 64-entry address table expires visibility after ten seconds and reclaims slots after fifteen seconds.
New/gone counts compare visible identities, not just the difference in totals. Overflow advertisements are displayed.
The first advertised service UUID per identity is retained, with three distinct UUID samples shown on detail pages.
Manufacturer and name describe the strongest visible advertiser; company IDs are shown numerically when not in the
small Apple/Samsung/Microsoft map. Advertised names/company IDs are untrusted declarations.

Heuristics: a device >50 adv/s is highlighted in the high-rate count; total >150 adv/s is HIGH ADVERTISEMENT RATE;
>300 adv/s with >20 visible identities is BLE FLOOD SUSPECTED; >10 new identities after the initial window is DEVICE SPIKE.
A Global Monitor BLE slice is shorter than the disappearance timeout: use standalone BLE Counter to observe disappearance.

### RF / CC1101

Spectrum Monitor samples 41 bins, 10 kHz apart, +/-200 kHz around a selected center. Presets cover 315, 433.92, 868.35
and 915 MHz; a custom center is supported. The receive bandwidth is about 58 kHz, so the 10 kHz grid does not imply
10 kHz resolution or an exact carrier-frequency measurement. Peak Hold and the graph retain the session maximum;
OK clears them. Mean RSSI averages samples. Most active frequency uses threshold-positive sample counts.
RF Watch accepts a frequency in 300..348, 387..464 or 779..928 MHz and samples that frequency without retuning between reads.

Signal threshold: -75 dBm; strong: -45 dBm; continuous: >3 seconds above threshold. More than 30 rising bin events in a
one-second window is UNUSUAL ACTIVITY for three seconds. An event means a threshold crossing at a sampled bin, not a
unique transmitter or independent transmission. Spectrum duration is approximate across revisits and can miss gaps.
Logs throttle ordinary RF detections to two per second and warnings to one per two seconds; displayed counters retain
all observed bin events. Signal history uses the shared event log. There is no automatic retransmission.

### NFC

PN532 over configured I2C or SPI, ISO14443A UID polling only. Other configured NFC drivers are reported unavailable.
Scans count polling attempts; Tags counts presentations; Unique holds up to 64 exact UIDs. A stationary tag does not
increment on every poll; absence must last >750 ms before the same UID counts as another presentation.
Polling uses a 50 ms transport timeout (each readiness wait); the response's header and UID length are checked before copying.
RAPID NFC ACTIVITY means >=5 presentations in a three-second window. UID identifies a tag, not its owner.

### IR

Receive-only decoding: protocol, address, command, intervals, repeat count and a 32-second activity graph.
>20 decodes/s yields HIGH IR ACTIVITY. Library repeats or equal protocol/value within 300 ms count as repeating.
A demodulated IR receiver cannot measure carrier frequency; the screen and saved file explicitly say it is unknown.
Save Sample writes bounded raw timings in microseconds and parsed metadata to `/MaliCounter/ir_<uptime_ms>.txt`.
This is an analysis file, not a replay preset with an invented carrier. Overflow is recorded as truncation.

### 2.4 GHz

An attached nRF24 samples channels 0..79 (2400..2479 MHz) with auto-ack disabled. RPD hits and sampled occupancy describe
energy above the chip threshold; they are not packet or device counts. The graph is cumulative within the slice/session.

## Events, LEDs and lifecycle

The shared RAM ring retains 64 events (128 bytes per formatted line), and a total event counter.
Timestamps are explicitly `up HH:MM:SS` since boot, so no clock sync is required.
When SD is mounted, new entries append to `/MaliCounter/events.log` approximately once per second and on exit/switch.
View Log shows the current RAM ring; Export Log flushes it to the SD file; Clear Log clears RAM and removes the file
on the currently mounted SD. Older RAM events can be overwritten while SD is unavailable; historical SD contents are
not reloaded into RAM. Write failures are reported by Export Log; it does not report success for a failed flush.

Callbacks only update bounded storage under a critical section. Display, strings, LED events and SD writes run from the
UI loop, not the receive callback. Each monitor has begin/tick/end; callback storage survives teardown. Wi-Fi and BLE
owned by another tool are left untouched. CC1101 goes to idle, nRF24 powers down with CE low, IR receiver is disabled,
and NFC powers down and releases the I2C HAL on exit. LEDs restore their prior state through the existing guard.
Idle uses dim wine; monitoring uses stable wine; detected RF/NFC uses a short purple pulse, strong RF a brighter pulse,
and warnings use the existing temporary red effect.

Global Monitor counters restart for each slice. The dashboard and log survive tool navigation but not reboot.
Short synchronous hardware operations and SD writes remain; the UI loop yields and checks Back between operations.
Measurements and timing still require validation on the actual connected hardware.

## Validation

Build: `pio run -e lilygo-t-embed-cc1101`.
Compile-time regression checks in `tests/counter/metrics_test.cpp` cover alarm precedence and boundaries, wide rate
arithmetic, supported RF bands, unsigned timer rollover, truncated/malformed NFC replies and 4/7/10-byte UIDs.
They can be checked with the installed ESP32-S3 compiler using `-std=c++17 -fsyntax-only tests/counter/metrics_test.cpp`.

Hardware acceptance still required: leave every Counter during acquisition; cycle Global Monitor repeatedly with/without
SD and nRF24; verify no auto-ack/transmit from radio monitors; present stationary/repeated NFC tags; receive known IR
protocols and inspect saved timings; compare RF levels with a known source; exercise full/unavailable SD and existing
Wi-Fi/BLE sessions. Firmware has not been flashed by this change.
