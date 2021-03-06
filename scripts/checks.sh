#!/bin/bash
#
# Copyright 2005 Timo Hirvonen <tihirvon@ee.oulu.fi>
#
# This file is licensed under the GPLv2.

msg_checking()
{
	echo -n "checking $@... "
}

msg_result()
{
	echo "$@"
}

msg_error()
{
	echo "$@"
}

# which is totally brain dead on solaris
path_find()
{
	local program full

	argc 1
	program="$1"
	full=$(which "$program" 2> /dev/null)
	[[ $? -ne 0 ]] && return 1
	[[ ${full##/} = $full ]] && return 1
	echo "${full}"
	return 0
}

# @program: program to check
# @name:    name of variable where to store the full program name (optional)
#
# returns 0 on success and 1 on failure
check_program()
{
	local program varname filename

	argc 1 2
	program="$1"
	varname="$2"

	msg_checking "for program ${program}"
	filename=$(path_find "${program}")
	if [[ $? -eq 0 ]]
	then
		msg_result $filename
		[[ $# -eq 2 ]] && set_var $varname $filename
		return 0
	else
		msg_result "no"
		return 1
	fi
}

# @flag: option flag(s) to check
#
# check if $CC supports @flag
check_cc_flag()
{
	argc 1 100
	[[ -z $CC ]] && die "$FUNCNAME: CC not set"
	msg_checking "for CC flag $@"
	if $CC "$@" -S -o /dev/null -x c /dev/null &> /dev/null
	then
		msg_result "yes"
		return 0
	else
		msg_result "no"
		return 1
	fi
}

# @flag: option flag(s) to check
#
# check if $CXX supports @flag
check_cxx_flag()
{
	argc 1 100
	[[ -z $CXX ]] && die "$FUNCNAME: CXX not set"
	msg_checking "for CXX flag $@"
	if $CXX "$@" -S -o /dev/null -x c /dev/null &> /dev/null
	then
		msg_result "yes"
		return 0
	else
		msg_result "no"
		return 1
	fi
}

# c compiler
check_cc()
{
	var_default CC gcc
	var_default LD $CC
	var_default CFLAGS "-O2 -Wall -g"
	var_default LDFLAGS ""
	var_default SOFLAGS "-fPIC"
	if check_program $CC
	then
		makefile_env_vars CC LD CFLAGS LDFLAGS SOFLAGS
		if check_cc_flag -MT /dev/null -MD -MP -MF /dev/null
		then
			makefile_var CC_GENERATE_DEPS y
		else
			makefile_var CC_GENERATE_DEPS n
		fi
		return 0
	fi
	return 1
}

# c++ compiler
check_cxx()
{
	var_default CXX g++
	var_default CXXLD $CXX
	var_default CXXFLAGS "-O2 -Wall -g"
	var_default CXXLDFLAGS ""
	if check_program $CXX
	then
		makefile_env_vars CXX CXXLD CXXFLAGS CXXLDFLAGS
		if check_cxx_flag -MT /dev/null -MD -MP -MF /dev/null
		then
			makefile_var CXX_GENERATE_DEPS y
		else
			makefile_var CXX_GENERATE_DEPS n
		fi
		return 0
	fi
	return 1
}

check_ar()
{
	var_default AR ar
	var_default ARFLAGS "-cr"
	if check_program $AR
	then
		makefile_env_vars AR ARFLAGS
		return 0
	fi
	return 1
}

check_as()
{
	var_default AS gcc
	if check_program $AS
	then
		makefile_env_vars AS
		return 0
	fi
	return 1
}

check_pkgconfig()
{
	if [[ -z $PKG_CONFIG ]]
	then
		if check_program pkg-config PKG_CONFIG
		then
			makefile_env_vars PKG_CONFIG
		else
			# don't check again
			PKG_CONFIG="no"
			return 1
		fi
	fi
	return 0
}

# run pkg-config
#
# @name:    name
# @modules: 
#
# example:
#   ---
#   check_glib()
#   {
#     pkg_check_modules glib "glib-2.0 >= 2.2"
#     return $?
#   }
#
#   add_check check_glib
#   ---
#   GLIB_CFLAGS and GLIB_LIBS are automatically added to Makefile
pkg_check_modules()
{
	local name modules

	argc 2
	name="$1"
	modules="$2"
	
	check_pkgconfig
	if [[ $PKG_CONFIG = "no" ]]
	then
		msg_error "*** The pkg-config script could not be found. Make sure it is"
		msg_error "*** in your path, or set the PKG_CONFIG environment variable"
		msg_error "*** to the full path to pkg-config."
		msg_error "*** Or see http://www.freedesktop.org/software/pkgconfig to get pkg-config."
		return 1
	fi

	msg_checking "$modules"
	if $PKG_CONFIG --exists "$modules"
	then
		local i

		msg_result "yes"

		i=${#module_names[@]}

		msg_checking "CFLAGS for $name"
		module_cflags[$i]="$($PKG_CONFIG --cflags ""${modules}"")"
		msg_result ${module_cflags[$i]}

		msg_checking "LIBS for $name"
		module_libs[$i]="$($PKG_CONFIG --libs ""${modules}"")"
		msg_result ${module_libs[$i]}

		module_names[$i]="$name"
		return 0
	else
		msg_result "no"

		$PKG_CONFIG --errors-to-stdout --print-errors "$modules"
		msg_error "Library requirements (${modules}) not met; consider adjusting the PKG_CONFIG_PATH environment variable if your libraries are in a nonstandard prefix so pkg-config can find them."
		return 1
	fi
}

try_compile()
{
	local file src obj exe

	argc 1
	file="$1"
	src=$(tmp_file prog.c)
	obj=$(tmp_file prog.o)
	exe=$(tmp_file prog)
	echo "$file" > $src || exit 1
	$CC -c $src -o $obj 2>/dev/null || exit 1
	$LD -o $exe $obj 2>/dev/null
	return $?
}

# tries to link against a lib
# 
# @ldadd:  something like "-L/usr/X11R6/lib -lX11"
try_link()
{
	local ldadd
	local file src obj exe

	argc 1
	ldadd="$1"
	file="
int main(int argc, char *argv[])
{
	return 0;
}
"
	src=$(tmp_file prog.c)
	obj=$(tmp_file prog.o)
	exe=$(tmp_file prog)

	echo "$file" > $src || exit 1
	$CC -c $src -o $obj || return 1
	$LD $ldadd -o $exe $obj
	return $?
}

# @name:   user visible name
# @ldadd:  arg passed to try_link
check_lib()
{
	local name ldadd
	local output

	argc 2
	name="$1"
	ldadd="$2"
	msg_checking "$name"
	output=$(try_link "$ldadd" 2>&1)
	if [[ $? -eq 0 ]]
	then
		msg_result "yes"
		return 0
	else
		msg_result "no"
		msg_error "$output"
		return 1
	fi
}

check_x11()
{
	local X11_LIBS

	for X11_LIBS in "-lX11" "-L/usr/X11R6/lib -lX11"
	do
		if check_lib "X11" "$X11_LIBS"
		then
			makefile_var X11_LIBS "$X11_LIBS"
			return 0
		fi
	done
	return $?
}
