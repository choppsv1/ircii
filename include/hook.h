/*
 * hook.h: header for hook.c
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
 * @(#)$eterna: hook.h,v 1.51 2015/02/22 12:43:18 mrg Exp $
 */

#ifndef irc__hook_h_
# define irc__hook_h_

/* Hook: The structure of the entries of the hook functions lists */
typedef struct	hook_stru Hook;

/* HookFunc: A little structure to keep track of the various hook functions */
typedef struct hook_func_stru HookFunc;

/*
 * NumericList: a special list type to dynamically handle numeric hook
 * requests 
 */
typedef struct numeric_list_stru NumericList;

	int	do_hook(int, char *, ...)
			__attribute__((__format__ (__printf__, 2, 3)));
	void	on(u_char *, u_char *, u_char *);
	void	save_hooks(FILE *, int);
	void	remove_hook(int, u_char *, int, int, int);
	void	show_hook(Hook *, u_char *);
	int	in_on_who(void);
	int	set_in_on_who(int);
	int	current_dcc_hook(void);

enum {
	ACTION_LIST = 0,
	CHANNEL_NICK_LIST,
	CHANNEL_SIGNOFF_LIST,
	CONNECT_LIST,
	CTCP_LIST,
	CTCP_REPLY_LIST,
	DCC_CHAT_LIST,
	DCC_CONNECT_LIST,
	DCC_ERROR_LIST,
	DCC_LIST_LIST,
	DCC_LOST_LIST,
	DCC_RAW_LIST,
	DCC_REQUEST_LIST,
	DISCONNECT_LIST,
	ENCRYPTED_NOTICE_LIST,
	ENCRYPTED_PRIVMSG_LIST,
	EXEC_LIST,
	EXEC_ERRORS_LIST,
	EXEC_EXIT_LIST,
	EXEC_PROMPT_LIST,
	EXIT_LIST,
	FLOOD_LIST,
	HELP_LIST,
	HOOK_LIST,
	ICB_CMDOUT_LIST,
	ICB_ERROR_LIST,
	ICB_STATUS_LIST,
	ICB_WHO_LIST,
	IDLE_LIST,
	INPUT_LIST,
	INVITE_LIST,
	JOIN_LIST,
	KICK_LIST,
	LEAVE_LIST,
	LIST_LIST,
	MAIL_LIST,
	MODE_LIST,
	MSG_LIST,
	MSG_GROUP_LIST,
	NAMES_LIST,
	NICKNAME_LIST,
	NOTE_LIST,
	NOTICE_LIST,
	NOTIFY_SIGNOFF_LIST,
	NOTIFY_SIGNON_LIST,
	OS_SIGNAL_LIST,
	PUBLIC_LIST,
	PUBLIC_MSG_LIST,
	PUBLIC_NOTICE_LIST,
	PUBLIC_OTHER_LIST,
	RAW_IRC_LIST,
	RAW_SEND_LIST,
	SEND_ACTION_LIST,
	SEND_DCC_CHAT_LIST,
	SEND_MSG_LIST,
	SEND_NOTICE_LIST,
	SEND_PUBLIC_LIST,
	SEND_TALK_LIST,
	SERVER_NOTICE_LIST,
	SIGNOFF_LIST,
	TALK_LIST,
	TIMER_LIST,
	TOPIC_LIST,
	WALL_LIST,
	WALLOP_LIST,
	WHO_LIST,
	WIDELIST_LIST,
	WINDOW_LIST,
	WINDOW_KILL_LIST,
	WINDOW_LIST_LIST,
	WINDOW_SWAP_LIST,
	NUMBER_OF_LISTS
};

#endif /* irc__hook_h_ */
