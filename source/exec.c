/*
 * exec.c: handles exec'd process for IRCII 
 *
 * Written By Michael Sandrof
 *
 * Copyright (c) 1990 Michael Sandrof.
 * Copyright (c) 1991, 1992 Troy Rollo.
 * Copyright (c) 1992-2014 Matthew R. Green.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: exec.c,v 1.99 2014/03/14 20:59:19 mrg Exp $");

#include <sys/wait.h>

#include "exec.h"
#include "vars.h"
#include "ircaux.h"
#include "edit.h"
#include "window.h"
#include "screen.h"
#include "hook.h"
#include "input.h"
#include "list.h"
#include "server.h"
#include "output.h"
#include "parse.h"
#include "dcc.h"
#include "newio.h"
#include "alias.h"

/* Process: the structure that has all the info needed for each process */
typedef struct
{
	u_char	*name;			/* full process name */
	u_char	*logical;
	pid_t	pid;			/* process-id of process */
	int	p_stdin;		/* stdin description for process */
	int	p_stdout;		/* stdout descriptor for process */
	int	p_stderr;		/* stderr descriptor for process */
	int	counter;		/* output line counter for process */
	u_char	*redirect;		/* redirection command (MSG, NOTICE) */
	unsigned int	refnum;		/* a window for output to go to */
	int	server;			/* the server to use for output */
	u_char	*who;			/* nickname used for redirection */
	int	exited;			/* true if process has exited */
	int	termsig;		/* The signal that terminated
					 * the process */
	int	retcode;		/* return code of process */
	List	*waitcmds;		/* commands queued by WAIT -CMD */
}	Process;

static	Process **process_list = NULL;
static	int	process_list_size = 0;

static	int	exec_close(int);
static	void	start_process(u_char *, u_char *, u_char *, u_char *, unsigned int);
static	void	kill_process(int, int);
static	int	delete_process(int);
static	void	list_processes(void);
static	int	valid_process_index(int);
static	void	add_process(u_char *, u_char *, int, int, int, int, u_char *, u_char *, unsigned int);
static	int	is_logical_unique(u_char *);
static	void	send_exec_result(Process *, u_char *);

/* We use environ here */
extern	char	**environ;

/*
 * A nice array of the possible signals.  Used by the coredump trapping
 * routines and in the exec.c package .
 *
 * We really rely on the signals being all upper case.
 */

#include "sig.inc"

/*
 * exec_close: silly, eh?  Well, it makes the code look nicer.  Or does it
 * really?  No.  But what the hell 
 */
static	int
exec_close(int des)
{
	if (des != -1)
		new_close(des);
	return (-1);
}

/*
 * valid_process_index: checks to see if index refers to a valid running
 * process and returns true if this is the case.  Returns false otherwise 
 */
static	int
valid_process_index(int proccess)
{
	if ((proccess < 0) || (proccess >= process_list_size))
		return (0);
	if (process_list[proccess])
		return (1);
	return (0);
}

int
get_child_exit(int pid)
{
	return (check_wait_status(pid));
}

/*
 * check_wait_status: This waits on child processes, reporting terminations
 * as needed, etc 
 */
int
check_wait_status(int wanted)
{
	Process	*proc;
	int	status;
	int	pid,
		i;

	if ((pid = waitpid(wanted, &status, WNOHANG)) > 0)
	{
		if (wanted != -1 && pid == wanted)
		{
			if (WIFEXITED(status))
				return WEXITSTATUS(status);
			if (WIFSTOPPED(status))
				return - (WSTOPSIG(status));
			if (WIFSIGNALED(status))
				return - (WTERMSIG(status));
		}
		errno = 0;	/* reset errno, cause waitpid changes it */
		for (i = 0; i < process_list_size; i++)
		{
			if ((proc = process_list[i]) && proc->pid == pid)
			{
				proc->exited = 1;
				proc->termsig = WTERMSIG(status);
				proc->retcode = WEXITSTATUS(status);
				if ((proc->p_stderr == -1) &&
				    (proc->p_stdout == -1))
					delete_process(i);
				return 0;
			}
		}
	}
	return -1;
}

/*
 * check_process_limits: checks each running process to see if it's reached
 * the user selected maximum number of output lines.  If so, the processes is
 * effectively killed 
 */
void
check_process_limits(void)
{
	int	limit;
	int	i;
	Process	*proc;

	if ((limit = get_int_var(SHELL_LIMIT_VAR)) && process_list)
	{
		for (i = 0; i < process_list_size; i++)
		{
			if ((proc = process_list[i]) != NULL)
			{
				if (proc->counter >= limit)
				{
					proc->p_stdin = exec_close(proc->p_stdin);
					proc->p_stdout = exec_close(proc->p_stdout);
					proc->p_stderr = exec_close(proc->p_stderr);
					if (proc->exited)
						delete_process(i);
				}
			}
		}
	}
}

static void
send_exec_result(Process *proc, u_char *exec_buffer)
{
	if (proc->redirect)
	{
		if (!my_strcmp(proc->redirect, "FILTER"))
		{
			u_char *rest, *filter;
			int arg_flag = 0;
			
			rest = exec_buffer;
			while (*rest && *rest != ' ')
				++rest;
			
			while (*rest == ' ')
				++rest;
			
			filter = call_function(proc->who,
				exec_buffer /* f_args */,
				empty_string() /* args */,
				&arg_flag);
			if (filter)
			{
				u_char *sub_buffer = NULL;

				malloc_strcpy(&sub_buffer, filter);
				malloc_strcat(&sub_buffer, UP(" "));
				malloc_strcat(&sub_buffer, rest);
				
				parse_command(sub_buffer, 0, empty_string());
				new_free(&sub_buffer);
				new_free(&filter);
			}
		}
		else
		{
			send_text(proc->who, exec_buffer, proc->redirect);
		}
	}
	else
		put_it("%s", exec_buffer);
}

/*
 * do_processes: given a set of read-descriptors (as returned by a call to
 * select()), this will determine which of the process has produced output
 * and will hadle it appropriately 
 */
void
do_processes(fd_set *rd)
{
	int	i,
		flag;
	u_char	exec_buffer[INPUT_BUFFER_SIZE];
	u_char	*ptr;
	Process	*proc;
	int	old_timeout;
	int	server;

	if (process_list == NULL)
		return;
	old_timeout = dgets_timeout(1);
	for (i = 0; i < process_list_size; i++)
	{
		if ((proc = process_list[i]) && proc->p_stdout != -1)
		{
			if (FD_ISSET(proc->p_stdout, rd))
			{
				switch (dgets(exec_buffer, sizeof exec_buffer, proc->p_stdout))
				{
				case 0:
					if (proc->p_stderr == -1)
					{
						proc->p_stdin = exec_close(proc->p_stdin);
						proc->p_stdout = exec_close(proc->p_stdout);
						if (proc->exited)
							delete_process(i);
					}
					else
						proc->p_stdout = exec_close(proc->p_stdout);
					break;
				case -1:
					server = set_from_server(proc->server);
					if (proc->logical)
						flag = do_hook(EXEC_PROMPT_LIST, "%s %s", proc->logical, exec_buffer);
					else
						flag = do_hook(EXEC_PROMPT_LIST, "%d %s", i, exec_buffer);
					set_from_server(server);
					set_prompt_by_refnum(proc->refnum, exec_buffer);
					update_input(UPDATE_ALL);
					/* if (flag == 0) */
					break;
				default:
					server = set_from_server(proc->server);
					message_to(proc->refnum);
					proc->counter++;
					exec_buffer[sizeof(exec_buffer) - 1] = '\0';	/* blah... */
					ptr = exec_buffer + my_strlen(exec_buffer) - 1;
					if ((*ptr == '\n') || (*ptr == '\r'))
					{
						*ptr = (u_char) 0;
						ptr = exec_buffer + my_strlen(exec_buffer) - 1;
						if ((*ptr == '\n') || (*ptr == '\r'))
							*ptr = (u_char) 0;
					}
					if (proc->logical)
						flag = do_hook(EXEC_LIST, "%s %s", proc->logical, exec_buffer);
					else
						flag = do_hook(EXEC_LIST, "%d %s", i, exec_buffer);

					if (flag)
						send_exec_result(proc, exec_buffer);
					message_to(0);
					set_from_server(server);
					break;
				}
			}
		}
		if (process_list && i < process_list_size &&
		    (proc = process_list[i]) && proc->p_stderr != -1)
		{
			if (FD_ISSET(proc->p_stderr, rd))
			{
				switch (dgets(exec_buffer, sizeof exec_buffer, proc->p_stderr))
				{
				case 0:
					if (proc->p_stdout == -1)
					{
						proc->p_stderr = exec_close(proc->p_stderr);
						proc->p_stdin = exec_close(proc->p_stdin);
						if (proc->exited)
							delete_process(i);
					}
					else
						proc->p_stderr = exec_close(proc->p_stderr);
					break;

				case -1:
					server = set_from_server(proc->server);
					if (proc->logical)
						flag = do_hook(EXEC_PROMPT_LIST, "%s %s", proc->logical, exec_buffer);
					else
						flag = do_hook(EXEC_PROMPT_LIST, "%d %s", i, exec_buffer);
					set_prompt_by_refnum(proc->refnum, exec_buffer);
					update_input(UPDATE_ALL);
					set_from_server(server);
					if (flag == 0)
						break;

				default:
					server = set_from_server(proc->server);
					message_to(proc->refnum);
					(proc->counter)++;
					ptr = exec_buffer + my_strlen(exec_buffer) - 1;
					if ((*ptr == '\n') || (*ptr == '\r'))
					{
						*ptr = (u_char) 0;
						ptr = exec_buffer + my_strlen(exec_buffer) - 1;
						if ((*ptr == '\n') || (*ptr == '\r'))
							*ptr = (u_char) 0;
					}
					if (proc->logical)
						flag = do_hook(EXEC_ERRORS_LIST, "%s %s", proc->logical, exec_buffer);
					else
						flag = do_hook(EXEC_ERRORS_LIST, "%d %s", i, exec_buffer);
					if (flag)
						send_exec_result(proc, exec_buffer);
					message_to(0);
					set_from_server(server);
					break;
				}
			}
		}
	}
	(void) dgets_timeout(old_timeout);
}

/*
 * set_process_bits: This will set the bits in a fd_set map for each of the
 * process descriptors. 
 */
void
set_process_bits(fd_set *rd)
{
	int	i;
	Process	*proc;

	if (process_list)
	{
		for (i = 0; i < process_list_size; i++)
		{
			if ((proc = process_list[i]) != NULL)
			{
				if (proc->p_stdout != -1)
					FD_SET(proc->p_stdout, rd);
				if (proc->p_stderr != -1)
					FD_SET(proc->p_stderr, rd);
			}
		}
	}
}

/*
 * list_processes: displays a list of all currently running processes,
 * including index number, pid, and process name 
 */
static	void
list_processes(void)
{
	Process	*proc;
	int	i;
	int     lastlog_level;

	lastlog_level = set_lastlog_msg_level(LOG_CRAP);
	if (process_list)
	{
		say("Process List:");
		for (i = 0; i < process_list_size; i++)
		{
			if ((proc = process_list[i]) != NULL)
			{
				if (proc->logical)
					say("\t%d (%s): %s", i,
						proc->logical,
						proc->name);
				else
					say("\t%d: %s", i,
						proc->name);
			}
		}
	}
	else
		say("No processes are running");
	set_lastlog_msg_level(lastlog_level);
}

void
add_process_wait(int proc_index, u_char *cmd)
{
	List	*new,
		**posn;

	for (posn = &process_list[proc_index]->waitcmds; *posn != NULL; posn = &(*posn)->next)
		;
	new = new_malloc(sizeof *new);
	*posn = new;
	new->next = NULL;
	new->name = NULL;
	malloc_strcpy(&new->name, cmd);
}

/*
 * delete_process: Removes the process specifed by index from the process
 * list.  The does not kill the process, close the descriptors, or any such
 * thing.  It only deletes it from the list.  If appropriate, this will also
 * shrink the list as needed 
 */
static	int
delete_process(int process)
{
	int	flag;
	List	*cmd,
		*next;

	if (process_list)
	{
		if (process >= process_list_size)
			return (-1);
		if (process_list[process])
		{
			Process *dead;

			dead = process_list[process];
			process_list[process] = NULL;
			if (process == (process_list_size - 1))
			{
				int	i;

				for (i = process_list_size - 1;
						process_list_size;
						process_list_size--, i--)
				{
					if (process_list[i])
						break;
				}
				if (process_list_size)
					process_list = new_realloc(process_list, sizeof(*process_list) * process_list_size);
				else
				{
					new_free(&process_list);
					process_list = NULL;
				}
			}
			for (next = dead->waitcmds; next;)
			{
				cmd = next;
				next = next->next;
				parse_command(cmd->name, 0, empty_string());
				new_free(&cmd->name);
				new_free(&cmd);
			}
			dead->waitcmds = NULL;
			if (dead->logical)
				flag = do_hook(EXEC_EXIT_LIST, "%s %d %d",
					dead->logical, dead->termsig,
					dead->retcode);
			else
				flag = do_hook(EXEC_EXIT_LIST, "%d %d %d",
					process, dead->termsig, dead->retcode);
			if (flag)
			{
				if (get_int_var(NOTIFY_ON_TERMINATION_VAR))
				{
					if (dead->termsig)
						say("Process %d (%s) terminated with signal %s (%d)", process, dead->name, signals[dead->termsig],
							dead->termsig);
					else
						say("Process %d (%s) terminated with return code %d", process, dead->name, dead->retcode);
				}
			}
			new_free(&dead->name);
			new_free(&dead->logical);
			new_free(&dead->who);
			new_free(&dead->redirect);
			new_free(&dead);
			return (0);
		}
	}
	return (-1);
}

/*
 * add_process: adds a new process to the process list, expanding the list as
 * needed.  It will first try to add the process to a currently unused spot
 * in the process table before it increases its size. 
 */
static	void
add_process(u_char *name, u_char *logical, int pid, int p_stdin, int p_stdout, int p_stderr, u_char *redirect, u_char *who, unsigned refnum)
{
	int	i;
	Process	*proc;

	if (process_list == NULL)
	{
		process_list = new_malloc(sizeof *process_list);
		process_list_size = 1;
		process_list[0] = NULL;
	}
	for (i = 0; i < process_list_size; i++)
	{
		if (!process_list[i])
		{
			proc = process_list[i] = new_malloc(sizeof *proc);
			proc->name = NULL;
			malloc_strcpy(&(proc->name), name);
			proc->logical = NULL;
			malloc_strcpy(&(proc->logical), logical);
			proc->pid = pid;
			proc->p_stdin = p_stdin;
			proc->p_stdout = p_stdout;
			proc->p_stderr = p_stderr;
			proc->refnum = refnum;
			proc->redirect = NULL;
			if (redirect)
				malloc_strcpy(&(proc->redirect),
					redirect);
			if (parsing_server() != -1)
				proc->server = parsing_server();
			else
				proc->server = window_get_server(curr_scr_win);
			proc->counter = 0;
			proc->exited = 0;
			proc->termsig = 0;
			proc->retcode = 0;
			proc->who = NULL;
			proc->waitcmds = NULL;
			if (who)
				malloc_strcpy(&(process_list[i]->who), who);
			return;
		}
	}
	process_list_size++;
	process_list = new_realloc(process_list, sizeof(*process_list) * process_list_size);
	process_list[process_list_size - 1] = NULL;
	proc = process_list[i] = new_malloc(sizeof *proc);
	proc->name = NULL;
	malloc_strcpy(&(proc->name), name);
	proc->logical = NULL;
	malloc_strcpy(&(proc->logical), logical);
	proc->pid = pid;
	proc->p_stdin = p_stdin;
	proc->p_stdout = p_stdout;
	proc->p_stderr = p_stderr;
	proc->refnum = refnum;
	proc->redirect = NULL;
	if (redirect)
		malloc_strcpy(&(proc->redirect), redirect);
	proc->server = window_get_server(curr_scr_win);
	proc->counter = 0;
	proc->exited = 0;
	proc->termsig = 0;
	proc->retcode = 0;
	proc->who = NULL;
	proc->waitcmds = NULL;
	if (who)
		malloc_strcpy(&(proc->who), who);
}

/*
 * kill_process: sends the given signal to the process specified by the given
 * index into the process table.  After the signal is sent, the process is
 * deleted from the process table 
 */
static	void
kill_process(int kill_index, int sig)
{
	if (process_list && (kill_index < process_list_size) && process_list[kill_index])
	{
		pid_t	pgid;

		say("Sending signal %s (%d) to process %d: %s", signals[sig], sig, kill_index, process_list[kill_index]->name);
#ifdef HAVE_GETPGID
		pgid = getpgid(process_list[kill_index]->pid);
#else
# ifdef __sgi
		pgid = BSDgetpgrp(process_list[kill_index]->pid);
# else
#  ifdef HPUX
		pgid = getpgrp2(process_list[kill_index]->pid);
#  else
		pgid = getpgrp(process_list[kill_index]->pid);
#  endif /* HPUX */
# endif /* __sgi */
#endif /* HAVE_GETPGID */

		if (pgid == getpid())
		{
			yell("--- kill_process got pgid of pid!!!  something is very wrong");
			return;
		}
		killpg(pgid, sig);
		kill(process_list[kill_index]->pid, sig);
	}
	else
		say("There is no process %d", kill_index);
}

static	int
is_logical_unique(u_char *logical)
{
	Process	*proc;
	int	i;

	if (logical)
		for (i = 0; i < process_list_size; i++)
			if ((proc = process_list[i]) && proc->logical &&
			    (my_stricmp(proc->logical, logical) == 0))
				return 0;
	return 1;
}

/*
 * start_process: Attempts to start the given process using the SHELL as set
 * by the user. 
 */
static	void
start_process(u_char *name, u_char *logical, u_char *redirect, u_char *who, unsigned refnum)
{
	int	p0[2],
		p1[2],
		p2[2],
		pid,
		cnt;
	u_char	*shell,
		*flag,
		*arg;
	u_char	buffer[BIG_BUFFER_SIZE];

#ifdef DAEMON_UID
	if (getuid() == DAEMON_UID)
	{
		say("Sorry, you are not allowed to use EXEC");
		return;
	}
#endif /* DAEMON_UID */
	p0[0] = p0[1] = -1;
	p1[0] = p1[1] = -1;
	p2[0] = p2[1] = -1;
	if (pipe(p0) || pipe(p1) || pipe(p2))
	{
		say("Unable to start new process: %s", strerror(errno));
		if (p0[0] != -1)
		{
			new_close(p0[0]);
			new_close(p0[1]);
		}
		if (p1[0] != -1)
		{
			new_close(p1[0]);
			new_close(p1[1]);
		}
		if (p2[0] != -1)
		{
			new_close(p2[0]);
			new_close(p2[1]);
		}
		return;
	}
	switch (pid = fork())
	{
	case -1:
		say("Couldn't start new process!");
		break;
	case 0:
#ifdef HAVE_SETSID
		setsid();
#else
		setpgrp(0, getpid());
#endif /* HAVE_SETSID */
		MY_SIGNAL(SIGINT, (sigfunc *) SIG_IGN, 0);
		dup2(p0[0], 0);
		dup2(p1[1], 1);
		dup2(p2[1], 2);
		new_close(p0[0]);
		new_close(p0[1]);
		new_close(p1[0]);
		new_close(p1[1]);
		new_close(p2[0]);
		new_close(p2[1]);
		close_all_server();
		close_all_dcc();
		close_all_exec();
		close_all_screen();

		/* fix environment */
		for (cnt = 0, arg = UP(environ[0]);
		     arg; arg = UP(environ[++cnt]))
		{
			if (my_strncmp(arg, "TERM=", 5) == 0)
			{
				environ[cnt] = "TERM=tty";
				break;
			}
		}
		if ((shell = get_string_var(SHELL_VAR)) == NULL)
		{
			u_char	**args;
			int	max;

			cnt = 0;
			max = 5;
			args = new_malloc(sizeof(*args) * max);
			while ((arg = next_arg(name, &name)) != NULL)
			{
				if (cnt == max)
				{
					max += 5;
					args = new_realloc(args, sizeof(*args) * max);
				}
				args[cnt++] = arg;
			}
			args[cnt] = NULL;
			(void)setuid(getuid()); /* If we are setuid, set it back! */
			(void)setgid(getgid());
			execvp((char *) args[0], (char **) args);
		}
		else
		{
			if ((flag = get_string_var(SHELL_FLAGS_VAR)) ==
					NULL)
				flag = empty_string();
			(void)setuid(getuid());
			(void)setgid(getgid());
			execl(CP(shell), CP(shell), flag, name, NULL);
		}
		snprintf(CP(buffer), sizeof buffer, "*** Error starting shell \"%s\": %s\n", shell,
			strerror(errno));
		(void)write(1, buffer, my_strlen(buffer));
		_exit(-1);
		break;
	default:
		new_close(p0[0]);
		new_close(p1[1]);
		new_close(p2[1]);
		add_process(name, logical, pid, p0[1], p1[0], p2[0], redirect,
			who, refnum);
		break;
	}
}

/*
 * text_to_process: sends the given text to the given process.  If the given
 * process index is not valid, an error is reported and 1 is returned.
 * Otherwise 0 is returned. 
 * Added show, to remove some bad recursion, phone, april 1993
 */
int
text_to_process(int proc_index, u_char *text, int show)
{
	u_int	ref;
	Process	*proc;

	if (valid_process_index(proc_index) == 0)
	{
		say("No such process number %d", proc_index);
		return 1;
	}
	ref = process_list[proc_index]->refnum;
	proc = process_list[proc_index];
	message_to(ref);
	if (show)
		put_it("%s%s", get_prompt_by_refnum(ref), text); /* lynx */
	if (write(proc->p_stdin, text, my_strlen(text)) <= 0 ||
	    write(proc->p_stdin, "\n", 1) <= 0)
	{
		say("Write to process %d failed: %s", proc_index,
		    strerror(errno));
		return 1;
	}
	set_prompt_by_refnum(ref, empty_string());
	/*  update_input(UPDATE_ALL); */
	message_to(0);
	return 0;
}

/*
 * is_process_running: Given an index, this returns true if the index referes
 * to a currently running process, 0 otherwise 
 */
int
is_process_running(int proc_index)
{
	if (proc_index < 0 || proc_index >= process_list_size)
		return (0);
	if (process_list && process_list[proc_index])
		return (!process_list[proc_index]->exited);
	return (0);
}

/*
 * lofical_to_index: converts a logical process name to its approriate index
 * in the process list, or -1 if not found 
 */
int
logical_to_index(u_char *logical)
{
	Process	*proc;
	int	i;

	for (i = 0; i < process_list_size; i++)
	{
		if ((proc = process_list[i]) && proc->logical &&
		    (my_stricmp(proc->logical, logical) == 0))
			return i;
	}
	return -1;
}

/*
 * get_process_index: parses out a process index or logical name from the
 * given string 
 */
int
get_process_index(u_char **args)
{
	u_char	*s;

	if ((s = next_arg(*args, args)) != NULL)
	{
		if (*s == '%')
			s++;
		else
			return (-1);
		if (is_number(s))
			return (my_atoi(s));
		else
			return (logical_to_index(s));
	}
	else
		return (-1);
}

/* is_process: checks to see if arg is a valid process specification */
int
is_process(u_char *arg)
{
	if (arg && *arg == '%')
	{
		arg++;
		if (is_number(arg) || (logical_to_index(arg) != -1))
			return (1);
	}
	return (0);
}

/*
 * exec: the /EXEC command.  Handles the whole IRCII process crap. 
 */
void
execcmd(u_char *command, u_char *args, u_char *subargs)
{
	u_char	*who = NULL,
		*logical = NULL,
		*redirect, /* = NULL, */
		*flag,
		*cmd = NULL;
	unsigned int	refnum = 0;
	int	sig,
		i,
		refnum_flag = 0,
		logical_flag = 0;
	size_t	len;
	Process	*proc;

	if (get_int_var(EXEC_PROTECTION_VAR) && current_dcc_hook() != -1)
	{
		say("Attempt to use EXEC from within an ON function!");
		say("Command \"%s\" not executed!", args);
		say("Please read /HELP SET EXEC_PROTECTION");
		say("or important details about this!");
		return;
	}
#ifdef DAEMON_UID
	if (getuid() == DAEMON_UID)
	{
		say("You are not permitted to use EXEC.");
		return;
	}
#endif /* DAEMON_UID */
	if (*args == '\0')
	{
		list_processes();
		return;
	}
	redirect = NULL;
	while ((*args == '-') && (flag = next_arg(args, &args)))
	{
		if (*flag == '-')
		{
			len = my_strlen(++flag);
			malloc_strcpy(&cmd, flag);
			upper(cmd);
			if (my_strncmp(cmd, "OUT", len) == 0)
			{
				redirect = empty_string(); /* UP("PRIVMSG"); */
				if (!(who = get_channel_by_refnum(0)))
				{
					say("No current channel in this window for -OUT");
					new_free(&cmd);
					return;
				}
			}
			else if (my_strncmp(cmd, "TARGET", len) == 0)
			{
				redirect = UP("PRIVMSG");
				if (!(who = get_target_by_refnum(0)))
				{
					say("No current target in this window for -TARGET");
					new_free(&cmd);
					return;
				}
			}
			else if (my_strncmp(cmd, "NAME", len) == 0)
			{
				logical_flag = 1;
				if ((logical = next_arg(args, &args)) ==
						NULL)
				{
					say("You must specify a logical name");
					new_free(&cmd);
					return;
				}
			}
			else if (my_strncmp(cmd, "WINDOW", len) == 0)
			{
				refnum_flag = 1;
				refnum = current_refnum();
			}
			else if (my_strncmp(cmd, "MSG", len) == 0)
			{
				if (doing_privmsg())
					redirect = UP("NOTICE");
				else
					redirect = UP("PRIVMSG");
				if ((who = next_arg(args, &args)) ==
						NULL)
				{
					say("No nicknames specified");
					new_free(&cmd);
					return;
				}
			}
			else if (my_strncmp(cmd, "FILTER", len) == 0)
			{
				redirect = UP("FILTER");
				if ((who = next_arg(args, &args)) ==
						NULL)
				{
					say("No filter function specified");
					new_free(&cmd);
					return;
				}
			}
			else if (my_strncmp(cmd, "CLOSE", len) == 0)
			{
				if ((i = get_process_index(&args)) == -1)
				{
					say("Missing process number or logical name.");
					new_free(&cmd);
					return;
				}
				if (is_process_running(i))
				{
				    proc = process_list[i];
				    proc->p_stdin = exec_close(proc->p_stdin);
				    proc->p_stdout = exec_close(proc->p_stdout);
				    proc->p_stderr = exec_close(proc->p_stderr);
				}
				else
					say("No such process running!");
				new_free(&cmd);
				return;
			}
			else if (my_strncmp(cmd, "NOTICE", len) == 0)
			{
				redirect = UP("NOTICE");
				if ((who = next_arg(args, &args)) ==
						NULL)
				{
					say("No nicknames specified");
					new_free(&cmd);
					return;
				}
			}
			else if (my_strncmp(cmd, "IN", len) == 0)
			{
				if ((i = get_process_index(&args)) == -1)
				{
					say("Missing process number or logical name.");
					new_free(&cmd);
					return;
				}
				text_to_process(i, args, 1);
				new_free(&cmd);
				return;
			}
			else
			{
				u_char	*cmd2 = NULL;

				if ((i = get_process_index(&args)) == -1)
				{
					say("Invalid process specification");
					goto out;
				}
				if ((sig = my_atoi(flag)) > 0)
				{
					if ((sig > 0) && (sig < max_signo))
						kill_process(i, sig);
					else
						say("Signal number can be from 1 to %d", max_signo);
					goto out;
				}
				malloc_strcpy(&cmd2, flag);
				upper(cmd2);
				for (sig = 1; sig < max_signo && signals[sig]; sig++)
				{
					if (!my_strncmp(signals[sig], flag, len))
					{
						kill_process(i, sig);
						goto out2;
					}
				}
				say("No such signal: %s", flag);
out2:
				new_free(&cmd2);
out:
				new_free(&cmd);
				return;
			}
			new_free(&cmd);
		}
		else
			break;
	}
	if (is_process(args))
	{
		if ((i = get_process_index(&args)) == -1)
		{
			say("Invalid process specification");
			return;
		}
		if (valid_process_index(i))
		{
			proc = process_list[i];
			message_to(refnum);
			if (refnum_flag)
			{
				proc->refnum = refnum;
				if (refnum)
					say("Output from process %d (%s) now going to this window", i, proc->name);
				else
					say("Output from process %d (%s) not going to any window", i, proc->name);
			}
			malloc_strcpy(&(proc->redirect), redirect);
			malloc_strcpy(&(proc->who), who);
			if (redirect)
			{
				say("Output from process %d (%s) now going to %s", i, proc->name, who);
			}
			else
				say("Output from process %d (%s) now going to you", i, proc->name);
			if (logical_flag)
			{
				if (logical)
				{
					if (is_logical_unique(logical))
					{
						malloc_strcpy(&(
						    proc->logical),
						    logical);
						say("Process %d (%s) is now called %s",
							i, proc->name, proc->logical);
					}
					else
						say("The name %s is not unique!"
							, logical);
				}
				else
					say("The name for process %d (%s) has been removed", i, proc->name);
			}
			message_to(0);
		}
		else
			say("Invalid process specification");
	}
	else
	{
		if (is_logical_unique(logical))
			start_process(args, logical, redirect, who, refnum);
		else
			say("The name %s is not unique!", logical);
	}
}

/*
 * clean_up_processes: kills all processes in the procss list by first
 * sending a SIGTERM, then a SIGKILL to clean things up 
 */
void
clean_up_processes(void)
{
	int	i;

	if (process_list_size)
	{
		say("Cleaning up left over processes....");
		for (i = 0; i < process_list_size; i++)
		{
			if (process_list[i])
				kill_process(i, SIGTERM);
		}
		sleep(2);	/* Give them a chance for a graceful exit */
		for (i = 0; i < process_list_size; i++)
		{
			if (process_list[i])
				kill_process(i, SIGKILL);
		}
	}
}

/*
 * close_all_exec:  called when we fork of a wserv process for interaction
 * with screen/X, to close all unnessicary fd's that might cause problems
 * later.
 */
void
close_all_exec(void)
{
	int	i;
	unsigned display;

	display = set_display_off();
	for (i = 0; i < process_list_size; i++)
		if (process_list[i])
		{
			if (process_list[i]->p_stdin)
				new_close(process_list[i]->p_stdin);
			if (process_list[i]->p_stdout)
				new_close(process_list[i]->p_stdout);
			if (process_list[i]->p_stderr)
				new_close(process_list[i]->p_stderr);
			delete_process(i);
			kill_process(i, SIGKILL);
		}
	set_display(display);
}

void
exec_server_delete(int i)
{
	int	j;

	for (j = 0; j < process_list_size; j++)
		if (process_list[j] && process_list[j]->server >= i)
			process_list[j]->server--;
}

const u_char *
my_sigstr(int signo)
{
	if (signo <= max_signo) {
		return UP(signals[signo]);
	}
	return UP("(NOSIG)");
}
