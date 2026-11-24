# Level Detector – Ideas

Not planned yet. Pick up later.

## Worth doing
1. **Tank level %**: set empty/full distances once; show a percentage on the OLED and send it to a new `level` feed on Adafruit IO.
2. **Smooth distance**: take 5 readings and use the median to reject ultrasonic spikes.
3. **Temperature correction**: use the DHT11 temperature to correct the speed of sound (`331.3 + 0.606 * T` m/s) instead of a fixed 343 m/s.
4. **Connection status**: wait for Adafruit IO with a timeout in `setup()`, and show a Wi-Fi/IO status icon on the OLED.
5. **Move secrets out of the code**: put Wi-Fi and Adafruit IO keys in a `secrets.h` that stays out of git/backups.

## Nice to have
- Alerts: buzzer or LED for low/high level, plus an Adafruit IO action that sends an email.
- OTA updates: upload new code over Wi-Fi instead of USB.
- Deep sleep: only if running on a battery.
