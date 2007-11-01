#ifndef __OPTIONS_H__
#define __OPTIONS_H__

#include <stdbool.h>
#include "list.h"

/**
 * \defgroup opt Option & configuration module
 * \brief This module reads in the configuration and parses command line
 *
 *@{*/

///Cache enum
typedef enum
{
    CACHE_NEVER  = 0,               ///< Don't build tagsfile
    CACHE_AUTO   = 1,               ///< Build tagsfile if needed
    CACHE_ALWAYS = 2,               ///< Build tagsfile
} cache_e;

extern char*   opt_filename;        ///< Filename of file to process
extern char*   opt_output;          ///< Filename to write to
extern long    opt_line;            ///< Line to process
extern long    opt_column;          ///< Column to process
extern char*   opt_listmembers;     ///< List members of this class
extern char*   opt_liststatic;      ///< ??!?
extern char*   opt_lang;            ///< Language (defaults to c++)
extern char*   opt_tagfile;         ///< Tag file (defaults to tags)
extern char*   opt_listfile;	    ///< file used to store filenames to feed to ctags
extern cache_e opt_cache;           ///< Wether we should build cache or not
extern const char* opt_progname;    ///< argv[0]

/// Holds configuration options
struct config_s
{
	List *cpp_includes;           ///< Include directories
	List *cpp_macros;             ///< C++ Macros
} config;


/**
 * parses command line options with getopt_long()
 * and stores the values in opt_* variables
 * \param argc argument cound
 * \param argv arguments
 */
void get_cmdine_opts(int argc, char** argv);

/**
 * reads config files into struct config
 * free_config has to be called!
 */
void read_config();

/** free()'s all memory which was malloc'ed by read_config() */
void free_config();

/*@}*/

#endif /* __OPTIONS_H__ */

