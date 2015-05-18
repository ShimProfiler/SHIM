#!/bin/bash
if [ -a /dev/ppid_map ]
  then
    echo "/dev/ppid_map is exist already."
    exit
fi

deviceID=`cat /proc/devices  | grep ppid | grep -o [0-9]*`
echo $deviceID
mknod /dev/ppid_map c $deviceID 0
ls -lh /dev/ppid_map
