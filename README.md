# TFT on Waydroid — quick setup

> **Warning: account-ban risk**
>
> This is an unofficial, experimental workaround, with no approval from Riot Games. The startup helper modifies data in the game process and masks its tracer status. Riot's [Terms of Service](https://www.riotgames.com/en/terms-of-service-update-2024) prohibit unauthorized programs that interact with its games and circumvention of protective measures. Using this workaround may lead to account suspension or a permanent ban, even if it only runs during startup and provides no gameplay advantage. Its detection rate and ban probability are unknown. Do not use it with an account you are unwilling to lose.
>
> Publishing this project does not establish permission to use it. Riot's rules also prohibit encouraging violations of its terms. This disclaimer does not authorize use or redistribution and does not remove the associated risks. This project is not affiliated with Riot Games.

This experimental workaround was tested with one specific TFT and Waydroid build. The test reached TFT's signed-in main menu; a match was not tested. It may not work with other versions.

## Before you start

You need:

- Waydroid on an x86_64 Linux system, with Magisk root already working.
- TFT installed in Waydroid.
- Git, Python 3, and GCC with static glibc on the Linux host.
- The guide files extracted to a writable directory.

The module replaces Waydroid's ARM translator with HPE-14. Do not install it if you rely on the existing translator for other apps unless you are prepared to switch back.

## 1. Build the Magisk ZIP

From the extracted guide directory, run:

```bash
./build-module.sh
```

On Fedora Atomic, if the tools are in a Toolbox:

```bash
toolbox run ./build-module.sh
```

The script downloads a pinned HPE-14 revision, verifies its binary hash, builds the helper, and creates `tft-waydroid-hpe14.zip` beside the guide. On Fedora, install missing build tools with `sudo dnf install git python3 gcc glibc-static` (or run that inside the Toolbox).

## 2. Install the module

1. Copy `tft-waydroid-hpe14.zip` to Waydroid's Android `Download` folder.
2. In Magisk, choose **Modules → Install from storage** and select the ZIP.
3. Restart the Waydroid session with its full UI open. A computer reboot is not needed.

## 3. Start TFT

From the extracted guide directory, run:

```bash
python3 launch-waydroid-tft.py
```

Run the launcher on the host, where the `waydroid` command is installed. If ADB is only in Fedora Toolbox, the launcher automatically uses `toolbox run adb`. Approve Magisk root access if prompted. The launcher opens Waydroid and runs the startup helper. If TFT is already running, it brings the game forward without stopping it.

Accept the official game update and sign in within TFT yourself.

## Switch back

In Magisk, disable **TFT Waydroid HPE14 compatibility**, then restart the Waydroid session. This restores the translator that was active before the module. A translator already loaded by running processes is replaced only after restart.

## Troubleshooting

- If Magisk reports an incomplete installation, check its install log and the permissions of Magisk's `busybox`; do not assume another system has the same issue as the test machine.
- If the game crashes or fails to reach the menu, keep the logs and disable the module in Magisk. The tested helper log is `/data/adb/modules/tft_hpe14_test/run/last-start.log`.
- The launcher log is `~/.local/state/tft-waydroid/launcher.log` (or `$XDG_STATE_HOME/tft-waydroid/launcher.log`).

<details>
<summary>Technical details and test limits</summary>

Tested versions:

- Waydroid x86_64, LineageOS 20 / Android 13, image `20.0-20260927-GAPPS-waydroid_x86_64`.
- TFT `18.3-5530794`, versionCode `8530794`.
- ARM64 `libmvg.so`, SHA256 `fd69b7f9c89a446eec756628e50f9657aa81a52608c9cb5b96d140aadd8cb6f6`.
- HPE-14 `14.0.0_z.GoogleGame_com1.3`, commit `2f8f088671182e17e67321e098e8411a3972a628` from [supremegamers/vendor_intel_proprietary_houdini](https://github.com/supremegamers/vendor_intel_proprietary_houdini).
- x86_64 `libhoudini.so` SHA256 `5ef64035ec89bca3d5e33b9f3b6556f98f6c11d11957387d403ae4a4d7cc40ec`.

The original NDK Translation on this image did not execute `frsqrte v3.2d,v2.2d`. HPE-14 produced the same result as a native ARM64 phone. HPE reports a host PC through `/proc/.../syscall`, so the translator alone was insufficient.

During startup, the helper briefly traces zygote and its child processes. It substitutes the final PC token in 12 TFT read buffers, masks `TracerPid` digits in the process's own status buffer, and adds `MAP_NORESERVE` to one specific 48 GiB memory reservation. It detaches after that reservation succeeds, has a 30-second limit, and does not remain in the background. Other new processes can be traced and slowed during this brief interval.

For HPE, PC values are substituted in the order of 12 locations previously observed under NDK Translation; they are not read from Houdini guest registers. The helper verifies the expected instructions and `libmvg` hash before writing. It refuses to start with a different `libmvg` build; that version needs new analysis.

After a successful start, the log should include `DETACH_AFTER_RESERVATION`, `DETACH_COMPLETE`, and `CAPTURED_READS=12`. These lines alone do not prove the game works; confirm that TFT reaches the menu and remains running. No match was tested.

One early startup without an active display session caused a separate SurfaceFlinger failure. Starting with `waydroid show-full-ui` active worked.

</details>

## Share or build from source

See [PUBLISHING.md](PUBLISHING.md) for GitHub upload instructions and third-party licensing notes.

The guide archive includes the helper source, launchers, and build scripts. It does not include TFT, account data, logs, or HPE binaries. The build script fetches the pinned HPE revision when run. Remove device or account details from any logs before sharing them.
