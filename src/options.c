#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <config.h>
#include "options.h"
#include "error.h"

char       *opt_filename       = NULL;
char       *opt_output         = NULL;
long        opt_line           = 0;
long        opt_column         = 0;
char       *opt_listmembers    = NULL;
char       *opt_liststatic     = NULL;
char       *opt_lang           = "c++";
char       *opt_tagfile        = "tags";
char	   *opt_listfile       = ".icomplete_taglist";
cache_e     opt_cache          = CACHE_AUTO;
const char *opt_progname       = PACKAGE;
struct config_s config={NULL,NULL};

void get_cmdine_opts(int argc, char** argv)
{
	int c;

	/* the struct option look like:
	   struct option { const char *name; int has_arg; int *flag; int val; }; */
	static struct option long_options[] = {
		{"cache"        , 1 , 0 , 'a' }  , 
		{"column"       , 1 , 0 , 'c' }  , 
		{"help"         , 0 , 0 , 'h' }  , 
		{"line"         , 1 , 0 , 'l' }  , 
		{"list-members" , 1 , 0 , 'm' }  , 
		{"output"       , 1 , 0 , 'o' }  , 
		{"lang"         , 1 , 0 , 'g' }  , 
		{"tagfile"      , 1 , 0 , 't' }  , 
		{"listfile"	    , 1 , 0 , 's' }  ,
		{"version"      , 0 , 0 , 'v' }  , 
		{0              , 0 , 0 , 0   } 
	};

	while (1)
	{
		/*         int this_option_optind = optind ? optind : 1; */
		int option_index = 0;
		c = getopt_long (argc, argv, "a:c:g:t:hl:m:o:s:v", long_options, &option_index);
		if (c == -1)
			break;

		switch (c)
		{
			case 0:
				printf ("option %s", long_options[option_index].name);
				if (optarg)
					printf (" with arg %s", optarg);
				printf ("\n");
				break;

			case 'a':
				opt_cache = atoi(optarg);
				if(opt_cache<0 || opt_cache>2)
					bailout("Cache must be a number between 0 and 2!");
				break;

			case 'c':
				opt_column = atoi(optarg);
				break;

			case 'l':
				opt_line = atoi(optarg);
				break;

			case 'g':
				opt_lang = optarg;
				break;

			case 'h':
				usage();
				break;

			case 'm':
				opt_listmembers = optarg;
				break;

			case 'o':
				opt_output = optarg;
				break;

			case 's':
				opt_listfile = optarg;
				break;

			case 't':
				opt_tagfile = optarg;
				break;

			case 'v':
				printf(PACKAGE_NAME "\nVersion: " VERSION "\n"); 
				break;

			case '?':
				break;

			default:
				printf ("?? getopt returned character code 0%o ??\n", c);
		}
	}

	// file name given
	if (optind < argc)
	{
		opt_filename = argv[optind];
		optind++;

		if (optind < argc)
			bailout("Only one source file allowed.\nSee icomplete --help for a usage description.");
	}
	// read from stdin
	else
	{
	}
}

void read_config()
{
	/* cur_section points to the vector for the current [section] of the config file */
	List* cur_section = NULL;

	config.cpp_includes=alloc_list();
	config.cpp_macros=alloc_list();

	char* homedir = getenv("HOME");
	char homeconfig[512];
	if (homedir)
		snprintf(homeconfig, 512, "%s/.icompleterc", homedir);

	char *configs[3];
	configs[0] = ".icomplete";
	configs[1] = homedir ? homeconfig : NULL;
	configs[2] = SYSCONFDIR "/icomplete.conf";

	char line[512];
	char *ptr = NULL;
	for (int i=0; i < 3; i++)
	{
		if (configs[i] == NULL)
			continue;

		FILE *file = fopen(configs[i], "r");
		if (file)
		{
			while (fgets(line, 512, file) != NULL)
			{
				ptr = line;
				if (!strcmp(line, "[cpp_include_paths]\n"))
				{
					cur_section = config.cpp_includes;
					continue;
				}
				else if (!strcmp(line, "[cpp_macros]\n"))
				{
					cur_section = config.cpp_macros;
					continue;
				}

				/* strip leading whitespace and remove comments and empty lines */
				while (*ptr == ' ' || *ptr == '\t')
					ptr++;
				if (*ptr == '#')
					continue;
				if (*ptr == '\n')
					continue;

				if (cur_section != NULL)
				{
					/* remove trailing \n */
					int l = strlen(ptr);
					if (l && ptr[l-1] == '\n')
						ptr[l-1] = '\0';
					char *dup = strdup(line);
					if (!dup || !add_to_list(cur_section,dup))
						bailout ("strdup in read_config() failed");
#if DEBUG >= 2
					printf("DEBUG: option found: %s\n", dup);
#endif
				}
			}
			fclose(file);
		}
	}
}

void free_config()
{
#if DEBUG >= 2
	printf("DEBUG: freeing config\n");
#endif
	if (config.cpp_includes != NULL)
	{
		free_list(config.cpp_includes);
	}

	if (config.cpp_macros != NULL)
	{
		free_list(config.cpp_macros);
	}
}

