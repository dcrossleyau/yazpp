## $Id: yazpp.m4,v 1.1 2002-11-14 14:41:07 adam Exp $
AC_DEFUN([YAZPP_INIT],
[
        AC_SUBST(YAZPPLIB)
        AC_SUBST(YAZPPLALIB)
        AC_SUBST(YAZPPINC)
        AC_SUBST(YAZPPVERSION)
        yazppconfig=NONE
        yazpppath=NONE
        AC_ARG_WITH(yazppconfig, [  --with-yazppconfig=DIR  yaz++-config in DIR (example /home/yaz++-0.5)], [yazpppath=$withval])
        if test "x$yazpppath" != "xNONE"; then
                yazppconfig=$yazpppath/yaz++-config
        else
                if test "x$srcdir" = "x"; then
                        yazppsrcdir=.
                else
                        yazppsrcdir=$srcdir
                fi
                for i in ${yazppsrcdir}/../yaz++-* ${yazppsrcdir}/../yaz++; do
                        if test -d $i; then
                                if test -r $i/yaz++-config; then
                                        yazppconfig=$i/yaz++-config
                                fi
                        fi
                done
                if test "x$yazppconfig" = "xNONE"; then
                        AC_PATH_PROG(yazppconfig, yaz-config, NONE)
                fi
        fi
        AC_MSG_CHECKING(for YAZ++)
        if $yazppconfig --version >/dev/null 2>&1; then
                YAZPPLIB=`$yazppconfig --libs $1`
                YAZPPLALIB=`$yazppconfig --lalibs $1`
                YAZPPINC=`$yazppconfig --cflags $1`
                YAZPPVERSION=`$yazppconfig --version`
                AC_MSG_RESULT($yazppconfig)
        else
                AC_MSG_RESULT(Not found)
                YAZVERSION=NONE
        fi
])
