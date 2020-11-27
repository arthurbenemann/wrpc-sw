/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Copyright (C) 2012 GSI (www.gsi.de)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 * Author: Grzegorz Daniluk <grzegorz.daniluk@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <wrc.h>
#include "dev/console.h"
#include "dev/syscon.h"

#include "shell.h"
#include "storage.h"

#define SH_MAX_LINE_LEN 80
#define SH_MAX_ARGS 8

/* interactive shell state definitions */

#define SHELL_MAX_COMMANDS 32

#define SH_PROMPT 0
#define SH_INPUT 1
#define SH_EXEC 2
#define SH_EXEC_UI 3

#define ESCAPE_FLAG 0x10000

#define KEY_LEFT (ESCAPE_FLAG | 68)
#define KEY_RIGHT (ESCAPE_FLAG | 67)
#define KEY_ENTER (13)
#define KEY_ESCAPE (27)
#define KEY_BACKSPACE (127)
#define KEY_DELETE (126)

static char cmd_buf[SH_MAX_LINE_LEN + 1];
static int cmd_pos = 0, cmd_len = 0;
static int state = SH_PROMPT;
static int current_key = 0;

static struct wrc_shell_cmd *cmds[ SHELL_MAX_COMMANDS ];
static int n_cmds = 0;

int shell_is_interacting;
int (*shell_ui_callback)(void);

static int insert(char c)
{
	if (cmd_len >= SH_MAX_LINE_LEN)
		return 0;

	if (cmd_pos != cmd_len)
		memmove(&cmd_buf[cmd_pos + 1], &cmd_buf[cmd_pos],
			cmd_len - cmd_pos);

	cmd_buf[cmd_pos] = c;
	cmd_pos++;
	cmd_len++;

	return 1;
}

static void delete(int where)
{
	memmove(&cmd_buf[where], &cmd_buf[where + 1], cmd_len - where);
	cmd_len--;
}

static void esc(char code)
{
	pp_printf("\033[1%c", code);
}

static int _shell_exec(void)
{
	char *tokptr[SH_MAX_ARGS + 1];
	struct wrc_shell_cmd *p;
	int n = 0, i = 0, rv;

	memset(tokptr, 0, sizeof(tokptr));

	while (1) {
		if (n >= SH_MAX_ARGS)
			break;

		while (cmd_buf[i] == ' ' && cmd_buf[i])
			cmd_buf[i++] = 0;

		if (!cmd_buf[i])
			break;

		tokptr[n++] = &cmd_buf[i];
		while (cmd_buf[i] != ' ' && cmd_buf[i])
			i++;

		if (!cmd_buf[i])
			break;
	}

	if (!n)
		return 0;

	if (*tokptr[0] == '#')
		return 0;

	for (i = 0; i < n_cmds; i++)
	{
		p = cmds[i];
		if (!strcasecmp(p->name, tokptr[0])) {
			rv = p->exec((const char **)(tokptr + 1));
			if (rv < 0)
				pp_printf("Command \"%s\": error %d\n",
					p->name, rv);
			return rv;
		}
	}

	pp_printf("Unrecognized command \"%s\".\n", tokptr[0]);
	return -EINVAL;
}

int shell_exec(const char *cmd)
{
	int i;

	if (cmd != cmd_buf)
		strncpy(cmd_buf, cmd, SH_MAX_LINE_LEN);
	cmd_len = strlen(cmd_buf);
	shell_is_interacting = 1;
	i = _shell_exec();
	shell_is_interacting = 0;
	return i;
}

void shell_init()
{
	cmd_len = cmd_pos = 0;
	state = SH_PROMPT;
	shell_ui_callback = NULL;
}

int shell_interactive()
{
	int c;

	switch (state) {
	case SH_PROMPT:
		pp_printf("wrc# ");
		cmd_pos = 0;
		cmd_len = 0;
		state = SH_INPUT;
		return 1;

	case SH_INPUT:
		c = console_getc();

		if (c < 0)
			return 0;

		if (c == 27 || ((current_key & ESCAPE_FLAG) && c == 91))
			current_key = ESCAPE_FLAG;
		else
			current_key |= c;

		if (current_key & 0xff) {

			switch (current_key) {
			case KEY_LEFT:
				if (cmd_pos > 0) {
					cmd_pos--;
					esc('D');
				}
				break;
			case KEY_RIGHT:
				if (cmd_pos < cmd_len) {
					cmd_pos++;
					esc('C');
				}
				break;

			case KEY_ENTER:
				pp_printf("\n");
				state = SH_EXEC;
				break;

			case KEY_DELETE:
				if (cmd_pos != cmd_len) {
					delete(cmd_pos);
					esc('P');
				}
				break;

			case KEY_BACKSPACE:
				if (cmd_pos > 0) {
					esc('D');
					esc('P');
					delete(cmd_pos - 1);
					cmd_pos--;
				}
				break;

			case '\t':
				break;

			default:
				if (!(current_key & ESCAPE_FLAG)
				    && insert(current_key)) {
					esc('@');
					pp_printf("%c", current_key);
				}
				break;

			}
			current_key = 0;
		}
		return 1;

	case SH_EXEC:
		cmd_buf[cmd_len] = 0;
		_shell_exec();

// fixme: ugly hack, we should manage the shell FSM state in a cleaner way.
		if( state == SH_EXEC_UI )
			return 1;

		state = SH_PROMPT;
		return 1;


	case SH_EXEC_UI:
		if( !shell_ui_callback || shell_ui_callback() < 0 || console_getc() == 27 )
		{
			cmd_buf[cmd_len] = 0;
			state = SH_PROMPT;
		}
		return 1;
	}
	return 0;
}

static char shell_init_cmd[] = CONFIG_INIT_COMMAND;

static int build_init_readcmd(uint8_t *cmd, int maxlen)
{
	static char *p = shell_init_cmd;
	int i;

	/* use semicolon as separator */
	for (i = 0; i < maxlen && p[i] && p[i] != ';'; i++)
		cmd[i] = p[i];
	cmd[i] = '\0';
	p += i;
	if (*p == ';')
		p++;
	if (i == 0) {
		/* it's the last call, roll-back *p to be ready for the next
		 * call */
		p = shell_init_cmd;
	}
	return i;
}

void shell_boot_script(void)
{
	uint8_t next = 0;

	while (CONFIG_HAS_BUILD_INIT) {
		cmd_len = build_init_readcmd((uint8_t *)cmd_buf,
					SH_MAX_LINE_LEN);
		if (!cmd_len)
			break;
		pp_printf("executing: %s\n", cmd_buf);
		shell_exec(cmd_buf);
	}

	while (CONFIG_HAS_FLASH_INIT) {
		cmd_len = storage_init_readcmd((uint8_t *)cmd_buf,
					      SH_MAX_LINE_LEN, next);
		if (cmd_len <= 0) {
			if (next == 0)
				pp_printf("Empty init script...\n");
			break;
		}
		cmd_buf[cmd_len - 1] = 0;

		pp_printf("executing: %s\n", cmd_buf);
		shell_exec(cmd_buf);
		next = 1;
	}

	return;
}

void shell_show_build_init(void)
{
	uint8_t i = 0;

	pp_printf("-- built-in script --\n");
	while (CONFIG_HAS_BUILD_INIT) {
		cmd_len = build_init_readcmd((uint8_t *)cmd_buf,
					SH_MAX_LINE_LEN);
		if (!cmd_len)
			break;
		pp_printf("%s\n", cmd_buf);
		++i;
	}
	if (!i)
		pp_printf("(empty)\n");
}


void shell_register_command( struct wrc_shell_cmd* cmd )
{
	if( n_cmds >= SHELL_MAX_COMMANDS )
	{
		pp_printf("can't register shell command '%s', increase SHELL_MAX_COMMANDS\n", cmd->name );
		return;
	}
	cmds[ n_cmds ] = cmd;
	n_cmds++;
}

void shell_list_cmds()
{
	int i;

	for(i = 0; i < n_cmds; i++)
	{
		pp_printf("  %s\n", cmds[i]->name);
	}
}

void shell_activate_ui_command( int (*callback)(void) )
{
	shell_ui_callback = callback;
	state = SH_EXEC_UI;
	cmd_len = 0;
}

#define REGISTER_WRC_COMMAND(_name) \
	{ extern struct wrc_shell_cmd __wrc_cmd_ ## _name; shell_register_command( &__wrc_cmd_ ## _name ); }

void shell_register_commands(void)
{
	REGISTER_WRC_COMMAND(gui);
	REGISTER_WRC_COMMAND(ps);
	REGISTER_WRC_COMMAND(pll);
	REGISTER_WRC_COMMAND(ptp);
	REGISTER_WRC_COMMAND(verbose);
	REGISTER_WRC_COMMAND(mode);
	REGISTER_WRC_COMMAND(mac);
	REGISTER_WRC_COMMAND(sdb);
	REGISTER_WRC_COMMAND(calibration);
	REGISTER_WRC_COMMAND(help);
	REGISTER_WRC_COMMAND(diag);
	REGISTER_WRC_COMMAND(init);
	REGISTER_WRC_COMMAND(sfp);
	REGISTER_WRC_COMMAND(stat);
	REGISTER_WRC_COMMAND(sensors);
	if (HAS_IP)
		REGISTER_WRC_COMMAND(ip);
}

