
AC_DEFUN([RMF_COMPILER_WARNINGS],
[AC_ARG_ENABLE(more-warnings,
	AS_HELP_STRING([--enable-more-warnings], [Possible values: no/yes/error]),
	set_more_warnings="$enableval",set_more_warnings=error)
AC_MSG_CHECKING(for GCC compiler)
if test "$GCC" = "yes"; then
	AC_MSG_RESULT(yes)
	CFLAGS="-Wall -std=gnu89 $CFLAGS"

	# Settings to disable warnings always applicable, even when
	#  --disable-more-warnings is being used, and for both C and C++
	for option in -fno-strict-aliasing -Wno-deprecated-declarations \
		      -Wno-unused-parameter -Wno-sign-compare \
		      -Wno-psabi; do
		SAVE_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $option"
		SAVE_CXXFLAGS="$CXXFLAGS"
		CXXFLAGS="$CXXFLAGS $option"
		AC_MSG_CHECKING([whether gcc understands $option])
		AC_TRY_COMPILE([], [],
			has_option=yes,
			has_option=no,)
		if test $has_option = no; then
			CFLAGS="$SAVE_CFLAGS"
			CXXFLAGS="$SAVE_CXXFLAGS"
		fi
		AC_MSG_RESULT($has_option)
		unset has_option
		unset SAVE_CFLAGS
		unset SAVE_CXXFLAGS
	done
	unset option
	if test "x$set_more_warnings" = xerror; then
		CFLAGS="$CFLAGS -Werror"
		CXXFLAGS="$CXXFLAGS -Werror"
	fi


	AC_MSG_CHECKING(for more warnings)
	if test "$GCC" = "yes" -a "$set_more_warnings" != "no"; then
		AC_MSG_RESULT(yes)
		for option in -Wmissing-declarations -Wmissing-prototypes \
			      -Wdeclaration-after-statement -Wstrict-prototypes \
			      -Wint-to-pointer-cast -Wfloat-equal \
			      -Wunused-but-set-variable -Wundef \
			      -Wimplicit-function-declaration \
			      -Wpointer-arith -Winit-self -Wshadow \
			      -Wmissing-include-dirs -Waggregate-return \
			      -Wformat-security; do
			SAVE_CFLAGS="$CFLAGS"
			CFLAGS="$CFLAGS $option"
			AC_MSG_CHECKING([whether gcc understands $option])
			AC_TRY_COMPILE([], [],
				has_option=yes,
				has_option=no,)
			if test $has_option = no; then
				CFLAGS="$SAVE_CFLAGS"
			fi
			AC_MSG_RESULT($has_option)
			unset has_option
			unset SAVE_CFLAGS
		done
		unset option
		if test "x$set_more_warnings" = xerror; then
			CFLAGS="$CFLAGS -Werror"
		fi
	else
		AC_MSG_RESULT(no)
	fi
fi
])
