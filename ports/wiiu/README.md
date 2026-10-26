# Wii U port (experimental, branch `port/wiiu`)

A separate effort, never merged into `main`. Built with devkitPro (devkitPPC + wut) and run on a
console with the Aroma environment.

## Tools

- `hello/`: a smoke test. It logs over UDP (port 4405), writes `rac1-native/hello.txt` to the SD
  card, shows a few lines on the TV and the GamePad, and goes back to the Wii U Menu by itself after
  `argv[1]` seconds (5 by default; 0 waits for HOME).

  ```bash
  /opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake -S ports/wiiu/hello -B build-wiiu-hello
  cmake --build build-wiiu-hello -j 3
  ```

- `tools/wiiu-run.sh`: sends an `.rpx` / `.wuhb` with `wiiload`, records the app's UDP log and waits
  for its exit line. It sends only when the console's wiiload port answers, and never twice.

  ```bash
  WIIU_IP=<console address> ports/wiiu/tools/wiiu-run.sh build-wiiu-hello/rac1_wiiu_hello.rpx 5
  ```

## Console notes

- Aroma beta-27 or newer, with the wiiload and FTPiiU plugins; the LoggingModule for the system's
  own log lines on UDP 4405.
- The SD card matters: a slow or failing card makes every launch stall.
