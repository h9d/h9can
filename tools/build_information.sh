#!/bin/bash

cat << EOF
/* --------------------------------------------------------------------------- *
 * This file is by build_information target from Makefile                      *
 * during the make of your project. If you need to make changes                *
 * edit the build_information.sh file NOT THIS FILE.                           *
 * --------------------------------------------------------------------------- */

#ifndef _BUILD_INFORMATION_GEN_H_
#define _BUILD_INFORMATION_GEN_H_

EOF

echo "#define BUILD_TIME \"$(date +'%Y-%m-%dT%H:%M:%S%z')\""
echo "#define GIT_SHA \"$(git rev-parse --short HEAD)\""
echo "#define GIT_DESCRIBE \"$(git describe --dirty)\""

git describe --exact-match --tags $(git log -n1 --pretty='%h') > /dev/null 2>&1
ret=$?
[[ $ret -ne 0 ]] || [[ -n $(git status -s) ]] && echo "#define DIRTY_VERSION_BUILD"
[[ -n $(git status -s) ]] && echo "#define DIRTY_WORKING_TREE_BUILD"

cat << EOF

#endif /* _BUILD_INFORMATION_GEN_H_ */

EOF
