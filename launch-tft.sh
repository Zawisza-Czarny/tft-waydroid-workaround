#!/system/bin/sh
set -eu
MODDIR=${0%/*}
case "${1:-}" in ""|--check-only) ;; *) echo "Unknown argument" >&2; exit 2 ;; esac
PKG=com.riotgames.league.teamfighttactics
CORE_SHA=5ef64035ec89bca3d5e33b9f3b6556f98f6c11d11957387d403ae4a4d7cc40ec
MVG_SHA=fd69b7f9c89a446eec756628e50f9657aa81a52608c9cb5b96d140aadd8cb6f6
fail() { echo "TFT: $*" >&2; exit 1; }
[ "$(id -u)" = 0 ] || fail 'Magisk root is required.'
[ "$(getprop ro.dalvik.vm.native.bridge)" = libhoudini.so ] || fail 'HPE14 is not active. Restart Waydroid after installing the module.'
[ "$(sha256sum /system/lib64/libhoudini.so | cut -d ' ' -f 1)" = "$CORE_SHA" ] || fail 'Unsupported Houdini build.'
APK=$(pm path "$PKG" | sed -n 's/^package:\(.*\/base.apk\)$/\1/p' | head -n 1)
[ -n "$APK" ] || fail 'TFT is not installed.'
MVG=${APK%/*}/lib/arm64/libmvg.so
[ -r "$MVG" ] || fail 'Extracted ARM64 libmvg was not found.'
[ "$(sha256sum "$MVG" | cut -d ' ' -f 1)" = "$MVG_SHA" ] || fail 'Different libmvg build. Its offsets require new analysis.'
if [ "${1:-}" = --check-only ]; then
 echo "Configuration and libmvg hash match the tested build."
 exit 0
fi
# Android reads this option when creating ViewRootImpl; set it before launch.
setprop persist.waydroid.cursor_on_subsurface true
TOUCH_APPS=$(getprop persist.waydroid.fake_touch)
case ",$TOUCH_APPS," in
 *",$PKG,"*) ;;
 *)
  if [ -n "$TOUCH_APPS" ]; then TOUCH_APPS=$TOUCH_APPS,$PKG; else TOUCH_APPS=$PKG; fi
  [ "${#TOUCH_APPS}" -le 91 ] || fail 'The fake_touch list exceeds 91 characters; configure it manually.'
  setprop persist.waydroid.fake_touch "$TOUCH_APPS"
 ;;
esac
if pidof "$PKG" >/dev/null; then
 echo 'TFT is already running; bringing its window forward without restarting the process.'
 am start --activity-reorder-to-front -n "$PKG/com.epicgames.unreal.GameActivity"
 exit 0
fi
mkdir -p "$MODDIR/run"
chmod 700 "$MODDIR/run"
mkdir "$MODDIR/run/launch.lock" 2>/dev/null || fail 'Another launch is in progress. After an interrupted launch, remove run/launch.lock from the module.'
OBS_PID=
cleanup() {
 if [ -n "$OBS_PID" ] && kill -0 "$OBS_PID" 2>/dev/null; then
  kill -TERM "$OBS_PID" 2>/dev/null || true
  wait "$OBS_PID" 2>/dev/null || true
 fi
 rmdir "$MODDIR/run/launch.lock" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT TERM HUP
LOG=$MODDIR/run/last-start.log
ZYGOTE=$(pidof zygote64)
[ -n "$ZYGOTE" ] || fail 'zygote64 was not found.'
[ "$(awk '/^TracerPid:/{print $2}' /proc/$ZYGOTE/status)" = 0 ] || fail 'Zygote is already being traced by another tool.'
"$MODDIR/bin/tft-startup-compat" "$ZYGOTE" > "$LOG" 2>&1 &
OBS_PID=$!
READY=0
for i in $(seq 1 50); do
 if head -n 1 "$LOG" | grep -q '^OBSERVER_READY'; then READY=1; break; fi
 kill -0 "$OBS_PID" 2>/dev/null || break
 sleep 0.1
done
[ "$READY" = 1 ] || fail "The startup helper did not become ready. Log: $LOG"
am start -n "$PKG/com.epicgames.unreal.SplashActivity"
RC=0
wait "$OBS_PID" || RC=$?
OBS_PID=
[ "$RC" = 0 ] || fail "The startup helper exited with status $RC. Log: $LOG"
grep -q '^DETACH_AFTER_RESERVATION$' "$LOG" || fail "Memory reservation was not confirmed. Log: $LOG"
grep -q '^CAPTURED_READS=12$' "$LOG" || fail "Twelve PC reads were not confirmed. Log: $LOG"
[ "$(awk '/^TracerPid:/{print $2}' /proc/$ZYGOTE/status)" = 0 ] || fail 'Zygote still has an active tracer.'
echo "TFT started; the helper has detached. Log: $LOG"
