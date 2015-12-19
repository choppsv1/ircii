/*
 * edit.h: header for edit.c 
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
 *
 * @(#)$eterna: edit.h,v 1.46 2014/03/11 08:59:36 mrg Exp $
 */

#ifndef irc__edit_h_
#define irc__edit_h_

#include "whois.h" /* for WhoisStuff */

typedef struct edit_info_stru EditInfo;
typedef struct who_info_stru WhoInfo;
typedef struct prompt_stru Prompt;

	void	load(u_char *, u_char *, u_char *);
	void	send_text(u_char *, u_char *, u_char *);
	void	eval_inputlist(u_char *, u_char *);
	void	parse_command(u_char *, int, u_char *);
	void	parse_line(u_char *, u_char *, u_char *, int, int, int);
	void	edit_char(u_int);
	void	execute_timer(void);
	void	ison_now(WhoisStuff *, u_char *, u_char *);
	void	redirect_msg(u_char *, u_char *);
	void	query(u_char *, u_char *, u_char *);
	void	forward_character(u_int, u_char *);
	void	backward_character(u_int, u_char *);
	void	forward_history(u_int, u_char *);
	void	backward_history(u_int, u_char *);
	void	toggle_insert_mode(u_int, u_char *);
	void	send_line(u_int, u_char *);
	void	meta1_char(u_int, u_char *);
	void	meta2_char(u_int, u_char *);
	void	meta3_char(u_int, u_char *);
	void	meta4_char(u_int, u_char *);
	void	meta5_char(u_int, u_char *);
	void	meta6_char(u_int, u_char *);
	void	meta7_char(u_int, u_char *);
	void	meta8_char(u_int, u_char *);
	void	quote_char(u_int, u_char *);
	void	type_text(u_int, u_char *);
	void	parse_text(u_int, u_char *);
	void	irc_clear_screen(u_int, u_char *);
	void	command_completion(u_int, u_char *);
	void	e_quit(u_char *, u_char *, u_char *);
	int	check_wait_command(u_char *);
	u_char	*do_channel(u_char *, int);
	u_char	*prompt_current_prompt(void);
	int	prompt_active_count(void);
	void	timer_timeout(struct timeval *);
	EditInfo *alloc_edit_info(void);
	WhoInfo *alloc_who_info(void);
	int	get_digraph_hit(void);
	void	set_digraph_hit(int);
	u_char	get_digraph_first(void);
	void	set_digraph_first(u_char);
	int	get_inside_menu(void);
	void	set_inside_menu(int);
	u_char	*get_sent_nick(void);
	u_char	*get_sent_body(void);
	u_char	*get_recv_nick(void);
	void	set_recv_nick(u_char *);
	int	who_level_before_who_from(void);
	int	get_load_depth(void);
	void	load_global(void);
	int	loading_global(void);
	int	whoreply_check(u_char *, u_char *, u_char *, u_char *,
			       u_char *, u_char *, u_char *, u_char **,
			       u_char *);
	int	is_away_set(void);
	u_char	*alias_buffer(void);
	u_char	*get_invite_channel(void);
	void	set_invite_channel(u_char *);
	
#define AWAY_ONE 0
#define AWAY_ALL 1

#define STACK_POP 0
#define STACK_PUSH 1
#define STACK_SWAP 2

#endif /* irc__edit_h_ */
