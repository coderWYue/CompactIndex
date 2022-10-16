#!/bin/bash
SVN_REV=`svnversion -n .`
SVN_DIRTY=`svn diff 2>/dev/null | wc -l`
BUILD_ID=`uname -n`"-"`date +%s`
BUILD_TIME=`date +'%F %T'`

test -f ./inc/release.h || touch ./inc/release.h

#(cat release.h | grep SVN_REV | grep $SVN_REV) && \
[[ `cat ./inc/release.h | grep SVN_REV | awk '{printf $3}' | tr -d '"'` == "$SVN_REV" ]] && \
(cat ./inc/release.h | grep SVN_DIRTY | grep $SVN_DIRTY) && exit 0 # Already up-to-date

echo "#define LIBTDMS_SVN_REV  \"$SVN_REV\"" > ./inc/release.h
echo "#define LIBTDMS_SVN_DIRTY \"$SVN_DIRTY\"" >> ./inc/release.h
echo "#define LIBTDMS_BUILD_TIME \"$BUILD_TIME\"" >> ./inc/release.h
echo "#define LIBTDMS_BUILD_ID \"$BUILD_ID\"" >> ./inc/release.h

#touch ./src/release.c # Force recompile of release.c
