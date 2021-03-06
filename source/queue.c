/*
 * queue.c - The queue command
 *  
 * Queues allow for future batch processing
 *
 * Syntax:  /QUEUE -DO -SHOW -LIST -NO_FLUSH -DELETE -FLUSH <name> {commands}
 *
 * Written by Jeremy Nelson
 *
 * Copyright (c) 1993 Jeremy Nelson.
 * Copyright (c) 1993-2014 Matthew R. Green.
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
IRCII_RCSID("@(#)$eterna: queue.c,v 1.30 2014/03/11 20:35:05 mrg Exp $");

#include "alias.h"
#include "ircaux.h"
#include "debug.h"
#include "output.h"
#include "edit.h"
#include "if.h"
#include "queue.h"

typedef	struct CmdListT
{
        struct CmdListT	*next;
        u_char		*what;
} CmdList;

typedef	struct	QueueT
{
        struct QueueT   *next;
        struct CmdListT *first;
        u_char     *name;
} Queue;

static	Queue	*lookup_queue(Queue *, u_char *);
static	CmdList	*walk_commands(Queue *);
static	Queue	*make_new_queue(Queue *, u_char *);
static	int	add_commands_to_queue(Queue *, u_char *what, u_char *);
static	int	delete_commands_from_queue(Queue *, int);
static	Queue	*remove_a_queue(Queue *);
static	void	flush_queue(Queue *);
static	Queue	*do_queue(Queue *, int);
static	void	display_all_queues(Queue *);
static	void	print_queue(Queue *);
static	int	num_entries(Queue *);

void 
queuecmd(u_char *cmd, u_char *args, u_char *subargs)
{
	Queue	*tmp;
	u_char	*arg = NULL,
		*name = NULL,
		*startcmds = NULL,
		*cmds = NULL;
	int	noflush = 0,
		runit = 0, 
		list = 0,
		flush = 0,
		remove_by_number = 0,
		commands = 1,
		number = 0;
	static	Queue	*Queuelist = NULL;
	
	/* If the queue list is empty, make an entry */
	if (Queuelist == NULL) {
		u_char *blah_c_sucks = UP("Top");

		Queuelist = make_new_queue(NULL, blah_c_sucks);
	}

	if ((startcmds = my_index(args, '{')) == NULL) /* } */
		commands = 0;
	else
		*(startcmds-1) = '\0';

	while ((arg = upper(next_arg(args, &args))) != NULL)
	{
		if (*arg == '-')
		{
			*arg++ = '\0';
			if (!my_strcmp(arg, "NO_FLUSH"))
				noflush = 1;
			else
			if (!my_strcmp(arg, "SHOW"))
			{
				display_all_queues(Queuelist);
				return;
			}
			else
			if (!my_strcmp(arg, "LIST"))
				list = 1;
			else
			if (!my_strcmp(arg, "DO"))
				runit = 1;
			else
			if (!my_strcmp(arg, "DELETE"))
				remove_by_number = 1;
			else
			if (!my_strcmp(arg, "FLUSH"))
				flush = 1;
		}
		else
		{
			if (name)
				number = my_atoi(arg);
			else
				name = arg;
		}
	}

	if (name == NULL)
		return;

	/* Find the queue based upon the previous queue */
	tmp = lookup_queue(Queuelist, name);

	/* if the next queue is empty, then we need to see if 
	   we should make it or output an error */
	if ((tmp->next) == NULL)
	{
		if (commands)
			tmp->next = make_new_queue(NULL, name);
		else
		{
			yell ("QUEUE: (%s) no such queue",name);
			return;
		}
	}
	if (remove_by_number == 1)
	{
		if (delete_commands_from_queue(tmp->next,number))
			tmp->next = remove_a_queue(tmp->next);
	}
	if (list == 1)
	{
		print_queue(tmp->next);
	}
	if (runit == 1)
	{
		tmp->next = do_queue(tmp->next, noflush);
	}
	if (flush == 1)
	{
		tmp->next = remove_a_queue(tmp->next);
	}
	if (startcmds)
	{
		int booya;

		if ((cmds = next_expr(&startcmds, '{')) == NULL) /* } */
		{
			yell ("QUEUE: missing closing brace");
			return;
		}
		booya = add_commands_to_queue (tmp->next, cmds, subargs);
		say ("QUEUED: %s now has %d entries",name, booya);
	}
}

/*
 * returns the queue BEFORE the queue we are looking for
 * returns the last queue if no match
 */
static	Queue	*
lookup_queue(Queue *queue, u_char *what)
{
	Queue	*tmp = queue;

	upper(what);

	while (tmp->next)
	{
		if (!my_strcmp(tmp->next->name, what))
			return tmp;
		else
			if (tmp->next)
				tmp = tmp->next;
			else
				break;
	}
	return tmp;
}

/* returns the last CmdList in a queue, useful for appending commands */
static	CmdList	*
walk_commands(Queue *queue)
{
	CmdList	*ctmp;
	
	if (!queue)
		return NULL;

	ctmp = queue->first;
	if (ctmp)
	{
		while (ctmp->next)
			ctmp = ctmp->next;
		return ctmp;
	}
	return NULL;
}

/*----------------------------------------------------------------*/
/* Make a new queue, link it in, and return it. */
static	Queue	*
make_new_queue(Queue *afterqueue, u_char *name)
{
	Queue	*tmp;

	if (!name)
		return NULL;
	tmp = new_malloc(sizeof *tmp);

	tmp->next = afterqueue;
	tmp->first = NULL;
	tmp->name = NULL;
	malloc_strcpy(&tmp->name, name);
	upper(tmp->name);
	return tmp;
}
	
/* add a command to a queue, at the end of the list */
/* expands the whole thing once and stores it */
static	int
add_commands_to_queue(Queue *queue, u_char *what, u_char *subargs)
{
	CmdList *ctmp = walk_commands(queue);
	u_char	*list = NULL,
		*sa;
	int 	args_flag = 0;
	
	sa = subargs ? subargs : (u_char *) " ";
	list = expand_alias(NULL, what, sa, &args_flag, NULL);
	if (!ctmp)
	{
		queue->first = new_malloc(sizeof(*queue->first));
		ctmp = queue->first;
	}
	else
	{
		ctmp->next = new_malloc(sizeof(*ctmp->next));
		ctmp = ctmp->next;
	}
 	ctmp->what = NULL;
	malloc_strcpy(&ctmp->what, list);
	ctmp->next = NULL;
	return num_entries(queue);
}


/* remove the Xth command from the queue */
static	int
delete_commands_from_queue(Queue *queue, int which)
{
	CmdList *ctmp = queue->first;
	CmdList *blah;
	int x;

	if (which == 1)
		queue->first = ctmp->next;
	else
	{
		for (x=1;x<which-1;x++)
		{
			if (ctmp->next) 
				ctmp = ctmp->next;
			else 
				return 0;
		}
		blah = ctmp->next;
		ctmp->next = ctmp->next->next;
		ctmp = blah;
	}
	new_free(&ctmp->what);
	new_free(&ctmp);
	if (queue->first == NULL)
		return 1;
	else
		return 0;
}

/*-------------------------------------------------------------------*/
/* flush a queue, deallocate the memory, and return the next in line */
static	Queue	*
remove_a_queue(Queue *queue)
{
	Queue *tmp;

	tmp = queue->next;
	flush_queue(queue);
	new_free(&queue);
	return tmp;
}

/* walk through a queue, deallocating the entries */
static	void
flush_queue(Queue *queue)
{
	CmdList	*tmp,
		*tmp2;

	tmp = queue->first;

	while (tmp != NULL)
	{
		tmp2 = tmp;
		tmp = tmp2->next;
		if (tmp2->what != NULL)
			new_free(&tmp2->what);
		if (tmp2)
			new_free(&tmp2);
	}
}

/*------------------------------------------------------------------------*/
/* run the queue, and if noflush, then return the queue, else return the
   next queue */
static	Queue	*
do_queue(Queue *queue, int noflush)
{
	CmdList	*tmp;
	
	tmp = queue->first;
	
	do
	{
		if (tmp->what != NULL)
			parse_line(NULL, tmp->what, empty_string(), 0, 0, 0);
		tmp = tmp->next;
	}
	while (tmp != NULL);

	if (!noflush) 
		return remove_a_queue(queue);
	else
		return queue;
}

/* ---------------------------------------------------------------------- */
/* output the contents of all the queues to the screen */
static	void
display_all_queues(Queue *queue)
{
	Queue *tmp;

	if (!queue)
		return;

	tmp = queue->next;
	while (tmp)
	{
		print_queue(tmp);
		if (tmp->next == NULL)
			return;
		else
			tmp = tmp->next;
	}
	yell("QUEUE: No more queues");
}

/* output the contents of a queue to the screen */
static	void
print_queue(Queue *queue)
{
	CmdList *tmp;
	int 	x = 0;
	
	tmp = queue->first;
	while (tmp != NULL)
	{
		if (tmp->what)
			say ("<%s:%2d> %s",queue->name,++x,tmp->what);
		tmp = tmp->next;
	}
	say ("<%s> End of queue",queue->name);
}

/* return the number of entries in a queue */
static	int
num_entries(Queue *queue)
{
	int x = 1;
	CmdList *tmp;

	if ((tmp = queue->first) == NULL) 
		return 0;
	while (tmp->next)
	{
		x++;
		tmp = tmp->next;
	}
	return x;
}
