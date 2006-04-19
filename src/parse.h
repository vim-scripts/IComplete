#ifndef __PARSE_H__
#define __PARSE_H__

#include <stdbool.h>
#include "readtags.h"
#include "list.h"

typedef enum
{
	/* for now, these options are exclusive
	 * but define them as a bitfield for possible
	 * future improvements */
	ParseError =		0x00,
	AccessPointer = 	0x01,
	AccessMembers =		0x02,
	AccessStatic =		0x04,
	AccessGlobal =		0x08,
	AccessInFunction =	0x10
} Access;

typedef struct Expression
{
	Access access;
	char class[256];
	char function[256];
} Expression;

typedef struct Scope
{
	char scope[256]; // if the cursor is in a function Foo::bar(), store Foo as the scope
	char localdef[256]; // stores the type of a local definition or "" if none found
} Scope;


/*
 * for a given expr="str" and a string="std::string *str = new std::string("test");"
 * extract the type qualifier "std::string" and return the malloc'ed string.
 * The caller of this function is responsible to free the string
 */
char* extract_type_qualifier(const char *string, const char *expr);

/*
 * For a given expr "var->xy" or "var::" return `var'
 * and the AccessModifier (static, member,...)
 */
Expression parse_expression(const char *expr, Scope *scope);

/* fills the passed *exp pointer with information about the current expression
 * "QWidget w; w.getRect()." will return exp.class=QRect and exp.function=getRect
 */
bool get_type_of_expression(const char* expr, Expression *exp, Scope *scope);

/*
 * prettified tags entry look like these:
 *  rect:              QRect r;
 *  setRect():         void setRect (QRect &rect);
 *  @param ident_len defines the length of the left column
 *  or <0 to use a default value
 */
char* prettify_tag(tagEntry *entry, int ident_len);

/* 
 * extracts the scope at the end of a given code `expr'
 * and save the line of a local variable definition
 * Example:
 *
 * namespace Test {
 *   void MyClass::function(int var)
 *   {
 * 
 * If expr is the above code and ident is 'var', return:
 * scope = "Test::MyClass"
 * localdef = "int"
 */
void get_scope_and_locals (Scope *sc, const char* expr, const char *ident);

/*
 * @param: filename is a name like "string.h"
 * @return: the file descriptor (fd) and stores
 *          "/usr/include/string.h" in fullname
 */
int get_filedescriptor (const char* filename, char fullname[FILENAME_MAX]);

/*
 * @param: buf is a nullterminated string buffer
 * @return: list of all included files (z.b {"string.h","stdio.h",...})
 *          the strings and the list MUST be freed!
 */
List *get_includes(const char * buf);

#endif /* __PARSE_H__ */
