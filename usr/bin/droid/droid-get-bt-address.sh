#!/bin/sh

cd /var/lib/bluetooth
find . -mindepth 1 -maxdepth 1 -type d | cut -c3- | awk -F: -vOFS=: '{print $6,$5,$4,$3,$2,$1}' > /var/lib/bluetooth/board-address
