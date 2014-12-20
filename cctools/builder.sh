#!/bin/bash

source ./builder.conf

if [ -z ${1} ]; then
	echo -e "Usage:\n\t${0} { do_build | do_install <destdir> | do_clean }"
	exit 1
fi

do_build() {
	echo "Building ${PROJECT_NAME}..."

	if [ -x ./autogen.sh ]; then
		./autogen.sh
	fi

	if [ ! -x ./configure ]; then
		echo "${0}: error: autotools failure or invalid project!"
		return -1
	else
		./configure ${AUTOCONF_OPTIONS}
	fi

	make ${MAKE_OPTIONS}

	if [ ${?} -ne 0 ]; then
		echo "Building ${PROJECT_NAME} failed!"
	else
		echo "${PROJECT_NAME} successfully built."
	fi

	return ${?}
}

do_install() {
	local destdir=${1}

	echo "destdir=${destdir}"

	if [ ! -f ./Makefile ]; then
		echo "${0}: error: run do_build before using this option!"
		return -1
	else
		make ${MAKE_OPTIONS} DESTDIR=${destdir} install
	fi
}

do_clean() {
	if [ -f ./Makefile ]; then
		make distclean
	fi
}

${1} ${2}

exit ${?}
