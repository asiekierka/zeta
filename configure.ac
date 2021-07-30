AC_INIT([zeta], [beta36], [kontakt@asie.pl])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([autoconfig.h])

AC_PROG_CC
AC_LANG_PUSH([C])

AC_CANONICAL_HOST

AC_ARG_WITH([frontend],
	AS_HELP_STRING([--with-frontend=FRONTEND], [Use specified frontend]))

AS_IF([test "x$with_frontend" = "x"], [
	dnl Pick frontend.
	with_frontend=sdl2
])

AC_SUBST(FRONTEND_CFLAGS)
AC_SUBST(FRONTEND_LDFLAGS)
FRONTEND_CFLAGS=""
FRONTEND_LDFLAGS=""

AM_PATH_PYTHON([3.5])

AS_CASE([$with_frontend],
	[ncurses], [
		PKG_CHECK_MODULES(NCURSES, [ncursesw ncurses])
		FRONTEND_CFLAGS="$FRONTEND_CFLAGS $NCURSES_CFLAGS"
		FRONTEND_LDFLAGS="$FRONTEND_LDFLAGS $NCURSES_LIBS"
	],
	[sdl2], [
		PKG_CHECK_MODULES(SDL2, sdl2 >= 2.0.0)
		case "${host_os}" in
			mingw*)
				FRONTEND_CFLAGS="$FRONTEND_CFLAGS -mwindows $SDL2_CFLAGS"
				FRONTEND_LDFLAGS="$FRONTEND_LDFLAGS -Wl,-Bstatic -lstdc++ -lpthread -lmingw32 -lSDL2main -Wl,-Bdynamic $SDL2_LIBS -static-libgcc -static-libstdc++"
				;;
			*)
				FRONTEND_CFLAGS="$FRONTEND_CFLAGS $SDL2_CFLAGS"
				FRONTEND_LDFLAGS="$FRONTEND_LDFLAGS $SDL2_LIBS"
				;;
		esac
		PKG_CHECK_MODULES(LIBPNG, libpng >= 1.6.0, [
			AC_DEFINE(USE_LIBPNG, 1, [Use libpng.])
			FRONTEND_CFLAGS="$FRONTEND_CFLAGS $LIBPNG_CFLAGS"
			FRONTEND_LDFLAGS="$FRONTEND_LDFLAGS $LIBPNG_LIBS"
		])
		AX_CHECK_GL([
			AC_DEFINE(USE_OPENGL, 1, [Use OpenGL.])
			FRONTEND_CFLAGS="$FRONTEND_CFLAGS $GL_CFLAGS"
			FRONTEND_LDFLAGS="$FRONTEND_LDFLAGS $GL_LIBS"
		])
	],
	[AC_MSG_ERROR("frontend not selected or invalid frontend")]
)

AM_CONDITIONAL([FRONTEND_NCURSES], [test "x$with_frontend" = "xncurses"])
AM_CONDITIONAL([FRONTEND_SDL2], [test "x$with_frontend" = "xsdl2"])

AC_LANG_POP

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
