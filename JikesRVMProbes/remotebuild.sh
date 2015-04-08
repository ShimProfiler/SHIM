#!/bin/bash
#usage ./remotebuild.sh rat.moma /home/yangxi/probes "make OPTIONS=-m32"
#sync the directory
echo "syncing direcoty from `pwd` to $1"
echo "rsync -avz ./ $1:$2"
rsync -avz ./ $1:$2
echo "exec $2 in the remote directory $1"
echo "ssh $1 bash -c \"cd $2; $3\""
ssh $1 bash -c "'cd $2; $3'"

