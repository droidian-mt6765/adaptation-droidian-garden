#!/bin/bash

mkdir -p /sys/fs/cgroup/schedtune

mount -t cgroup -o schedtune stune /sys/fs/cgroup/schedtune

[ -f /sys/fs/cgroup/schedtune/schedtune.boost ] && echo 20 > /sys/fs/cgroup/schedtune/schedtune.boost
[ -f /sys/fs/cgroup/schedtune/schedtune.prefer_idle ] && echo 1 > /sys/fs/cgroup/schedtune/schedtune.prefer_idle
