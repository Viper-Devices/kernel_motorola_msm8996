/* 
 * Copyright (C) 2000, 2001, 2002 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <asm/unistd.h>
#include <asm/page.h>
#include "user_util.h"
#include "kern_util.h"
#include "user.h"
#include "process.h"
#include "signal_kern.h"
#include "signal_user.h"
#include "sysdep/ptrace.h"
#include "sysdep/sigcontext.h"
#include "irq_user.h"
#include "ptrace_user.h"
#include "time_user.h"
#include "init.h"
#include "os.h"
#include "uml-config.h"
#include "choose-mode.h"
#include "mode.h"
#include "tempfile.h"
#ifdef UML_CONFIG_MODE_SKAS
#include "skas.h"
#include "skas_ptrace.h"
#include "registers.h"
#endif

void init_new_thread_stack(void *sig_stack, void (*usr1_handler)(int))
{
	int flags = 0, pages;

	if(sig_stack != NULL){
		pages = (1 << UML_CONFIG_KERNEL_STACK_ORDER);
		set_sigstack(sig_stack, pages * page_size());
		flags = SA_ONSTACK;
	}
	if(usr1_handler) set_handler(SIGUSR1, usr1_handler, flags, -1);
}

void init_new_thread_signals(int altstack)
{
	int flags = altstack ? SA_ONSTACK : 0;

	set_handler(SIGSEGV, (__sighandler_t) sig_handler, flags,
		    SIGUSR1, SIGIO, SIGWINCH, SIGALRM, SIGVTALRM, -1);
	set_handler(SIGTRAP, (__sighandler_t) sig_handler, flags, 
		    SIGUSR1, SIGIO, SIGWINCH, SIGALRM, SIGVTALRM, -1);
	set_handler(SIGFPE, (__sighandler_t) sig_handler, flags, 
		    SIGUSR1, SIGIO, SIGWINCH, SIGALRM, SIGVTALRM, -1);
	set_handler(SIGILL, (__sighandler_t) sig_handler, flags, 
		    SIGUSR1, SIGIO, SIGWINCH, SIGALRM, SIGVTALRM, -1);
	set_handler(SIGBUS, (__sighandler_t) sig_handler, flags, 
		    SIGUSR1, SIGIO, SIGWINCH, SIGALRM, SIGVTALRM, -1);
	set_handler(SIGUSR2, (__sighandler_t) sig_handler, 
		    flags, SIGUSR1, SIGIO, SIGWINCH, SIGALRM, SIGVTALRM, -1);
	signal(SIGHUP, SIG_IGN);

	init_irq_signals(altstack);
}

struct tramp {
	int (*tramp)(void *);
	void *tramp_data;
	unsigned long temp_stack;
	int flags;
	int pid;
};

/* See above for why sigkill is here */

int sigkill = SIGKILL;

int outer_tramp(void *arg)
{
	struct tramp *t;
	int sig = sigkill;

	t = arg;
	t->pid = clone(t->tramp, (void *) t->temp_stack + page_size()/2,
		       t->flags, t->tramp_data);
	if(t->pid > 0) wait_for_stop(t->pid, SIGSTOP, PTRACE_CONT, NULL);
	kill(os_getpid(), sig);
	_exit(0);
}

int start_fork_tramp(void *thread_arg, unsigned long temp_stack, 
		     int clone_flags, int (*tramp)(void *))
{
	struct tramp arg;
	unsigned long sp;
	int new_pid, status, err;

	/* The trampoline will run on the temporary stack */
	sp = stack_sp(temp_stack);

	clone_flags |= CLONE_FILES | SIGCHLD;

	arg.tramp = tramp;
	arg.tramp_data = thread_arg;
	arg.temp_stack = temp_stack;
	arg.flags = clone_flags;

	/* Start the process and wait for it to kill itself */
	new_pid = clone(outer_tramp, (void *) sp, clone_flags, &arg);
	if(new_pid < 0)
		return(new_pid);

	CATCH_EINTR(err = waitpid(new_pid, &status, 0));
	if(err < 0)
		panic("Waiting for outer trampoline failed - errno = %d",
		      errno);

	if(!WIFSIGNALED(status) || (WTERMSIG(status) != SIGKILL))
		panic("outer trampoline didn't exit with SIGKILL, "
		      "status = %d", status);

	return(arg.pid);
}

static int ptrace_child(void)
{
	int ret;
	int pid = os_getpid(), ppid = getppid();
	int sc_result;

	if(ptrace(PTRACE_TRACEME, 0, 0, 0) < 0){
		perror("ptrace");
		os_kill_process(pid, 0);
	}
	os_stop_process(pid);

	/*This syscall will be intercepted by the parent. Don't call more than
	 * once, please.*/
	sc_result = os_getpid();

	if (sc_result == pid)
		ret = 1; /*Nothing modified by the parent, we are running
			   normally.*/
	else if (sc_result == ppid)
		ret = 0; /*Expected in check_ptrace and check_sysemu when they
			   succeed in modifying the stack frame*/
	else
		ret = 2; /*Serious trouble! This could be caused by a bug in
			   host 2.6 SKAS3/2.6 patch before release -V6, together
			   with a bug in the UML code itself.*/
	_exit(ret);
}

static int start_ptraced_child(void)
{
	int pid, n, status;
	
	pid = fork();
	if(pid == 0)
		ptrace_child();

	if(pid < 0)
		panic("check_ptrace : fork failed, errno = %d", errno);
	CATCH_EINTR(n = waitpid(pid, &status, WUNTRACED));
	if(n < 0)
		panic("check_ptrace : wait failed, errno = %d", errno);
	if(!WIFSTOPPED(status) || (WSTOPSIG(status) != SIGSTOP))
		panic("check_ptrace : expected SIGSTOP, got status = %d",
		      status);

	return(pid);
}

/* When testing for SYSEMU support, if it is one of the broken versions, we must
 * just avoid using sysemu, not panic, but only if SYSEMU features are broken.
 * So only for SYSEMU features we test mustpanic, while normal host features
 * must work anyway!*/
static int stop_ptraced_child(int pid, int exitcode, int mustexit)
{
	int status, n, ret = 0;

	if(ptrace(PTRACE_CONT, pid, 0, 0) < 0)
		panic("stop_ptraced_child : ptrace failed, errno = %d", errno);
	CATCH_EINTR(n = waitpid(pid, &status, 0));
	if(!WIFEXITED(status) || (WEXITSTATUS(status) != exitcode)) {
		int exit_with = WEXITSTATUS(status);
		if (exit_with == 2)
			printk("check_ptrace : child exited with status 2. "
			       "Serious trouble happening! Try updating your "
			       "host skas patch!\nDisabling SYSEMU support.");
		printk("check_ptrace : child exited with exitcode %d, while "
		      "expecting %d; status 0x%x", exit_with,
		      exitcode, status);
		if (mustexit)
			panic("\n");
		else
			printk("\n");
		ret = -1;
	}

	return ret;
}

static int force_sysemu_disabled = 0;

int ptrace_faultinfo = 1;
int proc_mm = 1;

static int __init skas0_cmd_param(char *str, int* add)
{
	ptrace_faultinfo = proc_mm = 0;
	return 0;
}

static int __init nosysemu_cmd_param(char *str, int* add)
{
	force_sysemu_disabled = 1;
	return 0;
}

__uml_setup("skas0", skas0_cmd_param,
		"skas0\n"
		"    Disables SKAS3 usage, so that SKAS0 is used, unless you \n"
		"    specify mode=tt.\n\n");

__uml_setup("nosysemu", nosysemu_cmd_param,
		"nosysemu\n"
		"    Turns off syscall emulation patch for ptrace (SYSEMU) on.\n"
		"    SYSEMU is a performance-patch introduced by Laurent Vivier. It changes\n"
		"    behaviour of ptrace() and helps reducing host context switch rate.\n"
		"    To make it working, you need a kernel patch for your host, too.\n"
		"    See http://perso.wanadoo.fr/laurent.vivier/UML/ for further information.\n\n");

static void __init check_sysemu(void)
{
	int pid, syscall, n, status, count=0;

	printk("Checking syscall emulation patch for ptrace...");
	sysemu_supported = 0;
	pid = start_ptraced_child();

	if(ptrace(PTRACE_SYSEMU, pid, 0, 0) < 0)
		goto fail;

	CATCH_EINTR(n = waitpid(pid, &status, WUNTRACED));
	if (n < 0)
		panic("check_sysemu : wait failed, errno = %d", errno);
	if(!WIFSTOPPED(status) || (WSTOPSIG(status) != SIGTRAP))
		panic("check_sysemu : expected SIGTRAP, "
		      "got status = %d", status);

	n = ptrace(PTRACE_POKEUSR, pid, PT_SYSCALL_RET_OFFSET,
		   os_getpid());
	if(n < 0)
		panic("check_sysemu : failed to modify system "
		      "call return, errno = %d", errno);

	if (stop_ptraced_child(pid, 0, 0) < 0)
		goto fail_stopped;

	sysemu_supported = 1;
	printk("OK\n");
	set_using_sysemu(!force_sysemu_disabled);

	printk("Checking advanced syscall emulation patch for ptrace...");
	pid = start_ptraced_child();
	while(1){
		count++;
		if(ptrace(PTRACE_SYSEMU_SINGLESTEP, pid, 0, 0) < 0)
			goto fail;
		CATCH_EINTR(n = waitpid(pid, &status, WUNTRACED));
		if(n < 0)
			panic("check_ptrace : wait failed, errno = %d", errno);
		if(!WIFSTOPPED(status) || (WSTOPSIG(status) != SIGTRAP))
			panic("check_ptrace : expected (SIGTRAP|SYSCALL_TRAP), "
			      "got status = %d", status);

		syscall = ptrace(PTRACE_PEEKUSR, pid, PT_SYSCALL_NR_OFFSET,
				 0);
		if(syscall == __NR_getpid){
			if (!count)
				panic("check_ptrace : SYSEMU_SINGLESTEP doesn't singlestep");
			n = ptrace(PTRACE_POKEUSR, pid, PT_SYSCALL_RET_OFFSET,
				   os_getpid());
			if(n < 0)
				panic("check_sysemu : failed to modify system "
				      "call return, errno = %d", errno);
			break;
		}
	}
	if (stop_ptraced_child(pid, 0, 0) < 0)
		goto fail_stopped;

	sysemu_supported = 2;
	printk("OK\n");

	if ( !force_sysemu_disabled )
		set_using_sysemu(sysemu_supported);
	return;

fail:
	stop_ptraced_child(pid, 1, 0);
fail_stopped:
	printk("missing\n");
}

void __init check_ptrace(void)
{
	int pid, syscall, n, status;

	printk("Checking that ptrace can change system call numbers...");
	pid = start_ptraced_child();

	if (ptrace(PTRACE_OLDSETOPTIONS, pid, 0, (void *)PTRACE_O_TRACESYSGOOD) < 0)
		panic("check_ptrace: PTRACE_SETOPTIONS failed, errno = %d", errno);

	while(1){
		if(ptrace(PTRACE_SYSCALL, pid, 0, 0) < 0)
			panic("check_ptrace : ptrace failed, errno = %d", 
			      errno);
		CATCH_EINTR(n = waitpid(pid, &status, WUNTRACED));
		if(n < 0)
			panic("check_ptrace : wait failed, errno = %d", errno);
		if(!WIFSTOPPED(status) || (WSTOPSIG(status) != SIGTRAP + 0x80))
			panic("check_ptrace : expected SIGTRAP + 0x80, "
			      "got status = %d", status);
		
		syscall = ptrace(PTRACE_PEEKUSR, pid, PT_SYSCALL_NR_OFFSET,
				 0);
		if(syscall == __NR_getpid){
			n = ptrace(PTRACE_POKEUSR, pid, PT_SYSCALL_NR_OFFSET,
				   __NR_getppid);
			if(n < 0)
				panic("check_ptrace : failed to modify system "
				      "call, errno = %d", errno);
			break;
		}
	}
	stop_ptraced_child(pid, 0, 1);
	printk("OK\n");
	check_sysemu();
}

int run_kernel_thread(int (*fn)(void *), void *arg, void **jmp_ptr)
{
	sigjmp_buf buf;
	int n;

	*jmp_ptr = &buf;
	n = sigsetjmp(buf, 1);
	if(n != 0)
		return(n);
	(*fn)(arg);
	return(0);
}

void forward_pending_sigio(int target)
{
	sigset_t sigs;

	if(sigpending(&sigs)) 
		panic("forward_pending_sigio : sigpending failed");
	if(sigismember(&sigs, SIGIO))
		kill(target, SIGIO);
}

extern void *__syscall_stub_start, __syscall_stub_end;

#ifdef UML_CONFIG_MODE_SKAS

static inline void check_skas3_ptrace_support(void)
{
	struct ptrace_faultinfo fi;
	int pid, n;

	printf("Checking for the skas3 patch in the host...");
	pid = start_ptraced_child();

	n = ptrace(PTRACE_FAULTINFO, pid, 0, &fi);
	if (n < 0) {
		ptrace_faultinfo = 0;
		if(errno == EIO)
			printf("not found\n");
		else {
			perror("not found");
		}
	}
	else {
		if (!ptrace_faultinfo)
			printf("found but disabled on command line\n");
		else
			printf("found\n");
	}

	init_registers(pid);
	stop_ptraced_child(pid, 1, 1);
}

int can_do_skas(void)
{
	printf("Checking for /proc/mm...");
	if (os_access("/proc/mm", OS_ACC_W_OK) < 0) {
		proc_mm = 0;
		printf("not found\n");
	} else {
		if (!proc_mm)
			printf("found but disabled on command line\n");
		else
			printf("found\n");
	}

	check_skas3_ptrace_support();
	return 1;
}
#else
int can_do_skas(void)
{
	return(0);
}
#endif
