#!/bin/bash
# variables
prefix=/Applications/Vindo.app/Contents/Resources/usr
export PATH="/usr/bin:/bin:$prefix/bin"
export DYLD_FALLBACK_LIBRARY_PATH="$prefix/lib"
export LD_LIBRARY_PATH="$prefix/lib"
export CFLAGS="-m32"
export CXXFLAGS="-m32"

function bold() {
	tput smso
	echo $@
	tput rmso
}	
function run() {
	declare -i status

	bold "$ $@"
	if ! $@; then
		bold "COMMAND FAILED"
		bold ABORTING
		exit 1
	fi
}
function download() {
	run curl -LO# $1
	run tar -xf `basename $1`
}
function configure() {
	run ./configure --prefix=$prefix --disable-dependency-tracking $@
}
function make_install() {
	run make
	run make install
}

# setup
if [ -e $prefix ]; then
	echo backing up existing usr...
	run mv $prefix ${prefix}.bak
fi

#pkg-config
download http://pkgconfig.freedesktop.org/releases/pkg-config-0.28.tar.gz
run cd pkg-config*
configure --with-internal-glib --with-pc-path="$prefix/lib/pkgconfig"
make_install
run cd ..
run rm -rf pkg-config-0.28*

#libpng
download http://download.sourceforge.net/libpng/libpng-1.6.17.tar.gz
run cd libpng*
configure
make_install
run cd ..
run rm -rf libpng*

#freetype
download https://downloads.sf.net/project/freetype/freetype2/2.6/freetype-2.6.tar.bz2
run cd freetype*
configure --without-harfbuzz
make_install
run cd ..
run rm -rf freetype*

#jpeg
download http://www.ijg.org/files/jpegsrc.v8d.tar.gz
run cd jpeg*
configure
make_install
run cd ..
run rm -rf jpeg*

#libtiff
download http://download.osgeo.org/libtiff/tiff-4.0.4.tar.gz
run cd tiff*
configure --without-x --disable-lzma --with-jpeg-include-dir=$prefix/include --with-jpeg-lib-dir=$prefix/lib
make_install
run cd ..
run rm -rf tiff*

#Now is the right time to do this. I'm not telling you why.
#actually, because all the things after this need it. except little cms. but it's fine.
#maybe should have done this at the beginning?
export CFLAGS="-m32 -L$prefix/lib"

#libicns
download https://downloads.sourceforge.net/project/icns/libicns-0.8.1.tar.gz
run cd libicns*
configure --disable-debug
make_install
run cd ..
run rm -rf libicns*

#little-cms2
download https://downloads.sourceforge.net/project/lcms/lcms/1.19/lcms-1.19.tar.gz
run cd lcms*
configure --disable-debug
make_install
run cd ..
rm -rf lcms*

# and finally, wine!

configure --without-x --without-gettext
make_install

# copy the share folder
run cp -r share $prefix

# cleanup
echo copying usr to current directory...
run rm -rf usr
run mv $prefix usr
if [ -d ${prefix}.bak ]; then
	echo restoring usr backup...
	run mv ${prefix}.bak $prefix
fi
