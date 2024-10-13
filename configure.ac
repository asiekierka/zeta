AC_INIT([zeta], [1.0.5], [kontakt@asie.pl])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([autoconfig.h])

AC_PROG_CC
AC_LANG_PUSH([C])

AC_CANONICAL_HOST

AM_PATH_PYTHON([3.5])

AC_ARG_WITH([frontend],
	AS_HELP_STRING([--with-frontend=FRONTEND], [Use specified frontend]))
AC_ARG_WITH([resampler],
	AS_HELP_STRING([--with-resampler=RESAMPLER], [Use specified resampler: nearest, linear, bandlimited]))
AC_ARG_ENABLE([opengl], AS_HELP_STRING([--enable-opengl], [Enable OpenGL support (for SDL3 frontend; default: auto)]), [use_opengl=$enableval])

AC_SUBST(FRONTEND_CFLAGS)
AC_SUBST(FRONTEND_LDFLAGS)
FRONTEND_CFLAGS=""
FRONTEND_LDFLAGS=""

AC_C_BIGENDIAN([AC_DEFINE(ZETA_BIG_ENDIAN, 1, [Platform is big endian.])])
AX_CHECK_ALIGNED_ACCESS_REQUIRED

AX_CHECK_COMPILE_FLAG([-std=c11], [
	FRONTEND_CFLAGS="$FRONTEND_CFLAGS -std=c11"
], [
	echo "C compiler cannot compile C11 code"
	exit -1
])

dnl Pick frontend.
AS_IF([test "x$with_frontend" = "x"], [
	with_frontend=sdl3
])

AS_CASE([$with_frontend],
	[ncurses], [
		PKG_CHECK_MODULES(NCURSES, [ncursesw ncurses])
		FRONTEND_CFLAGS="$FRONTEND_CFLAGS $NCURSES_CFLAGS"
		FRONTEND_LDFLAGS="$FRONTEND_LDFLAGS $NCURSES_LIBS"
	],
	[sdl3], [
		PKG_CHECK_MODULES(SDL3, sdl3 >= 3.1.1)
		case "${host_os}" in
			mingw*)
				FRONTEND_CFLAGS="$FRONTEND_CFLAGS -mwindows $SDL3_CFLAGS -DUSE_SDL"
				FRONTEND_LDFLAGS="$FRONTEND_LDFLAGS -Wl,-Bstatic -lstdc++ -lpthread -lmingw32 -Wl,-Bdynamic $SDL3_LIBS -static-libgcc -static-libstdc++"
				;;
			*)
				FRONTEND_CFLAGS="$FRONTEND_CFLAGS $SDL3_CFLAGS -DUSE_SDL"
				FRONTEND_LDFLAGS="$FRONTEND_LDFLAGS $SDL3_LIBS"
				;;
		esac
		PKG_CHECK_MODULES(LIBPNG, libpng >= 1.6.0, [
			AC_DEFINE(USE_LIBPNG, 1, [Use libpng.])
			FRONTEND_CFLAGS="$FRONTEND_CFLAGS $LIBPNG_CFLAGS"
			FRONTEND_LDFLAGS="$FRONTEND_LDFLAGS $LIBPNG_LIBS"
		])
		AX_CHECK_GL([
			AS_IF([test "x$use_opengl" != xno], [
				AC_DEFINE(USE_OPENGL, 1, [Use OpenGL.])
				FRONTEND_CFLAGS="$FRONTEND_CFLAGS $GL_CFLAGS"
				FRONTEND_LDFLAGS="$FRONTEND_LDFLAGS $GL_LIBS"
			])
		], [
			AS_IF([test "x$use_opengl" = xyes], [
				echo "OpenGL not found"
				exit -1
			])
		])
	],
	[AC_MSG_ERROR("frontend not selected or invalid frontend")]
)

AM_CONDITIONAL([FRONTEND_NCURSES], [test "x$with_frontend" = "xncurses"])
AM_CONDITIONAL([FRONTEND_SDL3], [test "x$with_frontend" = "xsdl3"])

dnl Pick resampler.
AS_IF([test "x$with_resampler" = "x"], [
	AC_CHECK_LIB([m],[cosf],[with_resampler=bandlimited],[with_resampler=linear])
])

AS_CASE([$with_resampler],
	[nearest], [AC_DEFINE(RESAMPLE_NEAREST, 1, [Resampling mode.])],
	[linear], [AC_DEFINE(RESAMPLE_LINEAR, 1, [Resampling mode.])],
	[bandlimited], [
		AC_DEFINE(RESAMPLE_BANDLIMITED, 1, [Resampling mode.])
		AC_CHECK_LIB([m],[cosf])
	],
	[AC_MSG_ERROR("resampler not selected or invalid resampler")]
)

AC_CHECK_FUNCS([ftruncate opendir])

AC_LANG_POP

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
