#include <stdlib.h>
#include <stdio.h>

#include <config.h>
#include "error.h"
#include "options.h"

void usage()
{
	fprintf (stderr, "%s\n"
			"(c) 2005, Martin Stubenschrott\n\n"
			"Usage: %s -l <line> -c <column> [options] [file]\n"
			"   or: %s -m <class> [options] [file]\n\n"
			"Options:\n"
			"  -a --cache=0|1|2          : 0=never build tags file, 1=when necessary 2=always\n"
			"  -c --column=<num>         : The column number where completion should start\n"
			"  -l --line=<num>           : The line number where completion should start\n"
 			"  -t --tagfile=<filename>   : Write and read tags from <filename> instead of `tags'\n"
            "  -g --lang=c++|java|cs     : Input is considered as this language, c++ is default\n"
			"  -m --list-members=<class> : List all (also inherited) members of <class>\n"
			"  -o --output=<filename>    : Write completions to <filename> instead of stdout\n"
			"  -v --version              : Prints the version\n"
			, PACKAGE_NAME,opt_progname,opt_progname);
	exit (1);
}

void bailout(const char* desc)
{
	if(desc)
		fprintf(stderr, "icomplete: %s\n", desc);
	else
		fprintf(stderr, "icomplete: Undefined error\n");
	exit (EXIT_FAILURE);
}
