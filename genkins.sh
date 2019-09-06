#!/bin/bash

# Check for git updates
sudo -u guy git pull

sudo -u guy make

if [ $? -eq 0 ] ; then
    echo "Build succeeded"

    while IFS= read -r pid; do
        echo "PID of wctl process is: $pid"

        kill $pid
    done < "wctl.pid"

    make install

    wctl -d -cfg /etc/weatherctl/wctl.cfg
else
    echo "Build failed"
    exit
fi    
