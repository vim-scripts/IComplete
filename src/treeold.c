#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h> /* MacOS X needs it included before regex.h */
#include <regex.h>
#include <string.h>
#include <assert.h>

#include <unistd.h> /* close() */
#include <fcntl.h> /* O_RDONLY */
#include <sys/stat.h>
#include <sys/mman.h> 

#include "tree.h"
#include "parse.h"
#include "options.h"
#include "readtags.h"
#include "error.h"
#include "treeold.h"

// added by maxime COSTE, see the remark in main.c about this
extern char* g_used_namespaces;
extern char* g_scope_namespaces;

//brc: new subroutine
static void add_tree_child (Tree* tree, const char* class)
{
/* check this class if not already in the tree to
 * avoid a hangup with a broken (=cirular) tags file */
	if (find_in_tree (tree, class, NULL) == 0)
	{
    		Tree *child = build_inheritance_tree(class);
                add_child_to_tree(tree,child);
	}
}

Tree* build_inheritance_tree (const char* class)
{
	Tree *root = alloc_tree(strdup(class));
	
	tagEntry entry;
	tagFileInfo info;
	tagFile *tfile = tagsOpen (opt_tagfile, &info);

//brc: split namespace part and classname
	char nbuffer[256];
	char* nspace=NULL;
	strncpy(nbuffer,class,256);
	nbuffer[255]='\0';
	char* cname=nbuffer;
	char* p=nbuffer;
	char* np=NULL;

 	while ((np=strstr(p,"::"))) 
		p=np+2;
	if (p!=nbuffer) {
		*(p-2)='\0'; 
		nspace=nbuffer;
		cname=p;
	}

	Tree *tree = root;

    // if namespaces were listed, try also to find classes in these namespaces
    // NOTE : there is a case where this wont work : 
    // namespace blah { namespace foo { ... } } in a header
    // using namespace blah; foo::thing in the file

    // first, scope namespaces, these may be chained (i.e. nsp1::nsp2::nsp3...)
    // but in order (I mean, if nsp1 appears first in the file, I dont think 
    // nsp2::nsp1 may exist
	if (g_scope_namespaces) {
		char buffer[512];
        // remember that namespaces is a zero separated string list terminated
        // by a double zero
        for (char* pointer = g_scope_namespaces; *pointer != 0; 
             pointer += strlen(pointer)+1) {
            // verify that the namespace is not already in the name being tested
            if (!strstr(class, pointer))
            {
                // please note that 254 + 2 + 256 = 511...
                strncpy (buffer, pointer, 254);
                strncat (buffer, "::", 2);
                strncat (buffer, class, 255);
#if DEBUG >= 2
                printf ("testing class %s\n", buffer);
#endif
                add_tree_child (tree, buffer);
            }
            else 
            {
                // if this namespace is already in the name, then, either 
                // next namespaces are already and then there is no point in 
                // continuing, either they arent, and if we continue we will 
                // test something like "nsp2::nsp1" even if nsp2 cannot be 
                // in nsp1
                break;
            }
        }
	}

    // used namespaces, these cannot be chained because they shall
    // be complete (i.e. you must type "using namespace nsp1::nsp2::nsp3)
    // and so in the namespace list I already got "nsp1::nsp2::nsp3"
    if (g_used_namespaces && !nspace)
    {
        char buffer[512];
        // remember that namespaces is a zero separated string list terminated
        // by a double zero
        for (char* pointer = g_used_namespaces; *pointer != 0; 
             pointer += strlen(pointer)+1) {
            // please note that 254 + 2 + 256 = 511...
            strncpy (buffer, pointer, 254);
            strncat (buffer, "::", 2);
            strncat (buffer, class, 255);
#if DEBUG >= 2
            printf ("testing class %s\n", buffer);
#endif
            add_tree_child (tree, buffer);
        }

    }

	int i = 0; // counters
	if (tfile && info.status.opened)
	{
		if (tagsFind (tfile, &entry, cname, TAG_OBSERVECASE | TAG_FULLMATCH) == TagSuccess)
		{
			do
			{
				//brc:follow typedefs ...
				if (!strcmp (entry.kind,"typedef") && entry.address.pattern )
				{
					const char *field_namespace = tagsField(&entry, "namespace"); 
					if ((nspace && field_namespace && !strcmp(field_namespace,nspace))||!nspace)
					{
						char pattern[256];
						strncpy(pattern,entry.address.pattern,256); pattern[255]='\0';
						char* p=strstr(pattern,"typedef");
						if (p && strstr(pattern,cname)) {
							p+=strlen("typedef");
							char* p1;
							if ((p1=strstr(p,"typename"))) p=p1+strlen("typename");
							if (strstr(p,"struct")) continue;
							if ((p=strtok(p," \n\t<"))) {
								free_tree(tree);
								if (nspace) {
									char buffer[512];
									strcpy(buffer,nspace);
									strcat(buffer,"::");
									strcat(buffer,p);
									return build_inheritance_tree(buffer);
								} else {
									return build_inheritance_tree(p);
								}
							}
						}
					}
				}
				/* need to get the entry for the class definition */
				if (!strcmp (entry.kind, "class") 
						|| !strcmp (entry.kind, "struct") 
						|| !strcmp (entry.kind, "union"))
				{
					/* and look for the entry "inherits" to see which are superclasses of class */
					for (; i < entry.fields.count; i++)
					{
						if (!strcmp(entry.fields.list[i].key, "inherits"))
						{
							/* split the inherits string to single classes for multiple inheritance */
							char *allinherits = strdup(entry.fields.list[i].value);
							char *buffer = NULL;
							char *inherit = strtok_r(allinherits, ",", &buffer);
							while (inherit != NULL)
							{
								add_tree_child(tree,inherit);

								/* next token */
								inherit = strtok_r (NULL, ",", &buffer);
							}
							free (allinherits);
							goto end;
						}
					}
					// no more superclasses
					goto end;
				}
			} while (tagsFindNext(tfile, &entry));
		}
end:
		tagsClose (tfile);
	}
	return root;
}

Tree* build_include_tree (Tree *start, const char* filename)
{
	static Tree *root = NULL;
	/* get the full name for the filename */
	if (strlen(filename) < 1)
		return NULL;
    
	char fullname[FILENAME_MAX];
        fullname[0]=0;
	int fd = get_filedescriptor(filename, fullname);
	if (fd < 0)
	{
		fprintf (stderr, "TREE: %s NOT found\n", filename);
		close (fd);
		return NULL;
	}
    
	/* and store it in a Tree* pointer */
	Tree *tree, *child;
    
	if (!start)
	{
		tree=alloc_tree(strdup (fullname));
	}
	else
		tree = start;
	if (!root)
		root = tree;
    
    
	/* find include'ed files and add them to tree */
	struct stat my_stat;
	fstat(fd,&my_stat);
	char *buf;
	if((buf=malloc((my_stat.st_size+1)*sizeof(char)))==NULL)
		return tree;
	if(read(fd,buf,my_stat.st_size)!=my_stat.st_size)
	{
		free(buf);
		return tree;
	}
	buf[my_stat.st_size]=0;
	List *includes=get_includes(buf);
	if(includes!=NULL)
	{
		List_item *it=includes->first;
		while(NULL!=it)
		{
			int temp_fd = get_filedescriptor(it->item, fullname);
			if (temp_fd < 0 )
			{
			    it=it->next;
			    continue;
			}
			else
			    close(temp_fd);
			/* only insert this include if not already present to
                * avoid cirular inclusion */
			if (find_in_tree (root, fullname, NULL) == NOT_FOUND)
			{
				child = add_to_tree(tree,strdup(fullname));
				build_include_tree(child, (char *)it->item);
			}
			it=it->next;
		}
		free_list(includes);
	}
    
	close(fd);
	
	return tree;
}

bool is_member_of_scope (const tagEntry *entry, const Scope *scope, const Tree *tree)
{
	const char *field_class = tagsField(entry, "class");
	const char *field_struct = tagsField(entry, "struct");
	const char *field_union = tagsField(entry, "union");
	//const char *field_namespace = tagsField(entry, "namespace"); 
	const char *field_kind = tagsField(entry, "kind"); 
	const char *field_access = tagsField(entry, "access");

	const char *tag_class = NULL;
	if (field_class)
		tag_class = field_class;
	else if (field_struct)
		tag_class = field_struct;
	else if (field_union)
		tag_class = field_union;
	else
		return false;

	/* tag must be part of (inherited) class to show it */
	Match match_tag = find_in_tree (tree, tag_class,NULL);
	if (match_tag == NOT_FOUND)
		return false;
 	if (field_union)
		return true;	
	if (!field_access)
		return false;
	if(!strcmp(field_access, "public") || !strcmp(field_access, "default")) /* default for java */
		return true;


	/* if access is not public, current scope must be in one of the
	 * (inherited) classes */
	Match match_curscope = find_in_tree (tree, scope->scope,NULL);
	if (!strcmp(field_access, "protected") && match_curscope >= DIRECT_HIT)
			return true;
	else if (!strcmp(field_access, "private") && match_curscope == DIRECT_HIT)
			return true;

	return false;
}

void find_entries (const Expression *exp, const Scope *scope)
{
	tagEntry entry;
	tagFileInfo info;
	tagFile *tfile = tagsOpen (opt_tagfile, &info);
	char lasttag[256] = ""; /* store last tag to avoid duplicates */

	/* write tags to file instead of stdout */
	FILE *output = stdout;
	if (opt_output)
	{
		output = fopen(opt_output, "w");
		if (!output)
		{
			perror (opt_output);
			return;
		}
	}

	/* create an inheritance tree of our class or current scope, depending what we want to complete */
	Tree *tree = NULL;
	if (exp->access & AccessMembers || exp->access & AccessPointer || exp->access & AccessStatic || exp->access & AccessInFunction)
		tree = build_inheritance_tree(exp->class);
	else if (exp->access & AccessGlobal)
		tree = build_inheritance_tree(scope->scope);

	if (!tree)
		bailout ("Couldn't build inheritance tree\n");


	/* parse the tags file */
	if (tfile && info.status.opened)
	{
		/* we can do a binary search for function definitions */
		if (exp->access == AccessInFunction && tagsFind (tfile, &entry, exp->function, TAG_FULLMATCH | TAG_OBSERVECASE) == TagSuccess)
		{
			do
			{
				bool show_this_tag = false;
				const char *field_kind = tagsField(&entry, "kind");


				if (field_kind && (!strcmp(field_kind, "member") || !strcmp(field_kind, "method") || !strcmp(field_kind, "field")))
					show_this_tag = is_member_of_scope (&entry, scope, tree);
				else if (!strcmp(field_kind, "function") || !strcmp(field_kind, "prototype"))
				{
					if (!strcmp(exp->class, "")) // only a function, not method of a class
						show_this_tag = true;
					else
						show_this_tag = is_member_of_scope (&entry, scope, tree);
				}
				if (!show_this_tag)
					continue;

				/* output the tag if it is valid and no duplicate either to stdout
				 * or to a file if the -o flag was given */
				const char *tag = NULL;
				tag = prettify_tag(&entry, strlen(entry.name) + 5);
				if (tag)
					fprintf(output, "%s\n", tag);
			}
			while ( tagsFindNext (tfile, &entry) == TagSuccess);
		}

		// otherwise loop through all tags
		// and filter those which match one of our inherited class names
		else if (tagsFirst (tfile, &entry) == TagSuccess)
		{
			do
			{
				bool show_this_tag = false;
				const char *field_kind = tagsField(&entry, "kind");
				const char *field_class = tagsField(&entry, "class");
				const char *field_struct = tagsField(&entry, "struct");
				const char *field_union = tagsField(&entry, "union");
				const char *field_namespace = tagsField(&entry, "namespace"); 
				const char *field_access = tagsField(&entry, "access");

				switch (exp->access)
				{
				/*
				 * find MEMBERS of classes/structs
				 */
				case AccessMembers:
				case AccessPointer:
					if (opt_lang && !strcmp(opt_lang,"c++")) // static has different meaning in java e.g.
					{
						if (strstr(entry.address.pattern, "static "))
							continue;
						/* Qt 3 defines this, and checking for it does not harm other users */
						if (strstr(entry.address.pattern, "QT_STATIC_CONST "))
							continue;
					}

					// don't show destructors, can't call them directly anyways.
					if (entry.name[0] == '~')
					{
						show_this_tag = false;
						break;
					}

					// TODO: namespace support?
					if (field_kind && (!strcmp(field_kind, "member") || !strcmp(field_kind, "function") || !strcmp(field_kind, "field") ||
								!strcmp(field_kind, "prototype") || !strcmp(field_kind, "method")))

						show_this_tag = is_member_of_scope (&entry, scope, tree);
					break;
				/*
				 * find STATIC functions/variables (everything after a ::)
				 */
				case AccessStatic:
					/* when we find a tag, the following conditions must be met, to show the tag:
					 * - must be part of a class or namespace
					 * - must contain "static" on the line
					 * - must be "public"
					 */
					if (field_class)
					{
						if (!strcmp (exp->class, field_class) &&
								field_access && (!strcmp(field_access, "public") || !strcmp(field_access, "default")))
							/* XXX: Workaround ctags limitation and try to find out
							 * ourselves, if this tag is a static one, if it 
							 * has "static" in the pattern */
							if (strstr(entry.address.pattern, "static ") ||
									strstr(entry.address.pattern, "QT_STATIC_CONST "))
								show_this_tag = true;
					}
					else if (field_namespace)
					{
						if (!strcmp (exp->class, field_namespace))
							show_this_tag = true;
					}
					break;

				/*
				 * find GLOBAL functions/variables/defines
				 */
				case AccessGlobal:
					/* everthing which is not part of a class or struct matches */
					if (!field_class && !field_struct && !field_union)
					{
						show_this_tag = true;
					}
					else /* also show locals of current scope */
					{
						show_this_tag = is_member_of_scope (&entry, scope, tree);
					}
					break;

				default:
					/* things like if() while() for(), etc. */
					break;
				}

				/* output the tag if it is valid and no duplicate either to stdout
				 * or to a file if the -o flag was given */
				if (show_this_tag && (exp->access == AccessInFunction || strcmp(entry.name, lasttag)))
				{
					const char *tag = NULL;
					if (exp->access & AccessInFunction)
						tag = prettify_tag(&entry, strlen(entry.name) + 5);
					else
					{
						strncpy (lasttag, entry.name, 255);
						lasttag[255] = '\0';
						tag = prettify_tag(&entry, -1);
					}
					
					if (tag)
						fprintf(output, "%s\n", tag);
				}
			}
			while ( tagsNext (tfile, &entry) == TagSuccess);
		}
		tagsClose(tfile);
	}
	else
	{
		char buf[BUFSIZ];
		snprintf(buf, BUFSIZ, "Could not open `%s' file. You should be able to build a valid one by running this command:\n\nicomplete -l 1 -c 1 <filename>\n\n<filename> should be a source file which contains all #include files you want to have in the tags file", opt_tagfile);
		bailout(buf);
	}

	free_tree (tree);
	if (output)
		fclose(output);
}

