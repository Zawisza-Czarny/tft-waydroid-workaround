#!/system/bin/sh
# Include TFT in Waydroid's mouse-to-touch list before apps create ViewRootImpl.
# Keep the Wayland cursor visible over game subsurfaces.
PKG=com.riotgames.league.teamfighttactics
setprop persist.waydroid.cursor_on_subsurface true
APPS=$(getprop persist.waydroid.fake_touch)
case ",$APPS," in
  *",$PKG,"*) exit 0 ;;
esac
if [ -n "$APPS" ]; then APPS=$APPS,$PKG; else APPS=$PKG; fi
[ "${#APPS}" -le 91 ] && setprop persist.waydroid.fake_touch "$APPS"
