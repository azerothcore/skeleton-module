#!/usr/bin/env bash

MOD_SKELETON_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/" && pwd )"

source $MOD_SKELETON_ROOT"/conf/config.sh.dist"

if [ -f $MOD_SKELETON_ROOT"/conf/config.sh" ]; then
    source $MOD_SKELETON_ROOT"/conf/config.sh"
fi
