#include <ctype.h>
#include <sys/types.h> /* MacOS X needs it included before regex.h */
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h> /* close() */
#include <fcntl.h> /* O_RDONLY */
#include <sys/stat.h>
#include <sys/mman.h> 
#include <assert.h>

#include <config.h>
#include "parse.h"
#include "error.h"
#include "tree.h"
#include "options.h"
#include "treeold.h"

typedef enum
{
    DEFAULT,           //
    INCLUDE,           //parser found #  -> search for include
    OPENBRACE,         //now we have #include -> search for < or "
    BEGINCOMMENT,      //parser found /
    ONELINECOMMENT,    //parser found // -> ignore till end of line
    MULTILINECOMMENT,  //parser found /* -> ignore till */
    MULTILINECOMMENTEND
} State;

/*
 * for a given expression "myvar->getX().toFloat"
 * the function should just return "myvar"
 * and move the **expr pointer after the ident
 * returns NULL, if no identifier is found, and then the value of expr is undefined
 */
static char* scan_for_ident(const char **expr)
{
	static char ident[256];
    int valid_chars = 0; /* number of identified characters in the ident string */

	char c;
	while ((c = **expr) != '\0')
	{
		if(valid_chars == 0 && isspace(c))
		{
			(*expr)++;
			continue;
		}
		// XXX: Namespace support
		else if(isalpha(c) || c == '_') // ???: || c == ':')
		{
			ident[valid_chars] = c;
			if (++valid_chars == 255)
			{
				ident[255] = '\0';
				return ident;
			}
		}
		/* a digit may be part of an ident but not from the start */
		else if (isdigit(c))
		{
			if (valid_chars)
			{
				ident[valid_chars] = c;
				if (++valid_chars == 255)
				{
					ident[255] = '\0';
					return ident;
				}
			}
			else
				return NULL;

		}
		else
			break;

		(*expr)++;
	}

	if (valid_chars)
	{
		ident[valid_chars] = '\0';
		return ident;
	}
	else
		return NULL;
}

/* if the current identfier is the name of a function */
static bool scan_for_funcdef(const char *expr)
{
	char c;
	while ((c = *expr) != '\0')
	{
		switch(c)
		{
			case ' ':
			case '\t':
			case '\n':
				expr++;
				continue;

			case '(':
				return true;
			default:
				return false;
		}
	}
	return false;
}

/* for ident=getRect and class=QWidget return QRect
 * @param token_is_function = true, if the token is a function or false if a variable
 * caller must free() the returned string */
static char* get_type_of_token (const char* ident, const char* class, Scope* scope, bool token_is_function)
{
	/* if we have a variable and already found a local definition, just return it after duplicating */
	if (!token_is_function && scope->localdef && strlen(scope->localdef))
		return strdup(scope->localdef);

	/* if the identifier is this-> return the current class */
	if (!strcmp(ident, "this"))
		return strdup(scope->scope);

	Tree *tree = NULL;
	if (class && strlen(class))
	{
		tree = build_inheritance_tree(class);
		if (!tree)
			bailout ("Couldn't build inheritance tree\n");
	}

	tagFileInfo info;
	tagEntry entry;
	tagFile *tfile = tagsOpen (opt_tagfile, &info);
	if (tfile && info.status.opened)
	{
		if (tagsFind (tfile, &entry, ident, TAG_OBSERVECASE | TAG_FULLMATCH) == TagSuccess)
		{
			do
			{
				if (tree && !is_member_of_scope(&entry, scope, tree))
					continue;

				const char* kind = tagsField(&entry, "kind");
				if (token_is_function) /* only list if tag is a function */
				{
					if (!kind || (strcmp(kind, "function") && strcmp(kind, "prototype")))
						continue;
				}
				else /* or a variable */
				{
					//brc: add externvar for extern variables like cout
					if (!kind || (strcmp(kind, "variable") && strcmp(kind,"externvar") && strcmp(kind, "field")
					//brc: namespace workarround: add namespace
						&& strcmp(kind,"namespace") && strcmp(kind, "member")))
						continue;
				}

				/* need to duplicate the pattern, don't ask me why */
				char *pattern = strdup(entry.address.pattern);
				char *type = extract_type_qualifier(pattern, ident);
				free(pattern);
				if (tree)
					free_tree(tree);
				tagsClose(tfile);
				return type;
			}
			while (tagsFindNext(tfile, &entry) == TagSuccess);
		}
		tagsClose(tfile);
	}
	return NULL;
}

char* extract_type_qualifier(const char *string, const char *expr )
{
	/* check with a regular expression for the type */
	regex_t re;
	regmatch_t pm[8]; // 7 sub expression -> 8 matches
	memset (&pm, -1, sizeof(pm));
	/*
	 * this regexp catches things like:
	 * a) std::vector<char*> exp1[124] [12], *exp2, expr;
	 * b) QClass* expr1, expr2, expr;
	 * c) int a,b; char r[12] = "test, argument", q[2] = { 't', 'e' }, expr;
	 *
	 * it CAN be fooled, if you really want it, but it should
	 * work for 99% of all situations.
	 *
	 * QString
	 * 		var;
	 * in 2 lines does not work, because //-comments would often bring wrong results
	 */

#define STRING      "\\\".*\\\""
#define BRACKETEXPR "\\{.*\\}"
#define IDENT       "[a-zA-Z_][a-zA-Z0-9_]*"
#define WS          "[ \t\n]*"
#define PTR         "[\\*&]?\\*?"
#define INITIALIZER "=(" WS IDENT WS ")|=(" WS STRING WS ")|=(" WS BRACKETEXPR WS ")" WS
#define ARRAY 		WS "\\[" WS "[0-9]*" WS "\\]" WS

	char *res = NULL;
	static char pattern[512] =
		"(" IDENT "\\>)" 	// the 'std' in example a)
		"(::" IDENT ")*"	// ::vector
		"(" WS "<[^>;]*>)?"	// <char *>
		"(" WS PTR WS IDENT WS "(" ARRAY ")*" "(" INITIALIZER ")?," WS ")*" // other variables for the same ident (string i,j,k;)
		"[ \t\\*&]*";		// check again for pointer/reference type

	/* must add a 'termination' symbol to the regexp, otherwise
	 * 'exp' would match 'expr' */
	char regexp[512];
	snprintf(regexp, 512, "%s\\<%s\\>", pattern, expr);

	/* compile regular expression */
	int error = regcomp (&re, regexp, REG_EXTENDED) ;
	if (error)
		bailout("regcomp failed");

	/* this call to regexec finds the first match on the line */
	error = regexec (&re, string, 8, &pm[0], 0) ;
	while (error == 0) /* while matches found */
	{   
		/* subString found between pm.rm_so and pm.rm_eo */
		/* only include the ::vector part in the indentifier, if the second subpattern matches at all */
		int len = (pm[2].rm_so != -1 ? pm[2].rm_eo : pm[1].rm_eo) - pm[1].rm_so;
		if (res)
			free (res);
		res = (char*) malloc (len + 1);
		if (!res)
		{
			regfree (&re);
			return NULL;
		}
		strncpy (res, string + pm[1].rm_so, len); 
		res[len] = '\0';

		/* This call to regexec finds the next match */
		error = regexec (&re, string + pm[0].rm_eo, 8, &pm[0], 0) ;
		break;
	}
	regfree(&re);

	/* we if the proposed type is a keyword, we did something wrong, return NULL instead */
/*    static char *keywords[] = { "if", "else", "goto", "for", "do", "while", "const", "static", "volatile",*/
/*        "register", "break", "continue", "return", "switch", "case", "new", "typedef", "inline"};*/
	return res;
}


bool get_type_of_expression(const char* expr, Expression *exp, Scope *scope)
{
	char *ident = NULL;

	/* very basic stack implementation to keep track of tokens */
	const int max_items = 20;
	const char* stack[max_items];
	int num_stack = 0;

	/* skip nested brackets */
	int brackets = 0, square_brackets = 0;

	bool in_ident = false; /* if the current position is within an identifier */
	bool extract_ident = false; /* if we extract the next string which looks like an identifier - only after: . -> and ( */

	if (strlen(expr) < 1)
		return false;

	int len = strlen(expr);
	if (!len)
		return false;

	const char *start = expr + len;
	while (--start >= expr )
	{
		/* skip brackets */
		if (brackets > 0 || square_brackets > 0)
		{
			if (*start == '(')
				--brackets;
			else if (*start == ')')
				++brackets;
			else if (*start == '[')
				--square_brackets;
			else if (*start == ']')
				++square_brackets;
			continue;
		}
		/* identifier */
		if (isdigit(*start))
			in_ident = false;
		else if (isalpha(*start) || *start == '_')
		{
			in_ident = true;
		}
		else
		{
			switch (*start)
			{
				/* skip whitespace */
				case ' ':
				case '\t':
					if (in_ident)
						goto extract;
					else
					{
						in_ident = false;
						continue;
					}

				/* continue searching to the left, if we
				 * have a . or -> accessor */
				case '.':
					if (in_ident && extract_ident)
					{
						const char *ident = start+1;
						if (num_stack < max_items)
							stack[num_stack++] = ident;
						else
							return false;
					}
					in_ident = false;
					extract_ident = true;
					continue;
				case '>': /* pointer access */
				case ':': /* static access */
					if ((*start == '>' && (start-1 >= expr && (start-1)[0] == '-')) ||
						(*start == ':' && (start-1 >= expr && (start-1)[0] == ':')))
					{
						if (in_ident && extract_ident)
						{
							const char *ident = start+1;
							if (num_stack < max_items)
								stack[num_stack++] = ident;
							else
								return false;
						}
						in_ident = false;
						extract_ident = true;
						--start;
						continue;
					}
					else
					{
						start++;
						goto extract;
					}
				case '(': /* start of a function */
					if (extract_ident)
					{
						start++;
						goto extract;
					}
					else
					{
						extract_ident = true;
						in_ident = false;
						break;
					}

				case ')':
					if (in_ident) /* probably a cast - (const char*)str */
					{
						start++;
						goto extract;
					}
					brackets++;
					break;

				case ']':
					if (in_ident)
					{
						start++;
						goto extract;
					}						
					square_brackets++;
					break;

				default:
					start++;
					goto extract;
			}
		}
	}

extract:
	/* ident is the leftmost token, stack[*] the ones to the right
	 * Example: str.left(2,5).toUpper()
	 *          ^^^ ^^^^      ^^^^^^^
	 *        ident stack[1]  stack[0]
	 */
	ident = scan_for_ident(&start);
	if (!ident)
		return false;

	/* we need this little kludge with old_type to avoid mem leaks */
	char *type = NULL, *old_type = NULL;

	/* for static access, parsing is done, just return the identifier */
	if (exp->access != AccessStatic)
	{
		get_scope_and_locals(scope, expr, ident);
						//printf("%s\n",type);

		/* if we have the start of a function/method, don't return the type
		 * of this function, but class, which it is member of */
		type = get_type_of_token (ident, NULL, scope, scan_for_funcdef(start));
#if DEBUG >= 2
		fprintf(stderr, "type of token: %s : %s\n", ident, type);
#endif
		/* members can't be local variables */
		scope->localdef[0] = '\0';
		while (type && num_stack > 0)
		{
			ident = scan_for_ident(&stack[--num_stack]);
			free(old_type); /* disard token, if too old */
			old_type = type;
			//<brc code>: namespace workaround: if the "type" of an identifier is namespace ignore it
			if (!strcmp(type,"namespace"))
				old_type=NULL;
			//<\end brc code>
			type = get_type_of_token (ident, old_type, scope, scan_for_funcdef(stack[num_stack]));
#if DEBUG >= 2
			fprintf(stderr, "type of token: %s : %s\n", ident, type);
#endif
		}
	}
	else /* static member */
	{
		type = strdup(ident);
	}

	/* copy result into passed Expression argument */
	if (type && !(exp->access == AccessInFunction))
	{
		strncpy (exp->class, type, 255);
		exp->function[255] = '\0';
		exp->function[0] = '\0';
		free(type);
		free(old_type);
		return true;
	}
	else if (exp->access == AccessInFunction)
	{
		strncpy (exp->class, old_type ? old_type : "", 255);
		strncpy (exp->function, ident, 255);
		exp->class[255] = '\0';
		exp->function[255] = '\0';
		free(type);
		free(old_type);
		return true;
	}

	return false;
}

Expression parse_expression(const char *expr, Scope *scope)
{
	Expression exp;
	exp.access = ParseError;
	/* I need to duplicate the string, since writing to
	 * string literals would cause a segfault otherwise
	 * make sure, it is deleted before every return */
	char * expression = strdup(expr);
	if (!expression)
	{
		return exp;
	}
	int len = strlen(expression); /* strlen() returns the size without the NULL-byte */
	if (len < 1)
	{
		free(expression);
		return exp;
	}

	/* A global function should be completed, if nothing else is found */
	exp.access = AccessGlobal;

	bool allow_AccessInFunction = true; // false, when other characters than whitespace were typed
	/* search for the type of the correct completion */
	while (--len >= 0)
	{
		char last = expression[len]; 
		switch (last)
		{
			case ' ': 
			case '\t':
				/* skip initial spaces */
				if (exp.access == AccessGlobal)
				{
/*                    exp.access = ParseError;*/
					continue;
				}
				else
				{
					exp.access = AccessGlobal;
					goto extract;
				}

			case '>':
				if (len && expression[len-1] == '-')
				{
					exp.access = AccessPointer;
					expression[len+1] = '\0'; /* truncate the string */
					goto extract;
				}
				else
				{
					exp.access = AccessGlobal; /* XXX: AccessError ? */
					goto extract;
				}
			case '.':
					exp.access = AccessMembers;
					expression[len+1] = '\0'; /* truncate the string */
					goto extract;
			case ':':
				if (len && expression[len-1] == ':')
				{
					exp.access = AccessStatic;
					expression[len+1] = '\0'; /* truncate the string */
					goto extract;
				}
				else
				{
					exp.access = AccessGlobal;
					goto extract;
				}
			case '(':
				if (allow_AccessInFunction)
				{
					exp.access = AccessInFunction;
					expression[len+1] = '\0'; /* truncate the string */
				}
				goto extract;

			default:
				if (isalpha(last) || last == '_')
				{
					/* we only list function definitions if after the opening
					 * ( bracket of the function is no identifier */
					allow_AccessInFunction = false;
					break;
				}
				else
					goto extract;
		}
	}

extract:
	/* now extract the type out of the string */
	if (exp.access == AccessMembers || exp.access == AccessPointer || exp.access == AccessStatic || exp.access == AccessInFunction)
	{
		if (!get_type_of_expression((const char*)expression, &exp, scope))
			exp.access = ParseError;
	}
	else if (exp.access == AccessGlobal)
		get_scope_and_locals(scope, (const char*)expression, NULL);
	
	free (expression);
	return exp;
}

char* prettify_tag(tagEntry *entry, int ident_len)
{
#define TOTAL_LEN 255

	if (ident_len < 0)
		ident_len = 30;

	static char tag[TOTAL_LEN + 1];
	const char* signature = tagsField(entry, "signature");
	const char* kind = tagsField(entry, "kind");
	const char* access = tagsField(entry, "access");

	if (!kind)
		return NULL;

	snprintf (tag, TOTAL_LEN, "%s%s: ", entry->name, signature ? "()" : "");
	int len = strlen (tag);
	if (len < ident_len)
	{
		/* fill padding with spaces, overwriting NULL byte of snprintf */
		memset(tag + len, ' ', ident_len - len);
		len = ident_len;
	}
	int counter = len;

	char *p = (char*)entry->address.pattern;
	
	/* for a macro the pattern is already parsed */
	if (!strcmp(kind, "macro"))
	{
		/* NOTE: exuberant-ctags 5.5.4 does not provide a valid pattern for found macros
		 * work around it, by getting the line myself */
		char pat_macro[512];
		unsigned long line = entry->address.lineNumber;
		if (line == 0) /* sometimes ctags can't find the correct line */
			return tag;

		FILE *fileMacro = fopen(entry->file, "r");
		if (fileMacro)
		{
			while ((p = fgets(pat_macro, 512, fileMacro)) != NULL)
			{
				line--;
				if (line <= 0)
				{
					/* remove leading spaces */
					p++; /* skip over # - it is added later again */
					while (*p == ' ' || *p == '\t')
						p++;
					snprintf(tag + len, TOTAL_LEN - len, "#%s", p);
					/* remove new line at the end */
					tag[strlen(tag)-1] = '\0';
					break;
				}
			}
			strncat(tag, " [macro]", TOTAL_LEN);
			fclose(fileMacro);
			return tag;
		}
	}
	/* special handling for enumerator */
	if (!strcmp(kind, "enumerator"))
	{
		/* skip whitespace from variable/function patterns */
		size_t skip = strspn (p, "/^ \t");
		p += skip;
		/* remove trailing $/ characters */
		char *pos = NULL;
		if ((pos = strstr(p, "$/")) != NULL)
			*pos = '\0';
		/* replace \/\/ and \/ * *\/ to correctly show comments */
		while ((pos = strstr(p, "\\/\\/")) != NULL)
			memcpy(pos, "  //", 4);
		while ((pos = strstr(p, "\\/*")) != NULL)
			memcpy(pos, " /*", 3);
		while ((pos = strstr(p, "*\\/")) != NULL)
			memcpy(pos, " */", 3);

		snprintf(tag + len, TOTAL_LEN - len, "%s [enumerator]", p);
		return tag;
	}
	
	
	/* skip whitespace from variable/function patterns */
	size_t skip = strspn (p, "/^ \t");
	p += skip;

	/* find last position of the name in the pattern */
	char *pos = 0, *haystack = (char*)entry->address.pattern;
	while ((haystack = strstr (haystack, entry->name)) != NULL)
	{
		pos = haystack;
		haystack++;
	}
	if (pos)
		pos += strlen (entry->name);

	while(p && p < entry->address.pattern + TOTAL_LEN - len && p < pos)
	{
		/* skip two consecutive spaces and replace tabs with spaces */
		if (*p == '\t')
			*p = ' ';
		if ((*p == ' ' && *(p+1) != ' ' && *(p+1) != '\t') || *p != ' ')
		{
			tag[counter] = *p++;
			counter++;
		}
		else
			p++;
	}
	tag[counter] = '\0';

	/* if it is a function, add signature as well */
	if (signature)
	{
		strncat (tag, signature, TOTAL_LEN-strlen(tag));
	}

	if (access && (!strcmp(access, "private") || !strcmp(access, "protected")))
	{
		strncat(tag, " [", TOTAL_LEN);
		strncat(tag, access, TOTAL_LEN);
		strncat(tag, "]", TOTAL_LEN);
	}

	return tag;
}


void get_scope_and_locals (Scope *sc, const char* expr, const char *ident)
{
	// XXX: platform specific
	char tmpnameIn[64] = "/tmp/icomplete-XXXXXX";
	char tmpnameOut[64] = "/tmp/icomplete-XXXXXX";

	// initialize scope if nothing better is found
	strcpy(sc->scope, "");
	strcpy(sc->localdef, "");

	int fdIn = mkstemp (tmpnameIn);
	if (fdIn == -1)
		return;
	int fdOut = mkstemp (tmpnameOut);
	if (fdOut == -1)
	{
		close (fdIn);
		unlink (tmpnameIn);
		return;
	}
	
	if (write (fdOut, expr, strlen(expr)) == -1)
	{
		perror ("write failed");
		goto exit;
	}

	/* create a tags file for `expr' with function names only. 
	 * The function with the highest line number is our valid scope
	 * --sort=no, because tags generation probably faster, and
	 * so I just have to look for the last entry to find 'my' tag
	 */
	char command[FILENAME_MAX];
	// XXX: language specific
	snprintf (command, FILENAME_MAX, "%s --language-force=c++ --sort=no --fields=afKmnsz --c++-kinds=fnp -o \"%s\" \"%s\"", CTAGS_CMD, tmpnameIn, tmpnameOut);
	// I don't process any user input, so system() should be safe enough
	int sys = system (command);
	if (sys != EXIT_SUCCESS)
		printf("exuberant-ctags failed with error code: %d\n", sys);


	/* find the last entry, this is our current scope */
	tagFileInfo info;
	tagFile *tfile = tagsOpen (tmpnameIn, &info);
	tagEntry entry;
	const char* scope = NULL;
	char* type = NULL;
	unsigned long line = 0;
	if (tfile && info.status.opened)
	{
		if (tagsFirst (tfile, &entry) == TagSuccess)
		{
			do
			{
				scope = tagsField(&entry, "class");
				line = entry.address.lineNumber;
			}
			while (tagsNext (tfile, &entry) == TagSuccess);
		}
		tagsClose(tfile);
	}

	/* must be before the 'type = extract_type_qualifier()' code, which modifies scope */
	if (scope)
	{
		strncpy(sc->scope, scope, 255);
		sc->scope[255] = '\0';
	}

	/* get local definition (if any) */
	if (ident)
	{
		while (line > 1)
		{
			char c = *expr;
			if (c == '\0') //EOF
				break;
			else if (c == '\n')
			{
				line--;
				expr++;
				continue;
			}
			else
				expr++;
		}

		type = extract_type_qualifier(expr, ident);
		if (type && strlen(type))
		{
			strncpy(sc->localdef, type, 255);
			sc->scope[255] = '\0';
			free(type);
		}
		else
			strcpy(sc->localdef, "");
	}

exit:
	close (fdIn);
	unlink (tmpnameIn);
	close (fdOut);
	unlink (tmpnameOut);
}

int get_filedescriptor(const char* filename, char fullname[FILENAME_MAX])
{
	int fd = -1;
	struct stat fileStat;

	/* absolute path name */
	if (filename && filename[0] == '/')
	{
		fd = open(filename, O_RDONLY);
		strncpy (fullname, filename, FILENAME_MAX);
		fullname[FILENAME_MAX-1] = '\0';
		return fd;
	}

	/* relative pathname */
        List_item *it=config.cpp_includes->first;
	while(NULL!=it)
	{
		snprintf(fullname, FILENAME_MAX, "%s/%s", (char *)it->item, filename);
		fd = open(fullname, O_RDONLY);
		if (fd < 0) /* file not found */
                {
                        it=it->next;
			continue;
                }
		/* check for regular file */
		if (fstat(fd,&fileStat) < 0)
		{
			perror("Fstat Error");
			close(fd);
			fd=-1;
                        it=it->next;
			continue;
		}
		if (!(fileStat.st_mode&S_IFREG))
		{
			close(fd);
			fd=-1;
                        it=it->next;
			continue;
		}
		break;
	}
	return fd;
}

List *get_includes(const char *buf)
{
        char inc_str[10];
        State my_state=DEFAULT;
        int c,cl;
	size_t i;
        char *tmp;
	List *ret=alloc_list();
	if(NULL==ret)
            return NULL;
        buf--;
        while((c=*(++buf))!=0)
        {
                if(c==' ' || c== '\t')
                        continue;
                switch(my_state)
                {
                        case DEFAULT:
                                switch(c)
                                {
                                    case '#': my_state=INCLUDE;      continue;
                                    case '/': my_state=BEGINCOMMENT; continue;
                                    default:                         continue;
                                }
                                assert(0);
                        case INCLUDE:
                                switch(c)
                                {
                                        case 'i':
                                                strncpy(inc_str,buf,7);
						inc_str[7]=0;
						for(i=0;i<6&&*buf!=0;i++,buf++);
						if(buf==0)
							buf--;
                                                if(strcmp(inc_str,"include")==0)
                                                        my_state=OPENBRACE;
                                                else
                                                        my_state=ONELINECOMMENT;
                                                continue;
                                        default: my_state=ONELINECOMMENT; continue;
                                }
                                assert(0);
                        case OPENBRACE:
                                if((c=='<'&&(cl='>')) || (c=='"'&&(cl='"')))
                                {
                                        i=0;
                                        while((c=*(++buf))!=0 && c!=cl && c!='\n' && c!='\r')
						i++;
                                        if(c==cl)
                                        {
						if((tmp=malloc((i+1)*sizeof(char)))==NULL)
						{
							free_list(ret);
							return NULL;
						}
						strncpy(tmp,(char*)(buf-i),i);
                                                tmp[i]=0;
						add_to_list(ret,tmp);
                                        }
                                }
                                my_state=ONELINECOMMENT;
                                continue;
                        case BEGINCOMMENT:
                                switch(c)
                                {
                                        case '/': my_state=ONELINECOMMENT;   continue;
                                        case '*': my_state=MULTILINECOMMENT; continue;
                                        default:  my_state=DEFAULT;          continue;
                                }
                                assert(0);
                        case ONELINECOMMENT:
                                if(c=='\n' || c=='\r')
                                        my_state=DEFAULT;
                                continue;
                        case MULTILINECOMMENT:
                                if(c=='*')
                                        my_state=MULTILINECOMMENTEND;
                                continue;
                        case MULTILINECOMMENTEND:
                                if(c=='/')
                                        my_state=DEFAULT;
                                else if(c!='*')
                                        my_state=MULTILINECOMMENT;
                                continue;
                        default:
                                continue;
                }
                assert(0);
        }
	return ret;
}
