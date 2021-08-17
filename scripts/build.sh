#!/usr/bin/env sh

CC="gcc"
FLAGS="-Wall -Wextra -Werror -pedantic-errors -Wfatal-errors"
LIBS=""
SRCDIR="./src"
BUILDDIR="./build"
INSTALLDIR="/usr/local/bin"
BIN="a80"

case "$1" in
	"--debug")
		FLAGS="${FLAGS} -g -fsanitize=address,undefined"
		;;
	"--clean")
		rm -rv $BUILDDIR
		exit $?
		;;
	"--uninstall")
		rm -v $INSTALLDIR/$BIN
		exit $?
		;;
	"--install")
		install -v -m755 ./$BIN $INSTALLDIR/$BIN
		exit $?
		;;
esac

if [ -d $BUILDDIR ]; then
	rm -rv $BUILDDIR/*
else
	mkdir -v $BUILDDIR
fi

for f in $SRCDIR/*.c; do
	$CC $FLAGS -c -o $BUILDDIR/$(basename ${f%.*}).o $f
	echo "compiled '${f%.*}'"
done
$CC $FLAGS -o $BIN $BUILDDIR/*.o $LIBS

