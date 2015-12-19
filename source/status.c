/*
 * status.c: handles the status line updating, etc for IRCII 
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

/*
 * WARNING!  THIS CODE HAS DRAGONS.  BE *VERY* CAREFUL WHEN CHANGING
 * ANYTHING IN HERE.  TEST IT EXTENSIVELY.
 */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: status.c,v 1.121 2015/09/05 07:37:40 mrg Exp $");

#include "ircterm.h"
#include "status.h"
#include "server.h"
#include "vars.h"
#include "hook.h"
#include "input.h"
#include "edit.h"
#include "window.h"
#include "screen.h"
#include "mail.h"
#include "output.h"
#include "names.h"
#include "ircaux.h"
#include "translat.h"
#include "debug.h"

static	u_char	*convert_format(u_char *, int);
static	u_char	*status_nickname(Window *);
static	u_char	*status_query_nick(Window *);
static	u_char	*status_right_justify(Window *);
static	u_char	*status_chanop(Window *);
static	u_char	*status_channel(Window *);
static	u_char	*status_server(Window *);
static	u_char	*status_mode(Window *);
static	u_char	*status_umode(Window *);
static	u_char	*status_insert_mode(Window *);
static	u_char	*status_overwrite_mode(Window *);
static	u_char	*status_away(Window *);
static	u_char	*status_oper(Window *);
static	u_char	*status_voice(Window *);
static	u_char	*status_user0(Window *);
static	u_char	*status_user1(Window *);
static	u_char	*status_user2(Window *);
static	u_char	*status_user3(Window *);
static	u_char	*status_hold(Window *);
static	u_char	*status_version(Window *);
static	u_char	*status_clock(Window *);
static	u_char	*status_hold_lines(Window *);
static	u_char	*status_window(Window *);
static	u_char	*status_mail(Window *);
static	u_char	*status_refnum(Window *);
static	u_char	*status_null_function(Window *);
static	u_char	*status_notify_windows(Window *);
static	u_char	*status_group(Window *);
static	u_char	*status_scrolled(Window *);
static	u_char	*status_scrolled_lines(Window *);
static	void	alarm_switch(int);
static	u_char	*convert_sub_format(u_char *, int);
static	void	make_status_one(Window *, int, int);

/*
 * Maximum number of "%" expressions in a status line format.  If you change
 * this number, you must manually change the snprintf() in make_status 
 */
#define MAX_FUNCTIONS 45

/* The format statements to build each portion of the status line */
static	u_char	*mode_format = NULL;
static	u_char	*umode_format = NULL;
static	u_char	*status_format[3] = {NULL, NULL, NULL,};
static	u_char	*query_format = NULL;
static	u_char	*clock_format = NULL;
static	u_char	*hold_lines_format = NULL;
static	u_char	*scrolled_lines_format = NULL;
static	u_char	*channel_format = NULL;
static	u_char	*mail_format = NULL;
static	u_char	*server_format = NULL;
static	u_char	*notify_format = NULL;
static	u_char	*group_format = NULL;

/*
 * status_func: The list of status line function in the proper order for
 * display.  This list is set in convert_format() 
 */
static	u_char	*(*status_func[3][MAX_FUNCTIONS])(Window *);

/* func_cnt: the number of status line functions assigned */
static	int	func_cnt[3];

static	int	alarm_hours,	/* hour setting for alarm in 24 hour time */
		alarm_minutes;	/* minute setting for alarm */

/* Stuff for the alarm */
static	struct itimerval clock_timer = { { 10L, 0L }, { 1L, 0L } };
static	struct itimerval off_timer = { { 0L, 0L }, { 0L, 0L } };

static	void	alarmed(int);
static	int	do_status_alarmed;

/* alarmed: This is called whenever a SIGALRM is received and the alarm is on */
static	void
alarmed(int signo)
{
	do_status_alarmed = 1;
}

void
check_status_alarmed(void)
{
	if (do_status_alarmed) {
		u_char	time_str[16];

		say("The time is %s", update_clock(time_str, 16, GET_TIME));
		term_beep();
		term_beep();
		term_beep();
		do_status_alarmed = 0;
	}
}

/*
 * alarm_switch: turns on and off the alarm display.  Sets the system timer
 * and sets up a signal to trap SIGALRMs.  If flag is 1, the alarmed()
 * routine will be activated every 10 seconds or so.  If flag is 0, the timer
 * and signal stuff are reset 
 */
static	void
alarm_switch(int flag)
{
	static	int	alarm_on = 0;

	if (flag)
	{
		if (!alarm_on)
		{
			(void) MY_SIGNAL(SIGALRM, alarmed, 0);
			setitimer(ITIMER_REAL, &clock_timer,
				NULL);
			alarm_on = 1;
		}
	}
	else if (alarm_on)
	{
		setitimer(ITIMER_REAL, &off_timer, NULL);
		(void) MY_SIGNAL(SIGALRM, (sigfunc *)SIG_IGN, 0);
		alarm_on = 0;
	}
}

/*
 * set_alarm: given an input string, this checks its validity as a clock
 * type time thingy.  It accepts two time formats.  The first is the HH:MM:XM
 * format where HH is between 1 and 12, MM is between 0 and 59, and XM is
 * either AM or PM.  The second is the HH:MM format where HH is between 0 and
 * 23 and MM is between 0 and 59.  This routine also looks for one special
 * case, "OFF", which sets the alarm string to null 
 */
void
set_alarm(u_char *str)
{
	u_char	hours[10],
		minutes[10],
		merid[3];
	u_char	time_str[10];
	int	c,
		h,
		m,
		min_hours,
		max_hours;

	if (str == NULL)
	{
		alarm_switch(0);
		return;
	}
	if (!my_stricmp(str, UP(var_settings(OFF))))
	{
		set_string_var(CLOCK_ALARM_VAR, NULL);
		alarm_switch(0);
		return;
	}
	
	c = sscanf(CP(str), " %2[^:]:%2[^paPA]%2s ", hours, minutes, merid);
	switch (c)
	{
	case 2:
		min_hours = 0;
		max_hours = 23;
		break;
	case 3:
		min_hours = 1;
		max_hours = 12;
		upper(UP(merid));
		break;
	default:
		say("CLOCK_ALARM: Bad time format.");
		set_string_var(CLOCK_ALARM_VAR, NULL);
		return;
	}
	
	h = my_atoi(hours);
	m = my_atoi(minutes);
	if (h >= min_hours && h <= max_hours && isdigit(hours[0]) &&
		(isdigit(hours[1]) || hours[1] == (u_char) 0))
	{
		if (m >= 0 && m <= 59 && isdigit(minutes[0]) &&
				isdigit(minutes[1]))
		{
			alarm_minutes = m;
			alarm_hours = h;
			if (max_hours == 12)
			{
				if (merid[0] != 'A')
				{
					if (merid[0] == 'P')
					{
						if (h != 12)
							alarm_hours += 12;
					}
					else
					{
	say("CLOCK_ALARM: alarm time must end with either \"AM\" or \"PM\"");
	set_string_var(CLOCK_ALARM_VAR, NULL);
					}
				}
				else
				{
					if (h == 12)
						alarm_hours = 0;
				}
				if (merid[1] == 'M')
				{
					snprintf(CP(time_str), sizeof time_str,
					    "%02d:%02d%s", h, m, merid);
					set_string_var(CLOCK_ALARM_VAR,
						time_str);
				}
				else
				{
	say("CLOCK_ALARM: alarm time must end with either \"AM\" or \"PM\"");
	set_string_var(CLOCK_ALARM_VAR, NULL);
				}
			}
			else
			{
				snprintf(CP(time_str), sizeof time_str,
				    "%02d:%02d", h, m);
				set_string_var(CLOCK_ALARM_VAR, time_str);
			}
		}
		else
		{
	say("CLOCK_ALARM: alarm minutes value must be between 0 and 59.");
	set_string_var(CLOCK_ALARM_VAR, NULL);
		}
	}
	else
	{
		say("CLOCK_ALARM: alarm hour value must be between %d and %d.",
			min_hours, max_hours);
		set_string_var(CLOCK_ALARM_VAR, NULL);
	}
}

u_char	*
format_clock(u_char *buf, size_t len, int hour, int min)
{
	char	*merid;

	if (get_int_var(CLOCK_24HOUR_VAR))
		merid = CP(empty_string());
	else
	{
		if (hour < 12)
			merid = "AM";
		else
			merid = "PM";
		if (hour > 12)
			hour -= 12;
		else if (hour == 0)
			hour = 12;
	}
	snprintf(CP(buf), len, "%02d:%02d%s", hour, min, merid);

	return buf;
}


/* update_clock: figures out the current time and returns it in a nice format */
u_char	*
update_clock(u_char *buf, size_t len, int flag)
{
	struct tm	*time_val;
	static	u_char	time_str[10];
	static	int	min = -1, hour = -1;
	time_t	t;
	int	tmp_hour, tmp_min;

	t = time(0);
	time_val = localtime(&t);
	tmp_hour = time_val->tm_hour;
	tmp_min = time_val->tm_min;

	Debug(DB_STATUS, "update_clock (%s): time %lu (%02d:%02d) [old %02d:%02d]",
	    flag == RESET_TIME ? "reset" :
	     flag == GET_TIME ? "get" :
	     flag == UPDATE_TIME ? "update" : "unknown", 
	    (long unsigned) t, tmp_hour, tmp_min, hour, min);

	if (get_string_var(CLOCK_ALARM_VAR))
	{
		if ((tmp_hour == alarm_hours) && (tmp_min == alarm_minutes))
			alarm_switch(1);
		else
			alarm_switch(0);
	}

	if (flag == RESET_TIME ||
	    (flag == UPDATE_TIME && (tmp_min != min || tmp_hour != hour)))
	{
		int server;

		format_clock(time_str, sizeof time_str, tmp_hour, tmp_min);
		server = set_from_server(get_primary_server());
		Debug(DB_STATUS, "update_clock: in reset_time block, time_str: %s",
		       time_str);
		if (flag == UPDATE_TIME && (tmp_min != min || tmp_hour != hour))
		{
			hour = tmp_hour;
			min = tmp_min;
			do_hook(TIMER_LIST, "%s", time_str);
		}
		do_hook(IDLE_LIST, "%ld", (t - idle_time()) / 60L);
		set_from_server(server);
		flag = GET_TIME;
	}
	if (buf)
	{
		my_strncpy(buf, time_str, len - 1);
		buf[len - 1] = '\0';
	}
	if (flag == GET_TIME)
		return(buf ? buf : time_str);
	else
		return (NULL);
}

void
reset_clock(int unused)
{
	update_clock(0, 0, RESET_TIME);
	update_all_status();
}

/*
 * convert_sub_format: This is used to convert the formats of the
 * sub-portions of the status line to a format statement specially designed
 * for that sub-portions.  convert_sub_format looks for a single occurence of
 * %c (where c is passed to the function). When found, it is replaced by "%s"
 * for use is a snprintf.  All other occurences of % followed by any other
 * character are left unchanged.  Only the first occurence of %c is
 * converted, all subsequence occurences are left unchanged.  This routine
 * mallocs the returned string. 
 */
static	u_char	*
convert_sub_format(u_char *format, int c)
{
	u_char	lbuf[BIG_BUFFER_SIZE];
	static	u_char	bletch[] = "%% ";
	u_char	*ptr = NULL;
	int	dont_got_it = 1;

	if (format == NULL)
		return (NULL);
	*lbuf = (u_char) 0;
	while (format)
	{
		if ((ptr = my_index(format, '%')) != NULL)
		{
			*ptr = (u_char) 0;
			my_strmcat(lbuf, format, sizeof lbuf);
			*(ptr++) = '%';
			if ((*ptr == c) && dont_got_it)
			{
				dont_got_it = 0;
				my_strmcat(lbuf, "%s", sizeof lbuf);
			}
			else
			{
				bletch[2] = *ptr;
				my_strmcat(lbuf, bletch, sizeof lbuf);
			}
			ptr++;
		}
		else
			my_strmcat(lbuf, format, sizeof lbuf);
		format = ptr;
	}
	malloc_strcpy(&ptr, lbuf);
	return (ptr);
}

static	u_char	*
convert_format(u_char *format, int k)
{
	u_char	lbuf[BIG_BUFFER_SIZE];
	u_char	*ptr,
		*malloc_ptr = NULL; 
	int	*cp;
	
	*lbuf = (u_char) 0;
	while (format)
	{
		if ((ptr = my_index(format, '%')) != NULL)
		{
			*ptr = (u_char) 0;
			my_strmcat(lbuf, format, sizeof lbuf);
			*(ptr++) = '%';
			cp = &func_cnt[k];
			if (*cp < MAX_FUNCTIONS)
			{
				switch (*(ptr++))
				{
				case '%':
					/*
					 * %% instead of %, because this will
					 * be passed to snprintf
					 */
					my_strmcat(lbuf, "%%", sizeof lbuf);
					break;
				case 'N':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_nickname;
					break;
				case '>':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_right_justify;
					break;
				case 'G':
					new_free(&group_format);
					group_format =
		convert_sub_format(get_string_var(STATUS_GROUP_VAR), 'G');
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_group;
					break;
				case 'Q':
					new_free(&query_format);
					query_format =
		convert_sub_format(get_string_var(STATUS_QUERY_VAR), 'Q');
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_query_nick;
					break;
				case 'F':
					new_free(&notify_format);
					notify_format = 
		convert_sub_format(get_string_var(STATUS_NOTIFY_VAR), 'F');
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_notify_windows;
					break;
				case '@':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_chanop;
					break;
				case 'C':
					new_free(&channel_format);
					channel_format =
		convert_sub_format(get_string_var(STATUS_CHANNEL_VAR), 'C');
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_channel;
					break;
				case 'S':
					new_free(&server_format);
					server_format =
		convert_sub_format(get_string_var(STATUS_SERVER_VAR), 'S');
					my_strmcat(lbuf,"%s",sizeof lbuf);
					status_func[k][(*cp)++] =
						status_server;
					break;
				case '+':
					new_free(&mode_format);
					mode_format =
		convert_sub_format(get_string_var(STATUS_MODE_VAR), '+');
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_mode;
					break;
				case '#':
					new_free(&umode_format);
					umode_format =
		convert_sub_format(get_string_var(STATUS_UMODE_VAR), '#');
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_umode;
					break;
				case 'M':
					new_free(&mail_format);
					mail_format =
		convert_sub_format(get_string_var(STATUS_MAIL_VAR), 'M');
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_mail;
					break;
				case 'I':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_insert_mode;
					break;
				case 'O':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_overwrite_mode;
					break;
				case 'A':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_away;
					break;
				case 'V':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_version;
					break;
				case 'R':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_refnum;
					break;
				case 'T':
					new_free(&clock_format);
					clock_format =
		convert_sub_format(get_string_var(STATUS_CLOCK_VAR), 'T');
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_clock;
					break;
				case 'U':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_user0;
					break;
				case 'H':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_hold;
					break;
				case 'B':
					new_free(&hold_lines_format);
					hold_lines_format =
		convert_sub_format(get_string_var(STATUS_HOLD_LINES_VAR), 'B');
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_hold_lines;
					break;
				case 'P':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_scrolled;
					Debug(DB_STATUS, "got P in status");
					break;
				case 's':
					new_free(&scrolled_lines_format);
					scrolled_lines_format =
		convert_sub_format(get_string_var(STATUS_SCROLLED_LINES_VAR), 's');
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_scrolled_lines;
					Debug(DB_STATUS, "got s status: slformat '%s'",
					       scrolled_lines_format);
					break;
				case '*':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_oper;
					break;
				case 'v':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_voice;
					break;
				case 'W':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_window;
					break;
				case 'X':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_user1;
					break;
				case 'Y':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_user2;
					break;
				case 'Z':
					my_strmcat(lbuf, "%s", sizeof lbuf);
					status_func[k][(*cp)++] =
						status_user3;
					break;
		/* no default..?? - phone, jan 1993 */
		/* empty is a good default -lynx, mar 93 */
				}
			}
			else
				ptr++;
		}
		else
			my_strmcat(lbuf, format, sizeof lbuf);
		format = ptr;
	}
	/* this frees the old str first */
	malloc_strcpy(&malloc_ptr, lbuf);
	return (malloc_ptr);
}

void
build_status(u_char *format)
{
	int	i, k;

	for (k = 0; k < 3; k++) 
	{
		new_free(&status_format[k]);
		func_cnt[k] = 0;
		switch (k)
		{
		case 0 : 
			format = get_string_var(STATUS_FORMAT_VAR);
			break;
			
		case 1 : 
			format = get_string_var(STATUS_FORMAT1_VAR);
			break;
			
		case 2 : 
			format = get_string_var(STATUS_FORMAT2_VAR);
			break;
		}
		if (format != NULL)	/* convert_format mallocs for us */
			status_format[k] = convert_format(format, k);
		for (i = func_cnt[k]; i < MAX_FUNCTIONS; i++)
			status_func[k][i] = status_null_function;
	}
	update_all_status();
}

void
make_status(Window *window)
{
	int	k, l, final;

	switch (window_get_double_status(window)) {
	case -1:
		window_set_status_line(window, 0, NULL);
		window_set_status_line(window, 1, NULL);
		goto out;
	case 0:
		window_set_status_line(window, 1, NULL);
		final = 1;
		break;
	case 1:
		final = 2;
		break;
	default:
		yell("--- make_status: unknown window->double value %d",
		     window_get_double_status(window));
		final = 1;
	}
	for (k = 0 ; k < final; k++)
	{
		if (k)
			l = 2;
		else if (window_get_double_status(window))
			l = 1;
		else
			l = 0;
			
		if (!term_basic() && status_format[l])
			make_status_one(window, k, l);
	}
out:
	cursor_to_input();
}

static	void
make_status_one(Window *window, int k, int l)
{
	Screen	*old_current_screen;
	u_char	lbuf[BIG_BUFFER_SIZE];
	u_char	rbuf[BIG_BUFFER_SIZE];
	u_char	*func_value[MAX_FUNCTIONS];
	size_t	len;
	int	i;
	int rjustifypos;
	int justifypadlen;
	int RealPosition;

	/*
	 * XXX: note that this code below depends on the definition
	 * of MAX_FUNCTIONS (currently 45), and the snprintf must
	 * be updated if MAX_FUNCTIONS is changed.
	 */
	for (i = 0; i < MAX_FUNCTIONS; i++)
		func_value[i] = (status_func[l][i]) (window);
	lbuf[0] = REV_TOG;
	snprintf(CP(lbuf+1),
	       sizeof(lbuf) - 1,
	       CP(status_format[l]),
		func_value[0], func_value[1], func_value[2],
		func_value[3], func_value[4], func_value[5],
		func_value[6], func_value[7], func_value[8],
		func_value[9], func_value[10], func_value[11],
		func_value[12], func_value[13], func_value[14],
		func_value[15], func_value[16], func_value[17],
		func_value[18], func_value[19], func_value[20],
		func_value[21], func_value[22], func_value[23],
		func_value[24], func_value[25], func_value[26],
		func_value[27], func_value[28], func_value[29],
		func_value[30], func_value[31], func_value[32],
		func_value[33], func_value[34], func_value[35],
		func_value[36], func_value[37], func_value[38],
		func_value[39], func_value[40], func_value[41],
		func_value[42], func_value[43], func_value[44]);
	for (i = 0; i < MAX_FUNCTIONS; i++)
		new_free(&(func_value[i]));
	
	/*  Patched 26-Mar-93 by Aiken
	 *  make_window now right-justifies everything 
	 *  after a %>
	 *  it's also more efficient.
	 */
	
	rjustifypos   = -1;
	justifypadlen = 0;
	for (i = 0; lbuf[i]; i++)
	{
		/* formfeed is a marker for left/right border*/
		if (lbuf[i] == '\f')
		{
			int len_left;
			int len_right;
			
			/* Split the line to left and right part */
			lbuf[i] = '\0';
			
			/*
			 * Get lengths of left and right part in number of
			 * columns
			 */
			len_right = my_strlen_c(lbuf);
			len_left  = my_strlen_c(lbuf+i+1);
			
			justifypadlen = get_co() - len_right - len_left;
			
			if (justifypadlen < 0)
				justifypadlen = 0;
			
			/* Delete the marker */
			/* FIXME: strcpy may not be used for overlapping buffers */
			my_strcpy(lbuf+i, lbuf+i+1);
			
			rjustifypos = i;
		}
	}
	
	if (get_int_var(FULL_STATUS_LINE_VAR))
	{
		if (rjustifypos == -1)
		{
			int length    = my_strlen_c(lbuf);

			justifypadlen = get_co() - length;
			if (justifypadlen < 0)
				justifypadlen = 0;
			rjustifypos = my_strlen(lbuf);
		}
		if (justifypadlen > 0)
		{
			/* Move a part of the data out of way */
			memmove(lbuf + rjustifypos + justifypadlen,
				lbuf + rjustifypos,
				my_strlen(lbuf) - rjustifypos + 1);
					// +1 = zero terminator
			
			/* Then fill the part with spaces */
			memset(lbuf + rjustifypos,
				   ' ',
				   justifypadlen);
		}
	}
	
	len = my_strlen(lbuf);
	if (len > (sizeof(lbuf) - 2))
		len = sizeof(lbuf) - 2;
	lbuf[len] = ALL_OFF;
	lbuf[len+1] =  '\0';
	
	my_strcpy_ci(rbuf, sizeof rbuf, lbuf);
	
	RealPosition = 0;
	i = 0;
	
	old_current_screen = get_current_screen();
	set_current_screen(window_get_screen(window));
	term_move_cursor(RealPosition, window_get_bottom(window) + k);
	len = output_line(rbuf, i);
	cursor_in_display();
#ifdef TERM_USE_LAST_COLUMN
	/*
	 * If ignoring the last column, always erase it, but
	 * if it is in use, only do so if we haven't writtten
	 * there.
	 */
	if (len < get_co() && term_clear_to_eol())
#else
	if (term_clear_to_eol() && len < get_co())
#endif
		term_space_erase(get_co() - len);
	window_set_status_line(window, k, rbuf);
	set_current_screen(old_current_screen);
}

static	u_char	*
status_nickname(Window *window)
{
	u_char	*ptr = NULL;

	if (connected_to_server() == 1 && !get_int_var(SHOW_STATUS_ALL_VAR) &&
	    !window_get_current_channel(window) && !window_is_current(window))
		malloc_strcpy(&ptr, empty_string());
	else
		malloc_strcpy(&ptr,
			server_get_nickname(window_get_server(window)));
	return (ptr);
}

static	u_char	*
status_server(Window *window)
{
	u_char	*ptr = NULL,
		*rest,
		*name;
	u_char	lbuf[BIG_BUFFER_SIZE];

	if (connected_to_server() != 1)
	{
		int	server = window_get_server(window);

		if (server != -1)
		{
			if (server_format)
			{
				name = server_get_name(server);
				rest = my_index(name, '.');
				if (rest != NULL &&
				    my_strnicmp(name, UP("irc"), 3) != 0 &&
				    my_strnicmp(name, UP("icb"), 3) != 0)
				{
					if (is_number(name))
						snprintf(CP(lbuf), sizeof lbuf,
							CP(server_format),
							name);
					else
					{
						*rest = '\0';
						snprintf(CP(lbuf), sizeof lbuf,
							CP(server_format),
							name);
						*rest = '.';
					}
				}
				else
					snprintf(CP(lbuf), sizeof lbuf,
						CP(server_format), name);
			}
			else
				*lbuf = '\0';
		}
		else
			my_strcpy(lbuf, " No Server");
	}
	else
		*lbuf = '\0';
	malloc_strcpy(&ptr, lbuf);
	return (ptr);
}

static	u_char	*
status_group(Window *window)
{
	u_char	*ptr = NULL;
	int	group;

	group = window_get_server_group(window);
	if (group && group_format)
	{
		u_char	lbuf[BIG_BUFFER_SIZE];

		snprintf(CP(lbuf), sizeof lbuf, CP(group_format),
			find_server_group_name(group));
		malloc_strcpy(&ptr, lbuf);
	}
	else
		malloc_strcpy(&ptr, empty_string());
	return (ptr);
}

static	u_char	*
status_query_nick(Window *window)
{
	u_char	*ptr = NULL;

	if (window_get_query_nick(window) && query_format)
	{
		u_char	lbuf[BIG_BUFFER_SIZE];

		snprintf(CP(lbuf), sizeof lbuf, CP(query_format),
			window_get_query_nick(window));
		malloc_strcpy(&ptr, lbuf);
	}
	else
		malloc_strcpy(&ptr, empty_string());
	return (ptr);
}

static	u_char	*
status_right_justify(Window *window)
{
	u_char	*ptr = NULL;

	malloc_strcpy(&ptr, UP("\f"));
	return (ptr);
}

static	u_char	*
status_notify_windows(Window *window)
{
	u_char	refnum[10];
	int	doneone = 0;
	u_char	*ptr = NULL;
	u_char	buf2[81];

	if (get_int_var(SHOW_STATUS_ALL_VAR) || window_is_current(window))
	{
		Win_Trav wt;

		buf2[0] = '\0';
		wt.init = 1;
		while ((window = window_traverse(&wt)) != NULL)
		{
			if (window_get_miscflags(window) & WINDOW_NOTIFIED)
			{
				if (!doneone)
				{
					doneone++;
					snprintf(CP(refnum), sizeof refnum,
						"%d", window_get_refnum(window));
				}
				else
					snprintf(CP(refnum), sizeof refnum,
						",%d", window_get_refnum(window));
				my_strmcat(buf2, refnum, sizeof buf2);
			}
		}
	}
	if (doneone && notify_format)
	{
		u_char	lbuf[BIG_BUFFER_SIZE];

		snprintf(CP(lbuf), sizeof lbuf, CP(notify_format), buf2);
		malloc_strcpy(&ptr, lbuf);
	}
	else
		malloc_strcpy(&ptr, empty_string());
	return ptr;
}

static	u_char	*
status_clock(Window *window)
{
	u_char	*ptr = NULL;

	if (get_int_var(CLOCK_VAR) && clock_format &&
	    (get_int_var(SHOW_STATUS_ALL_VAR) || window_is_current(window)))
	{
		u_char	lbuf[BIG_BUFFER_SIZE];
		u_char	time_str[16];

		snprintf(CP(lbuf), sizeof lbuf, CP(clock_format),
			update_clock(time_str, sizeof time_str, GET_TIME));
		malloc_strcpy(&ptr, lbuf);
	}
	else
		malloc_strcpy(&ptr, empty_string());
	return (ptr);
}

static	u_char	*
status_mode(Window *window)
{
	u_char	*ptr = NULL;
	u_char	*channel = window_get_current_channel(window);
	int	server = window_get_server(window);

	if (channel && chan_is_connected(channel, server))
	{
		u_char	*mode;

		mode = get_channel_mode(channel, server);
		if (mode && *mode && mode_format)
		{
			u_char	lbuf[BIG_BUFFER_SIZE];

			snprintf(CP(lbuf), sizeof lbuf, CP(mode_format), mode);
			malloc_strcpy(&ptr, lbuf);
			return (ptr);
		}
	}
	malloc_strcpy(&ptr, empty_string());
	return (ptr);
}


static	u_char	*
status_umode(Window *window)
{
	u_char	*ptr = NULL;
	u_char	localbuf[10];
	u_char	*c;
	int	server = window_get_server(window);

	if (connected_to_server() == 0)
		malloc_strcpy(&ptr, empty_string());
	else if (connected_to_server() == 1 &&
		 !get_int_var(SHOW_STATUS_ALL_VAR) &&
		 !window_is_current(window))
		malloc_strcpy(&ptr, empty_string());
	else
	{
		c = localbuf;
		if (server_get_flag(server, USER_MODE_I))
			*c++ = 'i';
		if (server_get_operator(server))
			*c++ = 'o';
		if (server_get_flag(server, USER_MODE_R))
			*c++ = 'r';
		if (server_get_flag(server, USER_MODE_S))
			*c++ = 's';
		if (server_get_flag(server, USER_MODE_W))
			*c++ = 'w';
		if (server_get_flag(server, USER_MODE_Z))
			*c++ = 'z';
		*c++ = '\0';
		if (*localbuf != '\0' && umode_format)
		{
			u_char	lbuf[BIG_BUFFER_SIZE];

			snprintf(CP(lbuf), sizeof lbuf, CP(umode_format),
				 localbuf);
			malloc_strcpy(&ptr, lbuf);
		}
		else
			malloc_strcpy(&ptr, empty_string());
	}
	return (ptr);
}

static	u_char	*
status_chanop(Window *window)
{
	u_char	*ptr = NULL,
		*text;
	u_char	*channel = window_get_current_channel(window);
	int	server = window_get_server(window);

	if (channel &&
	    chan_is_connected(channel, server) &&
	    get_channel_oper(channel, server) &&
	    (text = get_string_var(STATUS_CHANOP_VAR)))
		malloc_strcpy(&ptr, text);
	else
		malloc_strcpy(&ptr, empty_string());
	return (ptr);
}


static	u_char	*
status_hold_lines(Window *window)
{
	u_char	*ptr = NULL;
	int	num;
	u_char	localbuf[40];
	int	lines = window_held_lines(window);

	num = lines - lines % 10;
	if (num)
	{
		u_char	lbuf[BIG_BUFFER_SIZE];

		snprintf(CP(localbuf), sizeof localbuf, "%d", num);
		snprintf(CP(lbuf), sizeof lbuf, CP(hold_lines_format),
			 localbuf);
		malloc_strcpy(&ptr, lbuf);
	}
	else
		malloc_strcpy(&ptr, empty_string());
	return (ptr);
}

static	u_char	*
status_scrolled(Window *window)
{
	u_char	*ptr = NULL,
		*text;
	int	lines = window_get_all_scrolled_lines(window);

	Debug(DB_STATUS, "status_scrolled: lines = %d", lines);
	if (lines && (text = get_string_var(STATUS_SCROLLED_VAR)))
		malloc_strcpy(&ptr, text);
	else
		malloc_strcpy(&ptr, empty_string());
	return (ptr);
}

static	u_char	*
status_scrolled_lines(Window *window)
{
	u_char	*ptr = NULL;
	u_char	localbuf[40];
	int	lines = window_get_all_scrolled_lines(window);

	if (lines)
	{
		u_char	lbuf[BIG_BUFFER_SIZE];

		snprintf(CP(localbuf), sizeof localbuf, "%d", lines);
		snprintf(CP(lbuf), sizeof lbuf, CP(hold_lines_format),
			 localbuf);
		malloc_strcpy(&ptr, lbuf);
	}
	else
		malloc_strcpy(&ptr, empty_string());
	Debug(DB_STATUS, "status_scrolled_lines: lines = %d, str = '%s'", lines, ptr);
	return (ptr);
}

static	u_char	*
status_channel(Window *window)
{
	int	num;
	u_char	*s, *ptr,
		channel[IRCD_BUFFER_SIZE + 1];
	int	server = window_get_server(window);

	s = window_get_current_channel(window);
	if (s && chan_is_connected(s, server))
	{
		u_char	lbuf[BIG_BUFFER_SIZE];

		if (get_int_var(HIDE_PRIVATE_CHANNELS_VAR) &&
		    is_channel_mode(window_get_current_channel(window),
				MODE_PRIVATE | MODE_SECRET, server))
			ptr = UP("*private*");
		else
			ptr = window_get_current_channel(window);
		strmcpy(channel, ptr, IRCD_BUFFER_SIZE);
		if ((num = get_int_var(CHANNEL_NAME_WIDTH_VAR)) &&
		    ((int) my_strlen(channel) > num))
			channel[num] = (u_char) 0;
		/* num = my_atoi(channel); */
		ptr = NULL;
		snprintf(CP(lbuf), sizeof lbuf, CP(channel_format), channel);
		malloc_strcpy(&ptr, lbuf);
	}
	else
	{
		ptr = NULL;
		malloc_strcpy(&ptr, empty_string());
	}
	return (ptr);
}

static	u_char	*
status_mail(Window *window)
{
	u_char	*ptr = NULL,
		*number;

	if (get_int_var(MAIL_VAR) && (number = check_mail()) && mail_format &&
	    (get_int_var(SHOW_STATUS_ALL_VAR) || window_is_current(window)))
	{
		u_char	lbuf[BIG_BUFFER_SIZE];

		snprintf(CP(lbuf), sizeof lbuf, CP(mail_format), number);
		malloc_strcpy(&ptr, lbuf);
	}
	else
		malloc_strcpy(&ptr, empty_string());
	return (ptr);
}

static	u_char	*
status_insert_mode(Window *window)
{
	u_char	*ptr = NULL,
		*text;

	text = empty_string();
	if (get_int_var(INSERT_MODE_VAR) &&
	    (get_int_var(SHOW_STATUS_ALL_VAR) || window_is_current(window)))
	{
		if ((text = get_string_var(STATUS_INSERT_VAR)) == NULL)
			text = empty_string();
	}
	malloc_strcpy(&ptr, text);
	return (ptr);
}

static	u_char	*
status_overwrite_mode(Window *window)
{
	u_char	*ptr = NULL,
		*text;

	text = empty_string();
	if (!get_int_var(INSERT_MODE_VAR) &&
	    (get_int_var(SHOW_STATUS_ALL_VAR) || window_is_current(window)))
	{
	    if ((text = get_string_var(STATUS_OVERWRITE_VAR)) == NULL)
		text = empty_string();
	}
	malloc_strcpy(&ptr, text);
	return (ptr);
}

static	u_char	*
status_away(Window *window)
{
	u_char	*ptr = NULL,
		*text;

	if (connected_to_server() == 0)
		malloc_strcpy(&ptr, empty_string());
	else if (connected_to_server() == 1 &&
		 !get_int_var(SHOW_STATUS_ALL_VAR) &&
		 !window_is_current(window))
		malloc_strcpy(&ptr, empty_string());
	else
	{
		if (server_get_away(window_get_server(window)) &&
				(text = get_string_var(STATUS_AWAY_VAR)))
			malloc_strcpy(&ptr, text);
		else
			malloc_strcpy(&ptr, empty_string());
	}
	return (ptr);
}

static	u_char	*
status_user0(Window *window)
{
	u_char	*ptr = NULL,
	*text;

	if ((text = get_string_var(STATUS_USER_VAR)) &&
	    (get_int_var(SHOW_STATUS_ALL_VAR) || window_is_current(window)))
		malloc_strcpy(&ptr, text);
	else
		malloc_strcpy(&ptr, empty_string());
	return (ptr);
}

static  u_char    *
status_user1(Window *window)
{
        u_char    *ptr = NULL,
        *text;

        if ((text = get_string_var(STATUS_USER1_VAR)) &&
            (get_int_var(SHOW_STATUS_ALL_VAR) || window_is_current(window)))
                malloc_strcpy(&ptr, text);
        else
                malloc_strcpy(&ptr, empty_string());
        return (ptr);
}

static  u_char    *
status_user2(Window *window)
{
        u_char    *ptr = NULL,
		*text;

        if ((text = get_string_var(STATUS_USER2_VAR)) &&
            (get_int_var(SHOW_STATUS_ALL_VAR) || window_is_current(window)))
                malloc_strcpy(&ptr, text);
        else
                malloc_strcpy(&ptr, empty_string());
        return (ptr);
}

static  u_char    *
status_user3(Window *window)
{
        u_char    *ptr = NULL,
		*text;

        if ((text = get_string_var(STATUS_USER3_VAR)) &&
            (get_int_var(SHOW_STATUS_ALL_VAR) || window_is_current(window)))
                malloc_strcpy(&ptr, text);
        else
                malloc_strcpy(&ptr, empty_string());
        return (ptr);
}

static	u_char	*
status_hold(Window *window)
{
	u_char	*ptr = NULL,
		*text;

	if (window_held(window) && (text = get_string_var(STATUS_HOLD_VAR)))
		malloc_strcpy(&ptr, text);
	else
		malloc_strcpy(&ptr, empty_string());
	return (ptr);
}

static	u_char	*
status_oper(Window *window)
{
	u_char	*ptr = NULL,
		*text;

	if (!connected_to_server())
		malloc_strcpy(&ptr, empty_string());
	else if (server_get_operator(window_get_server(window)) &&
		 (text = get_string_var(STATUS_OPER_VAR)) &&
		 (get_int_var(SHOW_STATUS_ALL_VAR) ||
		  connected_to_server() != 1 || 
		  window_is_current(window)))
		malloc_strcpy(&ptr, text);
	else
		malloc_strcpy(&ptr, empty_string());
	return (ptr);
}

static	u_char	*
status_voice(Window *window)
{
	u_char	*ptr = NULL,
		*text;
	int	server = window_get_server(window);

	if (!connected_to_server())
		malloc_strcpy(&ptr, empty_string());
	else if (has_voice(window_get_current_channel(window),
			   server_get_nickname(server),
			   server) &&
		 (text = get_string_var(STATUS_VOICE_VAR)) &&
		 (get_int_var(SHOW_STATUS_ALL_VAR) ||
		  connected_to_server() != 1 || window_is_current(window)))
		malloc_strcpy(&ptr, text);
	else
		malloc_strcpy(&ptr, empty_string());
	return (ptr);
}

static	u_char	*
status_window(Window *window)
{
	u_char	*ptr = NULL,
		*text;

	if ((text = get_string_var(STATUS_WINDOW_VAR)) &&
	    number_of_windows() > 1 && window_is_current(window))
		malloc_strcpy(&ptr, text);
	else
		malloc_strcpy(&ptr, empty_string());
	return (ptr);
}

static	u_char	*
status_refnum(Window *window)
{
	u_char	*ptr = NULL;
	u_char	*name = window_get_name(window);

	if (name)
		malloc_strcpy(&ptr, name);
	else
	{
		u_char	lbuf[10];

		snprintf(CP(lbuf), sizeof lbuf, "%u", window_get_refnum(window));
		malloc_strcpy(&ptr, lbuf);
	}
	return (ptr);
}

static	u_char	*
status_version(Window *window)
{
	u_char	*ptr = NULL;

	if (connected_to_server() == 1 && !get_int_var(SHOW_STATUS_ALL_VAR) &&
	    !window_is_current(window))
		malloc_strcpy(&ptr, empty_string());
	else
	{
		malloc_strcpy(&ptr, irc_version());
	}
	return (ptr);
}

static	u_char	*
status_null_function(Window *window)
{
	u_char	*ptr = NULL;

	malloc_strcpy(&ptr, empty_string());
	return (ptr);
}
