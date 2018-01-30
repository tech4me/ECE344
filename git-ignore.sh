#!/bin/bash

for i in $(find . -name .svnignore); do
    DIR=$(dirname $i)
    pushd $DIR
    cp .svnignore .gitignore
    popd
done
