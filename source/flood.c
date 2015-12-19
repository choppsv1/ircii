/*
 * flood.c: handle channel flooding. 
 *
 * This attempts to give you some protection from flooding.  Basically, it keeps
 * track of how far apart (timewise) messages come in from different people.
 * If a single nickname sends more than 3 messages in a row in under a
 * second, this is considered flooding.  It then activates the ON FLOOD with
 * the nickname and type (appropriate for use with IGNORE). 
 *
 * Thanks to Tomi Ollila <f36664r@puukko.hut.fi> for this one. 
 */

#include "irc.h"
IRCII_RCSID("@(#)$eterna: flood.c,v 1.22 2014/01/31 12:35:57 mrg Exp $");

#include "hook.h"
#include "ircaux.h"
#include "ignore.h"
#include "flood.h"
#include "vars.h"
#include "output.h"

static	char	*ignore_types[NUMBER_OF_FLOODS] =
{
	"MSG",
	"PUBLIC",
	"NOTICE",
	"WALL",
	"WALLOP"
};

typedef struct flood_stru
{
	u_char	*nick;
	int	type;
	u_char	flood;
	long	cnt;
	time_t	start;
}	Flooding;

static	Flooding *flood = NULL;

/*
 * check_flooding: This checks for message flooding of the type specified for
 * the given nickname.  This is described above.  This will return 0 if no
 * flooding took place, or flooding is not being monitored from a certain
 * person.  It will return 1 if flooding is being check for someone and an ON
 * FLOOD is activated. 
 *
 * NOTE:  flood's are never freed.
 */
int
check_flooding(u_char *nick, u_char *to, int type, u_char *line)
{
	static	int	users = 0,
			pos = 0;
	int	i;
	time_t	flood_time,
	diff;
	Flooding *tmp;

	if (users != get_int_var(FLOOD_USERS_VAR))
	{
		if ((users = get_int_var(FLOOD_USERS_VAR)) == 0)
			return(1);
		if (flood)
			flood = new_realloc(flood, sizeof(*flood) * users);
		else
			flood = new_malloc(sizeof(*flood) * users);
		for (i = 0; i < users; i++)
		{
			flood[i].nick = 0;
			flood[i].cnt = 0;
			flood[i].flood = 0;
		}
	}
	if (users == 0)
		return (1);
	for (i = 0; i < users; i++)
	{
		if (flood[i].nick && *(flood[i].nick))
		{
			if ((my_stricmp(nick, flood[i].nick) == 0) &&
					(type == flood[i].type))
				break;
		}
	}
	flood_time = time(0);
	if (i == users)
	{
		pos = (pos + 1) % users;
		tmp = &(flood[pos]);
		tmp->nick = 0;
		malloc_strcpy(&tmp->nick, nick);
		tmp->type = type;
		tmp->cnt = 1;
		tmp->start = flood_time;
		tmp->flood = 0;
		return (1);
	}
	else
		tmp = &(flood[i]);
	tmp->cnt++;
	diff = flood_time - tmp->start;
	if (tmp->cnt > get_int_var(FLOOD_AFTER_VAR))
	{
		if ((diff == 0) || (tmp->cnt / diff) >
				get_int_var(FLOOD_RATE_VAR))
		{
			if (tmp->flood == 0)
			{
				if (get_int_var(FLOOD_WARNING_VAR))
					say("%s flooding detected from %s",
						ignore_types[type], nick);
				tmp->flood = 1;
			}
			return (do_hook(FLOOD_LIST, "%s %s %s %s", nick,
				to, ignore_types[type], line));
		}
		else
		{
			tmp->flood = 0;
			tmp->cnt = 1;
			tmp->start = flood_time;
		}
	}
	return (1);
}
