#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* pid_t */
#include <sys/wait.h> 
#include <unistd.h> /* fork() */
#include <regex.h>

#include <config.h>
#include "parse.h"
#include "tree.h"
#include "options.h"
#include "readtags.h"
#include "error.h"
#include "tree.h"
#include "treeold.h"

// added by maxime COSTE, I know global variables are evil, but
// it seems to be much cleaner than changing a lot of function definition
// just to add a namespace parameter they will pass to other functions they 
// will call which will again pass it to other functions until we reach the 
// ones which needs the list of namespaces (and I also know my english is poor)
//
// used namespaces and scope namespaces are separated for speed reason :
// scope namespaces may be imbricated so we must test possibilities like 
// nsp1::nsp2 and nsp2::nsp1 (in fact we could parse and see wich is the
// good one)
// used namespaces shall be okay cause one must write 
// "using namespace foo::bar"
char* g_used_namespaces;
char* g_scope_namespaces;

/* open the supplied filename and read it until given line and column number
 * returning the malloc'ed buffer
 * Don't forget to free the string */
static char* read_source_file (const char* filename, int line, int col)
{
	char *buffer = NULL,*tmp; 
	int file_line = 1;
	int file_col = 1;
		
	FILE *file = fopen (opt_filename, "r");
	if (!file)
	{
		fprintf(stderr, "Error opening file `%s'\n", opt_filename);
		return NULL;
	}

	int c;
	int pos = 0;
	while (1)
	{
		/* increase the buffer size every 1024 bytes */
		if (pos % 1024 == 0)
		{
			tmp = realloc (buffer, ((pos / 1024) + 1) * 1024);
			if (!tmp)
			{
				free(tmp);
				fclose(file);
				return NULL;
			}
			buffer=tmp;
		}

		/* we found the correct line and column */
		if (file_line == line && file_col == col)
		{
			buffer[pos] = '\0';
			break;
		}

		/* if not keep reading the file */
		c = fgetc(file);
		if (c == EOF)
		{
			free (buffer);
			buffer = NULL;
			break;
		}

		if (c == '\n')
		{
			file_line++;
			file_col = 1;
		}
		else
			file_col++;

		buffer[pos] = c;
		pos++;
	}

	fclose (file);
	return buffer;
}

/* creates a simple hash for all #include lines of a line */
static long calculate_hash(const char* buf)
{
	long res = 0;
	List *includes=get_includes(buf);
	/* find include'ed files and add them to the cache */
        if(includes)
        {
            List_item *tmp=includes->first;
	    while(NULL!=tmp)
	    {
	        for (int i=0; i < strlen(tmp->item); i++)
			    res += (i+1) * ((char *)tmp->item)[i];
                tmp=tmp->next;
	    }
            free_list(includes);
        }
	return res;
}

// return a zero separated list of strings containing
// all the first substring match of the regex in the buffer
// the substrings are in the order they are found in the buffer
char* list_substrings (char* buf, regex_t* re)
{
    regmatch_t pm[2];
    memset (&pm[0], -1, sizeof(pm));

    char* substrings; 
    long  pos = 0;
    substrings = malloc (10);

    while (regexec (re, buf, 2, &pm[0], 0) == 0) /* while matches found... */
    {
        int len = pm[1].rm_eo - pm[1].rm_so;

        substrings = realloc (substrings, pos+len+2);

        strncpy (substrings+pos, buf + pm[1].rm_so, len);
        *(substrings+pos+len) = 0;
#if DEBUG >= 2
        printf ("adding substring %s \n", substrings+pos);
#endif
        pos += len+1;
       
        buf += pm[0].rm_eo;
    }
    *(substrings+pos) = 0;


    return substrings;

}

// list the namespaces in the file, putting all the used namespace
// in used_namespaces as a zero separated string list, and all
// namespaces of the current scope in scope_namespaces
void list_namespaces (char* buf, char** used_namespaces, 
                      char** scope_namespaces)
{
    regex_t re;
    // find all using namespace * and add it to the used_namespace list
    const char* pattern = 
        "using[ \t\n]+namespace[ \t\n]+([a-zA-Z0-9_:]+)"; 
    
    regcomp (&re, pattern, REG_EXTENDED);
    *used_namespaces = list_substrings (buf, &re); 
    regfree (&re);

    // special case : the std namespaces, on my computer
    // with gcc 3.4.6, the glibcxx uses _GLIBCXX_STD
    // how _GLIBCXX_STD works is just too complicated for
    // the -I option of ctags, that is why I add this 
    // hack.
    unsigned size = 0;
    int   std_used = 0;
    for (char* pointer = *used_namespaces; *pointer != 0;
         pointer += strlen (pointer)+1)
    {
        if (!strncmp (pointer, "std", 3))
        {
            std_used = 1;
        }
        size += strlen(pointer)+1;
    }
    if (std_used)
    {
        int len = strlen ("_GLIBCXX_STD");
        *used_namespaces = realloc (*used_namespaces, 
                                    size + len + 2);
        strncpy (*used_namespaces+size, "_GLIBCXX_STD", len);
        *(*used_namespaces+size+len) = 0;
        *(*used_namespaces+size+len+1) = 0;
#if DEBUG >= 2
            printf ("namespace _GLIBCXX_STD added\n");
#endif
    }
    // end of special case

    // find all namespace * { blocks
    pattern = "namespace[ \t\n]+([a-zA-Z0-9]+)[ \t\n]*\\{";
    regcomp (&re, pattern, REG_EXTENDED);
    *scope_namespaces = list_substrings (buf, &re);
    regfree (&re);
}

/* forks and executes ctags to rebuild a tags file
 * storing cache_value in the tags file */
static void build_tags_file(long cache_value)
{
	pid_t child_pid, wait_pid;
	int status = 0;
	switch (child_pid = fork())
	{
		case -1:
			bailout("error fork()'ing child process");
		case 0: /* child */ 
			{
				// create an .icomplete_taglist file to feed the exuberant-ctags program with filenames
				Tree *includes = build_include_tree(NULL, opt_filename);
				FILE *taglist = fopen(opt_listfile, "w");
				printf("%s - %s - %x - %x\n", opt_listfile, opt_filename, includes, taglist);
				if (includes && taglist)
				{
					write_tree_to_file(includes, taglist);
					fclose (taglist);

					/* save the cache information in the tags file */
					FILE *tags = fopen(opt_tagfile, "w");
					if (tags)
					{
						fprintf(tags, "!_TAG_FILE_FORMAT	2	/extended format; --format=1 will not append ;\" to lines/\n"
								"!_TAG_FILE_SORTED	1	/0=unsorted, 1=sorted, 2=foldcase/\n"
								"!_TAG_PROGRAM_AUTHOR	Darren Hiebert	/dhiebert@users.sourceforge.net/"
								"!_TAG_PROGRAM_NAME	Exuberant Ctags	//\n"
								"!_TAG_PROGRAM_URL	http://ctags.sourceforge.net	/official site/\n"
								"!_TAG_PROGRAM_VERSION	5.5.4	//\n"
								"!_TAG_CACHE\t%ld\n", cache_value);
						fclose(tags);
					}

					// 10, because we have argmuents[0-8] + the terminating NULL
					/*char **arguments = (char**)malloc(sizeof(char*) * (10 + (config.cpp_macros->size*2)));*/
					char **arguments = (char**)calloc(sizeof(char*), 10 + (config.cpp_macros->size*2));
					if (arguments == NULL)
						bailout ("Could not get memory in build_tags_file() for char **arguments");

					arguments[0]=CTAGS_CMD;
					arguments[1]="--append";
					arguments[2]="--language-force=c++";
					arguments[3]="--fields=afiKmsSzn";
					arguments[4]="--c++-kinds=cdefgmnpstuvx";
					arguments[5]="-L";
					arguments[6]=opt_listfile;
					arguments[7]="-f";
					arguments[8]=opt_tagfile;
					int i = 8;
					List_item *it=config.cpp_macros->first;
					while(NULL!=it)
					{
						arguments[++i]="-I";
						arguments[++i]=it->item;
                                                it=it->next;
                                                i+=2;
					}
					arguments[++i] = NULL;

#if DEBUG >= 2
					printf("CALLING: ");
					for (int x = 0; x <= i; x++)
						printf("%s ", arguments[x]);
					printf("\n");
#endif
					// call exuberant-ctags safely by fork() and exec()'ing instead of a simple system()
					if (execvp(CTAGS_CMD, arguments) == -1)
					{
						perror(CTAGS_CMD);
						unlink(opt_listfile);
						bailout("Make sure that exuberant-ctags is correctly installed.\n\nSometimes the executable name is not `ctags', then you need to rebuild the `icomplete' package like this:\n\n# CTAGS_CMD=exuberant-ctags ./configure\n# make\n# sudo make install");
					}
					free_tree(includes);
					printf("%d\n", unlink(opt_listfile));
				}
				else
					bailout ("Error creating taglist file.\nCheck permissions and the include directories in your .icompleterc file");
				break;
			}
		default: /* father */
			wait_pid = wait(&status);
			break;
	}
}

int main(int argc, char** argv)
{
	int ret = 0;
	get_cmdine_opts(argc, argv);
	read_config();

	// we have a file name as an argument, parse it
	if (opt_filename)
	{
		// if we supply a filename, line and column numbers are mandatory
		if (opt_line < 1 || opt_column < 1)
			usage();

		char *buf = read_source_file(opt_filename, opt_line, opt_column);
		if (!buf)
			bailout ("Could not read file until given line and column numbers.");

		long cache_value = calculate_hash(buf);
		/* automatic cache mode */
		/* we save a hash value for all included files from the current file
		 * in the tags file, if it matches the hash of the current buffer, nothing
		 * has changed, and reuse the existing tags file */
		if (opt_cache == 1)
		{
			FILE *fCache = fopen(opt_tagfile, "r");
			bool build_cache = true;
			if (fCache)
			{
				char cacheline[512];
				int maxlines = 10; /* for speed reasons, only search the first 10 lines */
				while (maxlines-- && fgets(cacheline, 512, fCache) != NULL)
				{
					char cmp_pattern[512];
					snprintf(cmp_pattern, 512, "!_TAG_CACHE\t%ld\n", cache_value);
					if (!strcmp(cacheline, cmp_pattern))
					{
#if DEBUG >= 2
						fprintf(stderr, "valid tags `%s' file found, icomplete will reuse it\n", opt_tagfile);
#endif
						build_cache = false;
					}
				}
				fclose(fCache);
			}
			if (build_cache)
				build_tags_file(cache_value);
		}
		/* no cache, always rebuild tags file */
		else if (opt_cache ==2)
		{
			build_tags_file(cache_value);
		}

        list_namespaces(buf, &g_used_namespaces, &g_scope_namespaces);

		/* real parsing starts here */
		Expression exp;
		Scope sc;
		exp = parse_expression(buf, &sc);
		free(buf);
		buf=NULL;
		
		if (exp.access == ParseError)
			bailout("could not parse current expression.");

#if DEBUG >= 2
		if (exp.access & AccessMembers || exp.access & AccessPointer || exp.access & AccessStatic)
			fprintf(stderr, "class:%s | scope:%s\n", exp.class, sc.scope);
		else if (exp.access & AccessInFunction)
			fprintf(stderr, "class:%s | function:%s | scope:%s\n", strcmp(exp.class, "") ? exp.class : "<none>", exp.function, sc.scope);
		else if (exp.access & AccessGlobal)
			fprintf(stderr, "global functions/variables for scope: %s\n", sc.scope);
#endif

		/* we have all relevant information, so just list the entries */
		if (exp.access == ParseError)
			bailout("Parse error");

		find_entries(&exp, &sc);
        free (g_used_namespaces);
        free (g_scope_namespaces);
	}

	else if (opt_listmembers)
	{
		Scope sc;
		sc.scope[0] = '\0';
		sc.localdef[0] = '\0';
		Expression exp;
		exp.access = AccessMembers;
		strncpy (exp.class, opt_listmembers, 255);
		exp.class[255] = '\0';
		exp.function[0] = '\0';

        g_used_namespaces = NULL;
        g_scope_namespaces = NULL;

		find_entries (&exp, &sc);
	}
	
	free_config();
	return ret;
}

