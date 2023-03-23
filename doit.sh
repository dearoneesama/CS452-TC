#!/bin/bash

DOMAIN=XXX@linux.student.cs.uwaterloo.ca
KEY=YYY
TARGET=CS017540

if [ $TARGET == "CS017541" ]; then
    IS_TRACK_A=0
    NO_CTS=0
    DEBUG_PI=0
elif [ $TARGET == "CS017540" ]; then
    IS_TRACK_A=1
    NO_CTS=0
    DEBUG_PI=0
else
    IS_TRACK_A=0
    NO_CTS=0
    DEBUG_PI=1
fi

JIXI_LOCAL=1 IS_TRACK_A=$IS_TRACK_A NO_CTS=$NO_CTS DEBUG_PI=$DEBUG_PI make

scp -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -i $KEY build/kernel8.img $DOMAIN:~/cs452
ssh -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -i $KEY $DOMAIN "/u/cs452/public/tools/setupTFTP.sh $TARGET cs452/kernel8.img"
