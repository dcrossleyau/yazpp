/* $Id: yaz-proxy-main.cpp,v 1.1 2004-03-29 22:48:31 adam Exp $
   Copyright (c) 1998-2004, Index Data.

This file is part of the yaz-proxy.

Zebra is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

Zebra is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with Zebra; see the file LICENSE.proxy.  If not, write to the
Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.
 */

#include <signal.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <stdarg.h>

#if HAVE_GETRLIMIT
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <yaz/log.h>
#include <yaz/options.h>

#include <yaz++/socket-manager.h>
#include <yaz++/pdu-assoc.h>
#include <yaz++/proxy/proxy.h>

void usage(char *prog)
{
    fprintf (stderr, "%s: [-c config] [-l log] [-a log] [-v level] [-t target] "
             "[-u uid] [-p pidfile] @:port\n", prog);
    exit (1);
}

static char *pid_fname = 0;
static char *uid = 0;
static char *log_file = 0;
static int debug = 0;
static int no_limit_files = 0;

int args(Yaz_Proxy *proxy, int argc, char **argv)
{
    char *addr = 0;
    char *arg;
    char *prog = argv[0];
    int ret;

    while ((ret = options("o:a:t:v:c:u:i:m:l:T:p:U:n:X",
			  argv, argc, &arg)) != -2)
    {
	int err;
        switch (ret)
        {
        case 0:
            if (addr)
	    {
		usage(prog);
		return 1;
	    }
	    addr = arg;
            break;
	case 'c':
	    err = proxy->set_config(arg);
	    if (err == -2)
	    {
		fprintf(stderr, "Config file support not enabled (proxy not compiled with libxml2 support)\n");
		exit(1);
	    }
	    else if (err == -1)
	    {
		fprintf(stderr, "Bad or missing file %s\n", arg);
		exit(1);
	    }
	    break;
	case 'a':
	    proxy->set_APDU_log(arg);
	    break;
        case 't':
	    proxy->set_default_target(arg);
	    break;
        case 'U':
            proxy->set_proxy_authentication(arg);
            break;
        case 'o':
	    proxy->option("optimize", arg);
	    break;
	case 'v':
	    yaz_log_init_level (yaz_log_mask_str(arg));
	    break;
	case 'l':
	    yaz_log_init_file (arg);
	    log_file = xstrdup(arg);
	    break;
	case 'm':
	    proxy->set_max_clients(atoi(arg));
	    break;
        case 'i':
	    proxy->set_client_idletime(atoi(arg));
	    break;
        case 'T':
	    proxy->set_target_idletime(atoi(arg));
	    break;
	case 'n':
	    no_limit_files = atoi(arg);
	    break;
	case 'X':
	    debug = 1;
	    break;
	case 'p':
	    if (!pid_fname)
		pid_fname = xstrdup(arg);
	    break;
	case 'u':
	    if (!uid)
		uid = xstrdup(arg);
	    break;
        default:
	    usage(prog);
	    return 1;
        }
    }
    if (addr)
    {
	if (proxy->server(addr))
	{
	    yaz_log(LOG_FATAL|LOG_ERRNO, "listen %s", addr);
	    exit(1);
	}
    }
    else
    {
	usage(prog);
	return 1;
    }
    return 0;
}

static Yaz_Proxy *static_yaz_proxy = 0;
static void sighup_handler(int num)
{
    signal(SIGHUP, sighup_handler);
    if (static_yaz_proxy)
	static_yaz_proxy->reconfig();
}

#if HAVE_XSLT
static void proxy_xml_error_handler(void *ctx, const char *fmt, ...)
{
    char buf[1024];

    va_list ap;
    va_start(ap, fmt);

    vsnprintf(buf, sizeof(buf), fmt, ap);

    yaz_log(LOG_WARN, "%s: %s", (char*) ctx, buf);

    va_end (ap);
}
#endif

static void child_run(Yaz_SocketManager *m, int run)
{
    signal(SIGHUP, sighup_handler);

#if HAVE_XSLT
    xmlSetGenericErrorFunc((void *) "XML", proxy_xml_error_handler);
    xsltSetGenericErrorFunc((void *) "XSLT", proxy_xml_error_handler);
#endif
    yaz_log(LOG_LOG, "0 proxy run=%d pid=%ld", run, (long) getpid());

    if (no_limit_files)
    {
#if HAVE_SETRLIMIT
	struct rlimit limit_data;
	limit_data.rlim_cur = no_limit_files;
	limit_data.rlim_max = no_limit_files;
	
	yaz_log(LOG_LOG, "0 setrlimit NOFILE cur=%d max=%d",
		limit_data.rlim_cur, limit_data.rlim_max);
	if (setrlimit(RLIMIT_NOFILE, &limit_data))
	    yaz_log(LOG_ERRNO|LOG_WARN, "setrlimit");
#else
	yaz_log(LOG_WARN, "setrlimit unavablable. Option -n ignored");
#endif
    }
    if (pid_fname)
    {
	FILE *f = fopen(pid_fname, "w");
	if (!f)
	{
	    yaz_log(LOG_ERRNO|LOG_FATAL, "Couldn't create %s", pid_fname);
	    exit(0);
	}
	fprintf(f, "%ld", (long) getpid());
	fclose(f);
	xfree(pid_fname);
    }
    if (uid)
    {
    	struct passwd *pw;

	if (!(pw = getpwnam(uid)))
	{
	    yaz_log(LOG_FATAL, "%s: Unknown user", uid);
	    exit(3);
	}
	if (log_file)
	{
	    chown(log_file, pw->pw_uid,  pw->pw_gid);
	    xfree(log_file);
	}
	if (setuid(pw->pw_uid) < 0)
	{
	    yaz_log(LOG_FATAL|LOG_ERRNO, "setuid");
	    exit(4);
	}
	xfree(uid);
    }
#if HAVE_GETRLIMIT
    struct rlimit limit_data;
    getrlimit(RLIMIT_NOFILE, &limit_data);
    yaz_log(LOG_LOG, "0 getrlimit NOFILE cur=%d max=%d",
	    limit_data.rlim_cur, limit_data.rlim_max);
#endif
    
    while (m->processEvent() > 0)
	;

    exit (0);
}

int main(int argc, char **argv)
{
#if HAVE_XSLT
    xmlInitMemory();
    
    LIBXML_TEST_VERSION
#endif
    int cont = 1;
    int run = 1;
    Yaz_SocketManager mySocketManager;
    Yaz_Proxy proxy(new Yaz_PDU_Assoc(&mySocketManager));

    static_yaz_proxy = &proxy;

    args(&proxy, argc, argv);

    if (debug)
    {
	child_run(&mySocketManager, run);
	exit(0);
    }
    while (cont)
    {
	pid_t p = fork();
	if (p == (pid_t) -1)
	{
	    yaz_log(LOG_FATAL|LOG_ERRNO, "fork");
	    exit(1);
	}
	else if (p == 0)
	{
	    child_run(&mySocketManager, run);
	}
	pid_t p1;
	int status;
	p1 = wait(&status);

	yaz_log_reopen();

	if (p1 != p)
	{
	    yaz_log(LOG_FATAL, "p1=%d != p=%d", p1, p);
	    exit(1);
	}
	if (WIFSIGNALED(status))
	{
	    switch(WTERMSIG(status)) {
	    case SIGILL:
		yaz_log(LOG_WARN, "Received SIGILL from child %ld", (long) p);
		cont = 1;
		break;
	    case SIGABRT:
		yaz_log(LOG_WARN, "Received SIGABRT from child %ld", (long) p);
		cont = 1;
		break ;
	    case SIGSEGV:
		yaz_log(LOG_WARN, "Received SIGSEGV from child %ld", (long) p);
		cont = 1;
		break;
	    case SIGBUS:	
		yaz_log(LOG_WARN, "Received SIGBUS from child %ld", (long) p);
		cont = 1;
		break;
	    case SIGTERM:
		yaz_log(LOG_LOG, "Received SIGTERM from child %ld",
			(long) p);
		cont = 0;
		break;
	    default:
		yaz_log(LOG_WARN, "Received SIG %d from child %ld",
			WTERMSIG(status), (long) p);
		cont = 0;
	    }
	}
	else if (status == 0)
	    cont = 0;
	else
	{
	    yaz_log(LOG_LOG, "Exit %d from child %ld", status, (long) p);
	    cont = 1;
	}
	if (cont)
	    sleep(1 + run/5);
	run++;
    }
    exit (0);
    return 0;
}