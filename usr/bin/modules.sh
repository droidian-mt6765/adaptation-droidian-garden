#!/bin/bash

mkdir -p /lib/modules/$(uname -r)
cp /vendor/lib/modules/* /lib/modules/$(uname -r)
depmod

modules=(
wmt_drv
wmt_chrdev_wifi
wlan_drv_gen4m
gps_drv
bt_drv
fmradio_drv
fpsgo
met
)

for name in "${modules[@]}"; do
    modprobe -f "$name"
done

echo 1 > /dev/wmtWifi

exit 0
