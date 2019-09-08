#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vlan.h"
#include "ubr.h"

char *prognm = NULL;
int cmdind = 0;
int debug = 0;

static int help    (char *arg);
static int version (char *arg);

struct cmd cmds[] = {
	{ NULL,      "help", 1, "[command]", "\tShow overview or help text for command(s)", help },
	{ NULL,      "version", 0, NULL, "\tShow program version", version },
	{ vlan_cmds, "vlan", 1, "VID", "\tManage bridge VLAN settings", NULL },
	{ 0 }
};

static int string_match(const char *a, const char *b)
{
   size_t min = MIN(strlen(a), strlen(b));

   return !strncasecmp(a, b, min);
}

static void cmd_usage(char *prefix, struct cmd *cmd, int name_max, int usage_max)
{
	printf("%s%-*s %-*s %s\n",
	       prefix ?: "",
	       name_max,
	       cmd->name,
	       usage_max,
	       cmd->usage ?: "",
	       cmd->help);
}

static int cmd_help(char *prefix, struct cmd *cmd)
{
	printf("Usage:\n\t"); cmd_usage(prefix, cmd, 0, 0);
	printf("Help:\n\t%s\n", cmd->help);

	return 0;
}

static int usage(char *prefix, struct cmd *ctx)
{
	size_t name_max = 0, usage_max = 0;

	for (int i = 0; ctx[i].name; i++) {
		if (strlen(ctx[i].name) > name_max)
			name_max = strlen(ctx[i].name);
		if (ctx[i].usage && strlen(ctx[i].usage) > usage_max)
			usage_max = strlen(ctx[i].usage);
	}
		
	for (int i = 0; ctx[i].name; i++)
		cmd_usage(prefix, &ctx[i], name_max, usage_max);

	return 0;
}

static struct cmd *cmd_parse(char *prefix, int argc, char *argv[], struct cmd *ctx)
{
	if (!prefix || !prefix[0])
		cmdind = 0;

	DBG("prefix '%s' -> argv[%d]: %s\n", prefix, cmdind, argv[cmdind]);
	for (int i = 0; argv[i] && i < 10; i++)
		DBG("argv[%d]: %s\n", i, argv[i]);

	for (int i = 0; ctx[i].name; i++) {
		struct cmd *cmd = &ctx[i];

		if (!argv[cmdind])
			continue;

		if (!string_match(cmd->name, argv[cmdind]))
			continue;

		if (cmd->ctx) {
			struct cmd *next;
			char buf[256];

			snprintf(buf, sizeof(buf), "%s%s ", prefix, cmd->name);
			DBG("append ctx, new prefix %s\n", buf);

			next = cmd_parse(buf, argc - 1, &argv[cmdind++], cmd->ctx);
			strcpy(prefix, buf);

			if (!next) {
				DBG("(none) -> %s context\n", cmd->name);
				cmdind--;
				return cmd;
			}

			return next;
		}

		cmdind++;
		DBG("=> argv[%d]: %s cmd[%d]: %s\n", cmdind, argv[cmdind], i, cmd->name);
		return cmd;
	}

	DBG("-> NULL\n");
	return NULL;
}

static int help(char *arg)
{
	struct cmd *cmd;
	char *argv[10] = { 0 };
	char *tok;
	char prefix[256] = "";
	int argc = 0;

	DBG("%s\n", arg);

	tok = strtok(arg, " \t");
	for (int i = 0; i < 10 && tok; i++) {
		argv[i] = tok;
		argc = i + 1;
		DBG("argv[%d]: %s, argc: %d\n", i, tok, argc);
		tok = strtok(NULL, " \t");
	}

	cmd = cmd_parse(prefix, argc, argv, cmds);
	DBG("cmd: %p, prefix: %s\n", cmd, prefix);
	if (!cmd)
		return usage(prefix, cmds);

	if (cmd->cb)
		return cmd_help(prefix, cmd);

	if (cmd->ctx)
		return usage(prefix, cmd->ctx);

	return 1;
}

static int cmd_exec(int argc, char *argv[])
{
	struct cmd *cmd;
	char prefix[256] = "";

	cmd = cmd_parse(prefix, argc, argv, cmds);
	if (!cmd)
		return usage(prefix, cmds);

	if (cmd->cb) {
		char arg[80] = "";

		DBG("prefix: '%s', cmdind: %d, argv[%d]: %s\n", prefix, cmdind, cmdind, argv[cmdind]);
		if (argv[cmdind] && string_match(argv[cmdind], "help"))
			return cmd_help(prefix, cmd);

		for (int i = cmdind; i < argc; i++) {
			if (i > 1)
				strlcat(arg, " ", sizeof(arg));
			strlcat(arg, argv[i], sizeof(arg));
		}

		return cmd->cb(arg);
	}

	if (cmd->ctx) {
		DBG("Got context, prefix %s\n", prefix);
		return usage(prefix, cmd->ctx);
	}

	return 1;
}

static int version(char *unused)
{
	puts(PACKAGE_VERSION);
	return 0;
}

static char *progname(const char *arg0)
{
	char *nm;

	nm = strrchr(arg0, '/');
	if (nm)
		nm++;
	else
		nm = (char *)arg0;

	return nm;
}

int main(int argc, char *argv[])
{
	int help = 0;
	int json = 0;
	int c;

	prognm = progname(argv[0]);
	while ((c = getopt(argc, argv, "dhjv")) != EOF) {
		switch (c) {
		case 'd':
			debug++;
			break;

		case 'h':
			help++;
			break;

		case 'j':
			json++;
			break;

		case 'v':
			return version(NULL);

		default:
			errx(0, "Unknown option -'%c'\n", c);
		}
	}

	if (optind >= argc) {
		printf("No argument, show ubr0 or first bridge found\n");
		return 0;
	}

	return cmd_exec(argc - optind, &argv[optind]);
}
