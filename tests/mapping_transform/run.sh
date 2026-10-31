#!/bin/sh
cd /home/iceisfun/test_gdi32_gdifull; rm -f build/mapping_transform.exe; make build/mapping_transform.exe 2>&1 | grep -E "error|warning" 
WP=/tmp/claude-1000/-home-iceisfun-test-user32/0dbca224-e0c6-4aac-8035-d3516ec0a47c/scratchpad
DISPLAY=:1 WINEPREFIX=$WP/wp_g_mt WINEARCH=win64 WINEDEBUG=-all timeout 300 /home/iceisfun/work/wine/proton-ge-custom/build/dist/files/bin/wine build/mapping_transform.exe "$@" 2>&1 | grep -vE "^(RUN|OK)|NOTE|Maximum|wineserver"
