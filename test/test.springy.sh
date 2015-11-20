#!/bin/bash

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
sudo umount "$DIR/test.dir1"
sudo umount "$DIR/test.dir2"

sudo mount -t tmpfs -o size=100M none "$DIR/test.dir1"
sudo mount -t tmpfs -o size=200M none "$DIR/test.dir2"

$DIR/../springy -f -s  "$DIR/test.dir1,$DIR/test.dir2" mounted
pushd "$DIR/ntfs-3g-pjd-fstest-20151117"
#sudo prove -r ../mounted
popd

