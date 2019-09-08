#!/bin/bash

ACTION='\033[1;90m'
FINISHED='\033[1;96m'
READY='\033[1;92m'
NOCOLOR='\033[0m' # No Color
ERROR='\033[0;31m'

# Check for git updates
echo
echo -e ${ACTION}Checking Git repo
echo -e =======================${NOCOLOR}

BRANCH=$(sudo -u git rev-parse --abbrev-ref HEAD)
if [ "$BRANCH" != "master" ] ; then
    echo -e ${ERROR}Not on master. Aborting. ${NOCOLOR}
    echo
    exit 0
fi

git fetch
HEADHASH=$(sudo -u git rev-parse HEAD)
UPSTREAMHASH=$(sudo -u git rev-parse master@{upstream})

if [ "$HEADHASH" != "$UPSTREAMHASH" ] ; then
    echo -e ${ACTION}Not up to date with origin. Pull from remote${NOCOLOR}
else
    echo -e ${FINISHED}Current branch is up to date with origin/master.${NOCOLOR}
    exit 0
fi

PULLRESULT=$(sudo -u guy git pull)

if [ "$PULLRESULT" = "Already up to date." ] ; then
    echo -e ${FINISHED}Current branch is up to date with origin/master.${NOCOLOR}
    exit 0
fi

echo -e ${ACTION}Building code...${NOCOLOR}
MAKERESULT=$(sudo -u guy make)

if [ "$MAKERESULT" = "make: Nothing to be done for \`all\'." ] ; then
    echo -e ${FINISHED}make - Nothing to build.${NOCOLOR}
    exit 0
fi

if [ $? -ne 0 ] ; then
    echo -e ${ERROR}Build failed. Aborting. ${NOCOLOR}
    exit 0
fi    

echo -e ${ACTION}Build succeeded${NOCOLOR}

while IFS= read -r pid; do
    echo -e ${ACTION}PID of wctl process is: $pid${NOCOLOR}
    kill $pid
done < "wctl.pid"

echo -e ${ACTION}Installing new version...${NOCOLOR}
make install

if [ $? -ne 0 ] ; then
    echo -e ${ERROR}Failed to install new version. Aborting. ${NOCOLOR}
    exit 0
fi    

# Restart the process...
echo -e ${ACTION}Restarting wctl process...${NOCOLOR}
wctl -d -cfg /etc/weatherctl/wctl.cfg
