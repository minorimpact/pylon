#!/bin/bash

set -x

while getopts "hn" cliopts
do
    case "$cliopts" in
    h)  echo $USAGE;
        exit 0;;
    n) REPO="norepo";;
    \?) echo $USAGE;
        exit 1;;
    esac
done

#The build script will write the build version info into some files in the BUILD directory
# prior to building according to values given in the .spec file.
#Expected markup in MAN_FILE to correctly perform substitution:
# MAN_DATE_STRING
# MAN_VERSION_STRING
MAN_FILE="doc/pylon.8"
#Expected format in PYLON_HEADER_FILE to correctly perform the substitution:
# where x is any character:
# #define VERSION "xxxxxx"
PYLON_HEADER_FILE='src/pylon.h'

RELEASE_DIR="/admin/yum-repo/opstools";
BASENAME=$(basename $0)
USAGE="Usage: $(basename $0) [-h]"
_DIRNAME=$(dirname $0)
if [ -d $_DIRNAME ]; then
    cd $_DIRNAME
    DIRNAME=$PWD
    cd $OLDPWD
fi

NAME=`/bin/grep Name $DIRNAME/*.spec | /bin/cut -d' ' -f2`;
VERSION=`/bin/grep Version $DIRNAME/$NAME.spec | /bin/cut -d' ' -f2`;
RELEASE=`/bin/grep Release $DIRNAME/$NAME.spec | /bin/cut -d' ' -f2`;
if [ "$VERSION" = '' -o "$RELEASE" = '' ];
then
    echo "Cannot determine version or release number."
    exit 1
fi

#we don't know rpm's dist value for this box
#Strip the %{dist} tag from the release value found in spec file
RELEASE=${RELEASE%\%*}
#try to get the %{dist} value that rpmbuild will use
DIST=`rpmbuild --showrc |grep ":[[:space:]]dist\b" | awk '{print $3}'`

mkdir -p $HOME/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

PACKAGE_NAME="${NAME}-${VERSION}"
BUILD_DIR="$HOME/rpmbuild/BUILD"
BASE_DIR="$BUILD_DIR/$PACKAGE_NAME"
ROOT_DIR="$BASE_DIR"
SOURCE_DIR="$HOME/rpmbuild/SOURCES"

[ -f $SOURCE_DIR/$PACKAGE_NAME.tar.gz ] && rm -f $SOURCE_DIR/$PACKAGE_NAME.tar.gz

[ -d $ROOT_DIR ] && rm -rf $ROOT_DIR

mkdir -p $ROOT_DIR

cp -a $DIRNAME/../* $ROOT_DIR

cd $BUILD_DIR

#keep man page up to date with latest build version info
MAN_DATE_STRING=`date +"%a %b %d %Y"`
MAN_VERSION_STRING="${VERSION}-${RELEASE}"
if [ -f ${BASE_DIR}/${MAN_FILE} ];
then
   sed -i -e "s/MAN_DATE_STRING/${MAN_DATE_STRING}/g" ${BASE_DIR}/${MAN_FILE};
   sed -i -e "s/MAN_VERSION_STRING/${MAN_VERSION_STRING}/g" ${BASE_DIR}/${MAN_FILE};
fi

#update internal documentation with build version
if [ -f ${BASE_DIR}/${PYLON_HEADER_FILE} ];
then
    sed -i -e "s/\(#define VERSION \"\)\(.\+\)\(\"\)/\1${MAN_VERSION_STRING}\3/" ${BASE_DIR}/${PYLON_HEADER_FILE};
fi

tar -c -v -z --exclude='.git' --exclude='build' -f ${PACKAGE_NAME}.tar.gz $PACKAGE_NAME/
cp ${PACKAGE_NAME}.tar.gz $SOURCE_DIR/

rm -rf $BASE_DIR

#do not specify target or dist here, we will try to derive that ourselves
rpmbuild -ba --define "_binary_filedigest_algorithm  1"  --define "_binary_payload 1" $DIRNAME/$NAME.spec

if [ "$REPO" != "norepo" ];
then
    #we don't know the arch value rpmbuild will use for this box
    ARCH_DIR=`arch`;
    cd $HOME
    RPM="${PACKAGE_NAME}-${RELEASE}${DIST}"
    cp rpmbuild/RPMS/${ARCH_DIR}/${RPM}.${ARCH_DIR}.rpm $RELEASE_DIR/
    createrepo -s sha $RELEASE_DIR
fi