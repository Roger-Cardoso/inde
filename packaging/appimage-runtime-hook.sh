#!/usr/bin/env bash

# Enchant and its Hunspell provider are loaded with dlopen(), so the regular
# ELF dependency scan cannot configure their data path for us.
if [[ -n "${APPDIR:-}" ]]; then
    export DICPATH="$APPDIR/usr/share/hunspell${DICPATH:+:$DICPATH}"
    export XDG_DATA_DIRS="$APPDIR/usr/share${XDG_DATA_DIRS:+:$XDG_DATA_DIRS}"
fi

# The GTK plugin intentionally ships no distro-specific IBus/Fcitx module.
# Do not inherit a module name that cannot be resolved inside the AppImage;
# leaving the variable unset lets GTK select its built-in fallback.
case "${GTK_IM_MODULE:-}" in
    ibus|fcitx|fcitx5)
        unset GTK_IM_MODULE
        ;;
esac
