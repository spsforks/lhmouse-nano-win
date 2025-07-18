/**************************************************************************
 *   rcfile.c  --  This file is part of GNU nano.                         *
 *                                                                        *
 *   Copyright (C) 2001-2011, 2013-2025 Free Software Foundation, Inc.    *
 *   Copyright (C) 2014 Mike Frysinger                                    *
 *   Copyright (C) 2019 Brand Huntsman                                    *
 *   Copyright (C) 2014-2021 Benno Schulenberg                            *
 *                                                                        *
 *   GNU nano is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published    *
 *   by the Free Software Foundation, either version 3 of the License,    *
 *   or (at your option) any later version.                               *
 *                                                                        *
 *   GNU nano is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see https://gnu.org/licenses/.     *
 *                                                                        *
 **************************************************************************/

#include "prototypes.h"

#ifdef ENABLE_NANORC

#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <string.h>
#include <unistd.h>
#include <shlobj.h>

#ifndef RCFILE_NAME
#define HOME_RC_NAME  ".nanorc"
#define RCFILE_NAME  "nanorc"
#else
#define HOME_RC_NAME  RCFILE_NAME
#endif

static const rcoption rcopts[] = {
	{"boldtext", BOLD_TEXT},
#ifdef ENABLE_JUSTIFY
	{"brackets", 0},
#endif
#ifdef ENABLE_WRAPPING
	{"breaklonglines", BREAK_LONG_LINES},
#endif
	{"casesensitive", CASE_SENSITIVE},
	{"constantshow", CONSTANT_SHOW},
#ifdef ENABLED_WRAPORJUSTIFY
	{"fill", 0},
#endif
#ifdef ENABLE_HISTORIES
	{"historylog", HISTORYLOG},
#endif
#ifdef ENABLE_LINENUMBERS
	{"linenumbers", LINE_NUMBERS},
#endif
#ifdef HAVE_LIBMAGIC
	{"magic", USE_MAGIC},
#endif
#ifdef ENABLE_MOUSE
	{"mouse", USE_MOUSE},
#endif
#ifdef ENABLE_MULTIBUFFER
	{"multibuffer", MULTIBUFFER},
#endif
	{"nohelp", NO_HELP},
	{"nonewlines", NO_NEWLINES},
#ifdef ENABLE_WRAPPING
	{"nowrap", NO_WRAP},  /* Deprecated; remove in 2027. */
#endif
#ifdef ENABLE_OPERATINGDIR
	{"operatingdir", 0},
#endif
#ifdef ENABLE_HISTORIES
	{"positionlog", POSITIONLOG},
#endif
	{"preserve", PRESERVE},
#ifdef ENABLE_JUSTIFY
	{"punct", 0},
	{"quotestr", 0},
#endif
	{"quickblank", QUICK_BLANK},
	{"rawsequences", RAW_SEQUENCES},
	{"rebinddelete", REBIND_DELETE},
	{"regexp", USE_REGEXP},
	{"saveonexit", SAVE_ON_EXIT},
#ifdef ENABLE_SPELLER
	{"speller", 0},
#endif
#ifndef NANO_TINY
	{"afterends", AFTER_ENDS},
	{"allow_insecure_backup", INSECURE_BACKUP},
	{"atblanks", AT_BLANKS},
	{"autoindent", AUTOINDENT},
	{"backup", MAKE_BACKUP},
	{"backupdir", 0},
	{"bookstyle", BOOKSTYLE},
	{"colonparsing", COLON_PARSING},
	{"cutfromcursor", CUT_FROM_CURSOR},
	{"emptyline", EMPTY_LINE},
	{"guidestripe", 0},
	{"indicator", INDICATOR},
	{"jumpyscrolling", JUMPY_SCROLLING},
	{"locking", LOCKING},
	{"matchbrackets", 0},
	{"minibar", MINIBAR},
	{"noconvert", NO_CONVERT},
	{"showcursor", SHOW_CURSOR},
	{"smarthome", SMART_HOME},
	{"softwrap", SOFTWRAP},
	{"stateflags", STATEFLAGS},
	{"tabsize", 0},
	{"tabstospaces", TABS_TO_SPACES},
	{"trimblanks", TRIM_BLANKS},
	{"unix", MAKE_IT_UNIX},
	{"whitespace", 0},
	{"whitespacedisplay", WHITESPACE_DISPLAY},
	{"wordbounds", WORD_BOUNDS},
	{"wordchars", 0},
	{"zap", LET_THEM_ZAP},
	{"zero", ZERO},
#endif
#ifdef ENABLE_COLOR
	{"titlecolor", 0},
	{"numbercolor", 0},
	{"stripecolor", 0},
	{"scrollercolor", 0},
	{"selectedcolor", 0},
	{"spotlightcolor", 0},
	{"minicolor", 0},
	{"promptcolor", 0},
	{"statuscolor", 0},
	{"errorcolor", 0},
	{"keycolor", 0},
	{"functioncolor", 0},
#endif
	{NULL, 0}
};

static size_t lineno = 0;
		/* The line number of the last encountered error. */
static char *nanorc = NULL;
		/* The path to the rcfile we're parsing. */
#ifdef ENABLE_COLOR
static bool opensyntax = FALSE;
		/* Whether we're allowed to add to the last syntax.  When a file ends,
		 * or when a new syntax command is seen, this bool becomes FALSE. */
static syntaxtype *live_syntax;
		/* The syntax that is currently being parsed. */
static bool seen_color_command = FALSE;
		/* Whether a syntax definition contains any color commands. */
static colortype *lastcolor = NULL;
		/* The end of the color list for the current syntax. */
#endif
#endif /* ENABLE_NANORC */

#if defined(ENABLE_NANORC) || defined(ENABLE_HISTORIES)
static linestruct *errors_head = NULL;
static linestruct *errors_tail = NULL;
		/* Beginning and end of a list of errors in rcfiles, if any. */

/* Send the gathered error messages (if any) to the terminal. */
void display_rcfile_errors(void)
{
	for (linestruct *error = errors_head; error != NULL; error = error->next)
		fprintf(stderr, "%s\n", error->data);
}

#define MAXSIZE  (PATH_MAX + 200)

/* Store the given error message in a linked list, to be printed upon exit. */
void jot_error(const char *msg, ...)
{
	linestruct *error = make_new_node(errors_tail);
	char textbuf[MAXSIZE];
	int length = 0;
	va_list ap;

	if (errors_head == NULL)
		errors_head = error;
	else
		errors_tail->next = error;
	errors_tail = error;

	if (startup_problem == NULL) {
#ifdef ENABLE_NANORC
		if (nanorc != NULL) {
			snprintf(textbuf, MAXSIZE, _("Mistakes in '%s'"), nanorc);
			startup_problem = copy_of(textbuf);
		} else
#endif
			startup_problem = copy_of(_("Problems with history file"));
	}
#ifdef ENABLE_NANORC
	if (lineno > 0)
		length = snprintf(textbuf, MAXSIZE, _("Error in %s on line %zu: "),
											nanorc, lineno);
#endif
	va_start(ap, msg);
	length += vsnprintf(textbuf + length, MAXSIZE - length, _(msg), ap);
	va_end(ap);

	error->data = nmalloc(length + 1);
	sprintf(error->data, "%s", textbuf);
}
#endif /* ENABLE_NANORC || ENABLE_HISTORIES */

#ifdef ENABLE_NANORC
/* Interpret a function string given in the rc file, and return a
 * shortcut record with the corresponding function filled in. */
keystruct *strtosc(const char *input)
{
	keystruct *s = nmalloc(sizeof(keystruct));

#ifndef NANO_TINY
	s->toggle = 0;
#endif

	if (!strcmp(input, "cancel"))
		s->func = do_cancel;
#ifdef ENABLE_HELP
	else if (!strcmp(input, "help"))
		s->func = do_help;
#endif
	else if (!strcmp(input, "exit"))
		s->func = do_exit;
	else if (!strcmp(input, "discardbuffer"))
		s->func = discard_buffer;
	else if (!strcmp(input, "writeout"))
		s->func = do_writeout;
	else if (!strcmp(input, "savefile"))
		s->func = do_savefile;
	else if (!strcmp(input, "insert"))
		s->func = do_insertfile;
	else if (!strcmp(input, "whereis"))
		s->func = do_search_forward;
	else if (!strcmp(input, "wherewas"))
		s->func = do_search_backward;
	else if (!strcmp(input, "findprevious"))
		s->func = do_findprevious;
	else if (!strcmp(input, "findnext"))
		s->func = do_findnext;
	else if (!strcmp(input, "replace"))
		s->func = do_replace;
	else if (!strcmp(input, "cut"))
		s->func = cut_text;
	else if (!strcmp(input, "copy"))
		s->func = copy_text;
	else if (!strcmp(input, "paste"))
		s->func = paste_text;
#ifndef NANO_TINY
	else if (!strcmp(input, "execute"))
		s->func = do_execute;
	else if (!strcmp(input, "cutrestoffile"))
		s->func = cut_till_eof;
	else if (!strcmp(input, "zap"))
		s->func = zap_text;
	else if (!strcmp(input, "mark"))
		s->func = do_mark;
#endif
#ifdef ENABLE_SPELLER
	else if (!strcmp(input, "tospell") ||
	         !strcmp(input, "speller"))
		s->func = do_spell;
#endif
#ifdef ENABLE_LINTER
	else if (!strcmp(input, "linter"))
		s->func = do_linter;
#endif
#ifdef ENABLE_FORMATTER
	else if (!strcmp(input, "formatter"))
		s->func = do_formatter;
#endif
	else if (!strcmp(input, "location"))
		s->func = report_cursor_position;
	else if (!strcmp(input, "gotoline"))
		s->func = do_gotolinecolumn;
#ifdef ENABLE_JUSTIFY
	else if (!strcmp(input, "justify"))
		s->func = do_justify;
	else if (!strcmp(input, "fulljustify"))
		s->func = do_full_justify;
	else if (!strcmp(input, "beginpara"))
		s->func = to_para_begin;
	else if (!strcmp(input, "endpara"))
		s->func = to_para_end;
#endif
#ifdef ENABLE_COMMENT
	else if (!strcmp(input, "comment"))
		s->func = do_comment;
#endif
#ifdef ENABLE_WORDCOMPLETION
	else if (!strcmp(input, "complete"))
		s->func = complete_a_word;
#endif
#ifndef NANO_TINY
	else if (!strcmp(input, "indent"))
		s->func = do_indent;
	else if (!strcmp(input, "unindent"))
		s->func = do_unindent;
	else if (!strcmp(input, "chopwordleft"))
		s->func = chop_previous_word;
	else if (!strcmp(input, "chopwordright"))
		s->func = chop_next_word;
	else if (!strcmp(input, "findbracket"))
		s->func = do_find_bracket;
	else if (!strcmp(input, "wordcount"))
		s->func = count_lines_words_and_characters;
	else if (!strcmp(input, "recordmacro"))
		s->func = record_macro;
	else if (!strcmp(input, "runmacro"))
		s->func = run_macro;
	else if (!strcmp(input, "anchor"))
		s->func = put_or_lift_anchor;
	else if (!strcmp(input, "prevanchor"))
		s->func = to_prev_anchor;
	else if (!strcmp(input, "nextanchor"))
		s->func = to_next_anchor;
	else if (!strcmp(input, "undo"))
		s->func = do_undo;
	else if (!strcmp(input, "redo"))
		s->func = do_redo;
	else if (!strcmp(input, "suspend"))
		s->func = do_suspend;
#endif
	else if (!strcmp(input, "left") ||
	         !strcmp(input, "back"))
		s->func = do_left;
	else if (!strcmp(input, "right") ||
	         !strcmp(input, "forward"))
		s->func = do_right;
	else if (!strcmp(input, "up") ||
	         !strcmp(input, "prevline"))
		s->func = do_up;
	else if (!strcmp(input, "down") ||
	         !strcmp(input, "nextline"))
		s->func = do_down;
#if !defined(NANO_TINY) || defined(ENABLE_HELP)
	else if (!strcmp(input, "scrollup"))
		s->func = do_scroll_up;
	else if (!strcmp(input, "scrolldown"))
		s->func = do_scroll_down;
#endif
	else if (!strcmp(input, "prevword"))
		s->func = to_prev_word;
	else if (!strcmp(input, "nextword"))
		s->func = to_next_word;
	else if (!strcmp(input, "home"))
		s->func = do_home;
	else if (!strcmp(input, "end"))
		s->func = do_end;
	else if (!strcmp(input, "prevblock"))
		s->func = to_prev_block;
	else if (!strcmp(input, "nextblock"))
		s->func = to_next_block;
#ifndef NANO_TINY
	else if (!strcmp(input, "toprow"))
		s->func = to_top_row;
	else if (!strcmp(input, "bottomrow"))
		s->func = to_bottom_row;
	else if (!strcmp(input, "center"))
		s->func = do_center;
	else if (!strcmp(input, "cycle"))
		s->func = do_cycle;
#endif
	else if (!strcmp(input, "pageup") ||
	         !strcmp(input, "prevpage"))
		s->func = do_page_up;
	else if (!strcmp(input, "pagedown") ||
	         !strcmp(input, "nextpage"))
		s->func = do_page_down;
	else if (!strcmp(input, "firstline"))
		s->func = to_first_line;
	else if (!strcmp(input, "lastline"))
		s->func = to_last_line;
#ifdef ENABLE_MULTIBUFFER
	else if (!strcmp(input, "prevbuf"))
		s->func = switch_to_prev_buffer;
	else if (!strcmp(input, "nextbuf"))
		s->func = switch_to_next_buffer;
#endif
	else if (!strcmp(input, "verbatim"))
		s->func = do_verbatim_input;
	else if (!strcmp(input, "tab"))
		s->func = do_tab;
	else if (!strcmp(input, "enter"))
		s->func = do_enter;
	else if (!strcmp(input, "delete"))
		s->func = do_delete;
	else if (!strcmp(input, "backspace"))
		s->func = do_backspace;
	else if (!strcmp(input, "refresh"))
		s->func = full_refresh;
	else if (!strcmp(input, "casesens"))
		s->func = case_sens_void;
	else if (!strcmp(input, "regexp"))
		s->func = regexp_void;
	else if (!strcmp(input, "backwards"))
		s->func = backwards_void;
	else if (!strcmp(input, "flipreplace"))
		s->func = flip_replace;
	else if (!strcmp(input, "flipgoto"))
		s->func = flip_goto;
#ifdef ENABLE_HISTORIES
	else if (!strcmp(input, "older"))
		s->func = get_older_item;
	else if (!strcmp(input, "newer"))
		s->func = get_newer_item;
#endif
#ifndef NANO_TINY
	else if (!strcmp(input, "dosformat"))
		s->func = dos_format;
	else if (!strcmp(input, "macformat"))
		s->func = mac_format;
	else if (!strcmp(input, "append"))
		s->func = append_it;
	else if (!strcmp(input, "prepend"))
		s->func = prepend_it;
	else if (!strcmp(input, "backup"))
		s->func = back_it_up;
	else if (!strcmp(input, "flipexecute"))
		s->func = flip_execute;
	else if (!strcmp(input, "flippipe"))
		s->func = flip_pipe;
	else if (!strcmp(input, "flipconvert"))
		s->func = flip_convert;
#endif
#ifdef ENABLE_MULTIBUFFER
	else if (!strcmp(input, "flipnewbuffer"))
		s->func = flip_newbuffer;
#endif
#ifdef ENABLE_BROWSER
	else if (!strcmp(input, "tofiles") ||
	         !strcmp(input, "browser"))
		s->func = to_files;
	else if (!strcmp(input, "gotodir"))
		s->func = goto_dir;
	else if (!strcmp(input, "firstfile"))
		s->func = to_first_file;
	else if (!strcmp(input, "lastfile"))
		s->func = to_last_file;
#endif
	else {
#ifndef NANO_TINY
		s->func = do_toggle;
		if (!strcmp(input, "nohelp"))
			s->toggle = NO_HELP;
		else if (!strcmp(input, "zero"))
			s->toggle = ZERO;
		else if (!strcmp(input, "constantshow"))
			s->toggle = CONSTANT_SHOW;
		else if (!strcmp(input, "softwrap"))
			s->toggle = SOFTWRAP;
#ifdef ENABLE_LINENUMBERS
		else if (!strcmp(input, "linenumbers"))
			s->toggle = LINE_NUMBERS;
#endif
		else if (!strcmp(input, "whitespacedisplay"))
			s->toggle = WHITESPACE_DISPLAY;
#ifdef ENABLE_COLOR
		else if (!strcmp(input, "nosyntax"))
			s->toggle = NO_SYNTAX;
#endif
		else if (!strcmp(input, "smarthome"))
			s->toggle = SMART_HOME;
		else if (!strcmp(input, "autoindent"))
			s->toggle = AUTOINDENT;
		else if (!strcmp(input, "cutfromcursor"))
			s->toggle = CUT_FROM_CURSOR;
#ifdef ENABLE_WRAPPING
		else if (!strcmp(input, "breaklonglines"))
			s->toggle = BREAK_LONG_LINES;
#endif
		else if (!strcmp(input, "tabstospaces"))
			s->toggle = TABS_TO_SPACES;
#ifdef ENABLE_MOUSE
		else if (!strcmp(input, "mouse"))
			s->toggle = USE_MOUSE;
#endif
		else
#endif /* !NANO_TINY */
		{
			free(s);
			return NULL;
		}
	}
	return s;
}

#define NUMBER_OF_MENUS  16
char *menunames[NUMBER_OF_MENUS] = { "main", "search", "replace", "replacewith",
									"yesno", "gotoline", "writeout", "insert",
									"execute", "help", "spell", "linter",
									"browser", "whereisfile", "gotodir",
									"all" };
int menusymbols[NUMBER_OF_MENUS] = { MMAIN, MWHEREIS, MREPLACE, MREPLACEWITH,
									MYESNO, MGOTOLINE, MWRITEFILE, MINSERTFILE,
									MEXECUTE, MHELP, MSPELL, MLINTER,
									MBROWSER, MWHEREISFILE, MGOTODIR,
									MMOST|MBROWSER|MHELP|MYESNO };

/* Return the symbol that corresponds to the given menu name. */
int name_to_menu(const char *name)
{
	int index = -1;

	while (++index < NUMBER_OF_MENUS)
		if (strcmp(name, menunames[index]) == 0)
			return menusymbols[index];

	return 0;
}

/* Return the name that corresponds to the given menu symbol. */
char *menu_to_name(int menu)
{
	int index = -1;

	while (++index < NUMBER_OF_MENUS)
		if (menusymbols[index] == menu)
			return menunames[index];

	return "boooo";
}

/* Parse the next word from the string (if any), null-terminate it,
 * and return a pointer to the next word.  The returned pointer will
 * point to '\0' when end-of-line was reached. */
char *parse_next_word(char *ptr)
{
	while (!isblank((unsigned char)*ptr) && *ptr != '\0')
		ptr++;

	if (*ptr == '\0')
		return ptr;

	/* Null-terminate and advance ptr. */
	*ptr++ = '\0';

	while (isblank((unsigned char)*ptr))
		ptr++;

	return ptr;
}

/* Parse an argument (optionally enveloped in double quotes).  When the
 * argument starts with a ", then the last " of the line indicates its
 * end -- meaning that an argument can contain "'s either way. */
char *parse_argument(char *ptr)
{
	const char *ptr_save = ptr;
	char *last_quote = NULL;

	if (*ptr != '"')
		return parse_next_word(ptr);

	while (*ptr != '\0') {
		if (*++ptr == '"')
			last_quote = ptr;
	}

	if (last_quote == NULL) {
		jot_error(N_("Argument '%s' has an unterminated \""), ptr_save);
		return NULL;
	}

	*last_quote = '\0';
	ptr = last_quote + 1;

	while (isblank((unsigned char)*ptr))
		ptr++;

	return ptr;
}

#ifdef ENABLE_COLOR
/* Advance over one regular expression in the line starting at ptr,
 * null-terminate it, and return a pointer to the succeeding text. */
char *parse_next_regex(char *ptr)
{
	char *starting_point = ptr;

	if (*(ptr - 1) != '"') {
		jot_error(N_("Regex strings must begin and end with a \" character"));
		return NULL;
	}

	/* Continue until the end of the line, or until a double quote followed
	 * by end-of-line or a blank. */
	while (*ptr != '\0' && (*ptr != '"' ||
						(ptr[1] != '\0' && !isblank((unsigned char)ptr[1]))))
		ptr++;

	if (*ptr == '\0') {
		jot_error(N_("Regex strings must begin and end with a \" character"));
		return NULL;
	}

	if (ptr == starting_point) {
		jot_error(N_("Empty regex string"));
		return NULL;
	}

	/* Null-terminate the regex and skip until the next non-blank. */
	*ptr++ = '\0';

	while (isblank((unsigned char)*ptr))
		ptr++;

	return ptr;
}

/* Compile the given regular expression and store the result in packed (when
 * this pointer is not NULL).  Return TRUE when the expression is valid. */
bool compile(const char *expression, int rex_flags, regex_t **packed)
{
	regex_t *compiled = nmalloc(sizeof(regex_t));
	int outcome = regcomp(compiled, expression, rex_flags);

	if (outcome != 0) {
		size_t length = regerror(outcome, compiled, NULL, 0);
		char *message = nmalloc(length);

		regerror(outcome, compiled, message, length);
		jot_error(N_("Bad regex \"%s\": %s"), expression, message);
		free(message);

		regfree(compiled);
		free(compiled);
	} else
		*packed = compiled;

	return (outcome == 0);
}

/* Parse the next syntax name and its possible extension regexes from the
 * line at ptr, and add it to the global linked list of color syntaxes. */
void begin_new_syntax(char *ptr)
{
	char *nameptr = ptr;

	/* Check that the syntax name is not empty. */
	if (*ptr == '\0' || (*ptr == '"' &&
						(*(ptr + 1) == '\0' || *(ptr + 1) == '"'))) {
		jot_error(N_("Missing syntax name"));
		return;
	}

	ptr = parse_next_word(ptr);

	/* Check that quotes around the name are either paired or absent. */
	if ((*nameptr == '\x22') ^ (nameptr[strlen(nameptr) - 1] == '\x22')) {
		jot_error(N_("Unpaired quote in syntax name"));
		return;
	}

	/* If the name is quoted, strip the quotes. */
	if (*nameptr == '\x22') {
		nameptr++;
		nameptr[strlen(nameptr) - 1] = '\0';
	}

	/* Redefining the "none" syntax is not allowed. */
	if (strcmp(nameptr, "none") == 0) {
		jot_error(N_("The \"none\" syntax is reserved"));
		return;
	}

	/* Initialize a new syntax struct. */
	live_syntax = nmalloc(sizeof(syntaxtype));
	live_syntax->name = copy_of(nameptr);
	live_syntax->filename = copy_of(nanorc);
	live_syntax->lineno = lineno;
	live_syntax->augmentations = NULL;
	live_syntax->extensions = NULL;
	live_syntax->headers = NULL;
	live_syntax->magics = NULL;
	live_syntax->linter = NULL;
	live_syntax->formatter = NULL;
	live_syntax->tabstring = NULL;
#ifdef ENABLE_COMMENT
	live_syntax->comment = copy_of(GENERAL_COMMENT_CHARACTER);
#endif
	live_syntax->color = NULL;
	live_syntax->multiscore = 0;

	/* Hook the new syntax in at the top of the list. */
	live_syntax->next = syntaxes;
	syntaxes = live_syntax;

	opensyntax = TRUE;
	seen_color_command = FALSE;

	/* The default syntax should have no associated extensions. */
	if (strcmp(live_syntax->name, "default") == 0 && *ptr != '\0') {
		jot_error(N_("The \"default\" syntax does not accept extensions"));
		return;
	}

	/* If there seem to be extension regexes, pick them up. */
	if (*ptr != '\0')
		grab_and_store("extension", ptr, &live_syntax->extensions);
}
#endif /* ENABLE_COLOR */

/* Verify that a syntax definition contains at least one color command. */
void check_for_nonempty_syntax(void)
{
#ifdef ENABLE_COLOR
	if (opensyntax && !seen_color_command) {
		size_t current_lineno = lineno;

		lineno = live_syntax->lineno;
		jot_error(N_("Syntax \"%s\" has no color commands"), live_syntax->name);
		lineno = current_lineno;
	}

	opensyntax = FALSE;
#endif
}

/* Return TRUE when the given function is present in almost all menus. */
bool is_universal(void (*func)(void))
{
	return (func == do_left || func == do_right ||
		func == do_home || func == do_end ||
#ifndef NANO_TINY
		func == to_prev_word || func == to_next_word ||
#endif
		func == do_delete || func == do_backspace ||
		func == cut_text || func == paste_text ||
		func == do_tab || func == do_enter || func == do_verbatim_input);
}

/* Bind or unbind a key combo, to or from a function. */
void parse_binding(char *ptr, bool dobind)
{
	char *keycopy, *keyptr, *menuptr;
	keystruct *newsc = NULL;
	char *funcptr = NULL;
	int keycode, menu;
	int mask = 0;

	check_for_nonempty_syntax();

	if (*ptr == '\0') {
		jot_error(N_("Missing key name"));
		return;
	}

	keyptr = ptr;
	ptr = parse_next_word(ptr);
	keycopy = copy_of(keyptr);

	/* Uppercase either the second or the first character of the key name. */
	if (keycopy[0] == '^') {
		if ('a' <= keycopy[1] && keycopy[1] <= 'z')
			keycopy[1] &= 0x5F;
	} else if ('a' <= keycopy[0] && keycopy[0] <= 'z')
		keycopy[0] &= 0x5F;

	/* Verify that the key name is not too short. */
	if (keycopy[1] == '\0' || (keycopy[0] == 'M' && keycopy[2] == '\0')) {
		jot_error(N_("Key name %s is invalid"), keycopy);
		goto free_things;
	}

	keycode = keycode_from_string(keycopy);

	if (keycode < 0) {
		jot_error(N_("Key name %s is invalid"), keycopy);
		goto free_things;
	}

	if (dobind) {
		funcptr = ptr;
		ptr = parse_argument(ptr);

		if (funcptr[0] == '\0') {
			jot_error(N_("Must specify a function to bind the key to"));
			goto free_things;
		} else if (ptr == NULL)
			goto free_things;
	}

	menuptr = ptr;
	ptr = parse_next_word(ptr);

	if (menuptr[0] == '\0') {
		/* TRANSLATORS: Do not translate the word "all". */
		jot_error(N_("Must specify a menu (or \"all\") "
						"in which to bind/unbind the key"));
		goto free_things;
	}

	menu = name_to_menu(menuptr);
	if (menu == 0) {
		jot_error(N_("Unknown menu: %s"), menuptr);
		goto free_things;
	}

	if (dobind) {
		/* If the thing to bind starts with a double quote, it is a string,
		 * otherwise it is the name of a function. */
		if (*funcptr == '"') {
			newsc = nmalloc(sizeof(keystruct));
			newsc->func = (functionptrtype)implant;
			newsc->expansion = copy_of(funcptr + 1);
#ifndef NANO_TINY
			newsc->toggle = 0;
#endif
		} else
			newsc = strtosc(funcptr);

		if (newsc == NULL) {
			jot_error(N_("Unknown function: %s"), funcptr);
			goto free_things;
		}
	}

	/* Wipe the given shortcut from the given menu. */
	for (keystruct *s = sclist; s != NULL; s = s->next)
		if ((s->menus & menu) && s->keycode == keycode)
			s->menus &= ~menu;

	/* When unbinding, we are done now. */
	if (!dobind)
		goto free_things;

	/* Limit the given menu to those where the function exists;
	 * first handle five special cases, then the general case. */
	if (is_universal(newsc->func))
		menu &= MMOST|MBROWSER;
#ifndef NANO_TINY
	else if (newsc->func == do_toggle && newsc->toggle == NO_HELP)
		menu &= (MMOST|MBROWSER|MYESNO) & ~MFINDINHELP;
	else if (newsc->func == do_toggle)
		menu &= MMAIN;
#endif
	else if (newsc->func == full_refresh)
		menu &= MMOST|MBROWSER|MHELP|MYESNO;
	else if (newsc->func == (functionptrtype)implant)
		menu &= MMOST|MBROWSER|MHELP;
	else {
		/* Tally up the menus where the function exists. */
		for (funcstruct *f = allfuncs; f != NULL; f = f->next)
			if (f->func == newsc->func)
				mask |= f->menus;

		menu &= mask;
	}

	if (!menu) {
		if (!ISSET(RESTRICTED) && !ISSET(VIEW_MODE))
			jot_error(N_("Function '%s' does not exist in menu '%s'"),
								funcptr, menuptr);
		goto free_things;
	}

	newsc->menus = menu;
	newsc->keystr = keycopy;
	newsc->keycode = keycode;

	/* Disallow rebinding <Esc> (^[). */
	if (newsc->keycode == ESC_CODE) {
		jot_error(N_("Keystroke %s may not be rebound"), keycopy);
  free_things:
		free(keycopy);
		free(newsc);
		return;
	}

#ifndef NANO_TINY
	/* If this is a toggle, find and copy its sequence number. */
	if (newsc->func == do_toggle) {
		for (keystruct *s = sclist; s != NULL; s = s->next)
			if (s->func == do_toggle && s->toggle == newsc->toggle)
				newsc->ordinal = s->ordinal;
	} else
		newsc->ordinal = 0;
#endif
	/* Add the new shortcut at the start of the list. */
	newsc->next = sclist;
	sclist = newsc;
}

/* Verify that the given file exists, is not a folder nor a device. */
bool is_good_file(char *file)
{
	struct stat rcinfo;

	/* First check that the file exists and is readable. */
	if (access(file, R_OK) != 0)
		return FALSE;

	/* If the thing exists, it may be neither a directory nor a device. */
	if (stat(file, &rcinfo) != -1 && (S_ISDIR(rcinfo.st_mode) ||
				S_ISCHR(rcinfo.st_mode) || S_ISBLK(rcinfo.st_mode))) {
		jot_error(S_ISDIR(rcinfo.st_mode) ? N_("\"%s\" is a directory") :
										N_("\"%s\" is a device file"), file);
		return FALSE;
	} else
		return TRUE;
}

#ifdef ENABLE_COLOR
/* Partially parse the syntaxes in the given file, or (when syntax
 * is not NULL) fully parse one specific syntax from the file. */
void parse_one_include(char *file, syntaxtype *syntax)
{
	char *was_nanorc = nanorc;
	size_t was_lineno = lineno;
	augmentstruct *extra;
	FILE *rcstream;

	/* Don't open directories, character files, or block files. */
	if (access(file, R_OK) == 0 && !is_good_file(file))
		return;

	rcstream = fopen(file, "rb");

	if (rcstream == NULL) {
		jot_error(N_("Error reading %s: %s"), file, strerror(errno));
		return;
	}

	/* Use the name and line number position of the included syntax file
	 * while parsing it, so we can know where any errors in it are. */
	nanorc = file;
	lineno = 0;

	/* If this is the first pass, parse only the prologue. */
	if (syntax == NULL) {
		parse_rcfile(rcstream, TRUE, TRUE);
		nanorc = was_nanorc;
		lineno = was_lineno;
		return;
	}

	live_syntax = syntax;
	lastcolor = NULL;

	/* Fully parse the given syntax (as it is about to be used). */
	parse_rcfile(rcstream, TRUE, FALSE);

	extra = syntax->augmentations;

	/* Apply any stored extendsyntax commands. */
	while (extra != NULL) {
		char *keyword = extra->data;
		char *therest = parse_next_word(extra->data);

		nanorc = extra->filename;
		lineno = extra->lineno;

		if (!parse_syntax_commands(keyword, therest))
			jot_error(N_("Command \"%s\" not understood"), keyword);

		extra = extra->next;
	}

	free(syntax->filename);
	syntax->filename = NULL;

	nanorc = was_nanorc;
	lineno = was_lineno;
}

/* Expand globs in the passed name, and parse the resultant files. */
void parse_includes(char *ptr)
{
	char *pattern, *expanded;
	glob_t files;
	int result;

	check_for_nonempty_syntax();

	pattern = ptr;
	if (*pattern == '"')
		pattern++;
	ptr = parse_argument(ptr);

	if (strlen(pattern) > PATH_MAX) {
		jot_error(N_("Path is too long"));
		return;
	}

	/* Expand a tilde first, then try to match the globbing pattern. */
	expanded = real_dir_from_tilde(pattern);
	result = glob(expanded, GLOB_ERR|GLOB_NOCHECK, NULL, &files);

	/* If there are matches, process each of them.  Otherwise, only
	 * report an error if it's something other than zero matches. */
	if (result == 0) {
		for (size_t i = 0; i < files.gl_pathc; ++i)
			parse_one_include(files.gl_pathv[i], NULL);
	} else if (result != GLOB_NOMATCH)
		jot_error(N_("Error expanding %s: %s"), pattern, strerror(errno));

	globfree(&files);
	free(expanded);
}

/* Return the index of the color that is closest to the given RGB levels,
 * assuming that the terminal uses the 6x6x6 color cube of xterm-256color.
 * When red == green == blue, return an index in the xterm gray scale. */
short closest_index_color(short red, short green, short blue)
{
	/* Translation table, from 16 intended color levels to 6 available levels. */
	static const short level[] = { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5 };

	/* Translation table, from 14 intended gray levels to 24 available levels. */
	static const short gray[] = { 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 15, 18, 21, 23 };

	if (COLORS != 256)
		return THE_DEFAULT;
	else if (red == green && green == blue && 0 < red && red < 0xF)
		return 232 + gray[red - 1];
	else
		return (36 * level[red] + 6 * level[green] + level[blue] + 16);
}

#define COLORCOUNT  34

const char hues[COLORCOUNT][8] = { "red", "green", "blue",
								   "yellow", "cyan", "magenta",
								   "white", "black", "normal",
								   "pink", "purple", "mauve",
								   "lagoon", "mint", "lime",
								   "peach", "orange", "latte",
								   "rosy", "beet", "plum",
								   "sea", "sky", "slate",
								   "teal", "sage", "brown",
								   "ocher", "sand", "tawny",
								   "brick", "crimson",
								   "grey", "gray" };

short indices[COLORCOUNT] = { COLOR_RED, COLOR_GREEN, COLOR_BLUE,
							  COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA,
							  COLOR_WHITE, COLOR_BLACK, THE_DEFAULT,
							  204, 163, 134, 38, 48, 148, 215, 208, 137,
							  175, 127, 98, 32, 111, 66, 35, 107, 100,
							  142, 186, 136, 166, 161,
							  COLOR_BLACK + 8, COLOR_BLACK + 8 };

/* Return the short value corresponding to the given color name, and set
 * vivid to TRUE for a lighter color, and thick for a heavier typeface. */
short color_to_short(const char *colorname, bool *vivid, bool *thick)
{
	if (strncmp(colorname, "bright", 6) == 0 && colorname[6] != '\0') {
		/* Prefix "bright" is deprecated; remove in 2027. */
		*vivid = TRUE;
		*thick = TRUE;
		colorname += 6;
	} else if (strncmp(colorname, "light", 5) == 0 && colorname[5] != '\0') {
		*vivid = TRUE;
		*thick = FALSE;
		colorname += 5;
	} else {
		*vivid = FALSE;
		*thick = FALSE;
	}

	if (colorname[0] == '#' && strlen(colorname) == 4) {
		unsigned short r, g, b;

		if (*vivid) {
			jot_error(N_("Color '%s' takes no prefix"), colorname);
			return BAD_COLOR;
		}

		if (sscanf(colorname, "#%1hX%1hX%1hX", &r, &g, &b) == 3)
			return closest_index_color(r, g, b);
	}

	for (int index = 0; index < COLORCOUNT; index++)
		if (strcmp(colorname, hues[index]) == 0) {
			if (index > 7 && *vivid) {
				jot_error(N_("Color '%s' takes no prefix"), colorname);
				return BAD_COLOR;
			} else if (index > 8 && COLORS < 255)
				return THE_DEFAULT;
			else
				return indices[index];
		}

	jot_error(N_("Color \"%s\" not understood"), colorname);
	return BAD_COLOR;
}

/* Parse the color name (or pair of color names) in the given string.
 * Return FALSE when any color name is invalid; otherwise return TRUE. */
bool parse_combination(char *combotext, short *fg, short *bg, int *attributes)
{
	bool vivid, thick;
	char *comma;

	*attributes = A_NORMAL;

	if (strncmp(combotext, "bold", 4) == 0) {
		*attributes |= A_BOLD;
		if (combotext[4] != ',') {
			jot_error(N_("An attribute requires a subsequent comma"));
			return FALSE;
		}
		combotext += 5;
	}

	if (strncmp(combotext, "italic", 6) == 0) {
#ifdef A_ITALIC
		*attributes |= A_ITALIC;
#endif
		if (combotext[6] != ',') {
			jot_error(N_("An attribute requires a subsequent comma"));
			return FALSE;
		}
		combotext += 7;
	}

	comma = strchr(combotext, ',');

	if (comma)
		*comma = '\0';

	if (!comma || comma > combotext) {
		*fg = color_to_short(combotext, &vivid, &thick);
		if (*fg == BAD_COLOR)
			return FALSE;
		if (vivid && !thick && COLORS > 8)
			*fg += 8;
		else if (vivid)
			*attributes |= A_BOLD;
	} else
		*fg = THE_DEFAULT;

	if (comma) {
		*bg = color_to_short(comma + 1, &vivid, &thick);
		if (*bg == BAD_COLOR)
			return FALSE;
		if (vivid && COLORS > 8)
			*bg += 8;
	} else
		*bg = THE_DEFAULT;

	return TRUE;
}

/* Parse the color specification that starts at ptr, and then the one or more
 * regexes that follow it.  For each valid regex (or start=/end= regex pair),
 * add a rule to the current syntax. */
void parse_rule(char *ptr, int rex_flags)
{
	char *names, *regexstring;
	short fg, bg;
	int attributes;

	if (*ptr == '\0') {
		jot_error(N_("Missing color name"));
		return;
	}

	names = ptr;
	ptr = parse_next_word(ptr);

	if (!parse_combination(names, &fg, &bg, &attributes))
		return;

	if (*ptr == '\0') {
		jot_error(N_("Missing regex string after '%s' command"), "color");
		return;
	}

	while (*ptr != '\0') {
		regex_t *start_rgx = NULL, *end_rgx = NULL;
			/* Intermediate storage for compiled regular expressions. */
		colortype *newcolor = NULL;
			/* Container for compiled regex (pair) and the color it paints. */
		bool expectend = FALSE;
			/* Whether it is a start=/end= regex pair. */

		if (strncmp(ptr, "start=", 6) == 0) {
			ptr += 6;
			expectend = TRUE;
		}

		regexstring = ++ptr;
		ptr = parse_next_regex(ptr);

		/* When there is no regex, or it is invalid, skip this line. */
		if (ptr == NULL || !compile(regexstring, rex_flags, &start_rgx))
			return;

		if (expectend) {
			if (strncmp(ptr, "end=", 4) != 0) {
				jot_error(N_("\"start=\" requires a corresponding \"end=\""));
				regfree(start_rgx);
				free(start_rgx);
				return;
			}

			regexstring = ptr + 5;
			ptr = parse_next_regex(ptr + 5);

			/* When there is no valid end= regex, abandon the rule. */
			if (ptr == NULL || !compile(regexstring, rex_flags, &end_rgx)) {
				regfree(start_rgx);
				free(start_rgx);
				return;
			}
		}

		/* Allocate a rule, fill in the data, and link it into the list. */
		newcolor = nmalloc(sizeof(colortype));

		newcolor->start = start_rgx;
		newcolor->end = end_rgx;

		newcolor->fg = fg;
		newcolor->bg = bg;
		newcolor->attributes = attributes;

		if (lastcolor == NULL)
			live_syntax->color = newcolor;
		else
			lastcolor->next = newcolor;

		newcolor->next = NULL;
		lastcolor = newcolor;

		/* For a multiline rule, give it a number and increase the count. */
		if (expectend) {
			newcolor->id = live_syntax->multiscore;
			live_syntax->multiscore++;
		}
	}
}

/* Set the colors for the given interface element to the given combination. */
void set_interface_color(int element, char *combotext)
{
	colortype *trio = nmalloc(sizeof(colortype));

	if (parse_combination(combotext, &trio->fg, &trio->bg, &trio->attributes)) {
		free(color_combo[element]);
		color_combo[element] = trio;
	} else
		free(trio);
}

/* Read regex strings enclosed in double quotes from the line pointed at
 * by ptr, and store them quoteless in the passed storage place. */
void grab_and_store(const char *kind, char *ptr, regexlisttype **storage)
{
	regexlisttype *lastthing, *newthing;
	const char *regexstring;

	if (!opensyntax) {
		jot_error(N_("A '%s' command requires a preceding 'syntax' command"), kind);
		return;
	}

	/* The default syntax doesn't take any file matching stuff. */
	if (strcmp(live_syntax->name, "default") == 0 && *ptr != '\0') {
		jot_error(N_("The \"default\" syntax does not accept '%s' regexes"), kind);
		return;
	}

	if (*ptr == '\0') {
		jot_error(N_("Missing regex string after '%s' command"), kind);
		return;
	}

	lastthing = *storage;

	/* If there was an earlier command, go to the last of those regexes. */
	while (lastthing != NULL && lastthing->next != NULL)
		lastthing = lastthing->next;

	/* Now gather any valid regexes and add them to the linked list. */
	while (*ptr != '\0') {
		regex_t *packed_rgx = NULL;

		regexstring = ++ptr;
		ptr = parse_next_regex(ptr);

		if (ptr == NULL)
			return;

		/* If the regex string is malformed, skip it. */
		if (!compile(regexstring, NANO_REG_EXTENDED | REG_NOSUB, &packed_rgx))
			continue;

		/* Copy the regex into a struct, and hook this in at the end. */
		newthing = nmalloc(sizeof(regexlisttype));
		newthing->one_rgx = packed_rgx;
		newthing->next = NULL;

		if (lastthing == NULL)
			*storage = newthing;
		else
			lastthing->next = newthing;

		lastthing = newthing;
	}
}

/* Gather and store the string after a comment/linter/formatter/tabgives command. */
void pick_up_name(const char *kind, char *ptr, char **storage)
{
	if (*ptr == '\0') {
		jot_error(N_("Missing argument after '%s'"), kind);
		return;
	}

	/* If the argument starts with a quote, find the terminating quote. */
	if (*ptr == '"') {
		char *look = ptr + strlen(ptr);

		while (*look != '"') {
			if (--look == ptr) {
				jot_error(N_("Argument of '%s' lacks closing \""), kind);
				return;
			}
		}

		*look = '\0';
		ptr++;
	}

	*storage = mallocstrcpy(*storage, ptr);
}

/* Handle the six syntax-only commands. */
bool parse_syntax_commands(char *keyword, char *ptr)
{
	if (strcmp(keyword, "color") == 0)
		parse_rule(ptr, NANO_REG_EXTENDED);
	else if (strcmp(keyword, "icolor") == 0)
		parse_rule(ptr, NANO_REG_EXTENDED | REG_ICASE);
	else if (strcmp(keyword, "comment") == 0) {
#ifdef ENABLE_COMMENT
		pick_up_name("comment", ptr, &live_syntax->comment);
#endif
	} else if (strcmp(keyword, "tabgives") == 0) {
		pick_up_name("tabgives", ptr, &live_syntax->tabstring);
	} else if (strcmp(keyword, "linter") == 0) {
		pick_up_name("linter", ptr, &live_syntax->linter);
		strip_leading_blanks_from(live_syntax->linter);
	} else if (strcmp(keyword, "formatter") == 0) {
		pick_up_name("formatter", ptr, &live_syntax->formatter);
		strip_leading_blanks_from(live_syntax->formatter);
	} else
		return FALSE;

	return TRUE;
}
#endif /* ENABLE_COLOR */

/* Verify that the user has not unmapped every shortcut for
 * a function that we consider 'vital' (such as "Exit"). */
static void check_vitals_mapped(void)
{
#define VITALS  4
	void (*vitals[VITALS])(void) = { do_exit, do_exit, do_exit, do_cancel };
	int inmenus[VITALS] = { MMAIN, MBROWSER, MHELP, MYESNO };

	for (int v = 0; v < VITALS; v++) {
		for (funcstruct *f = allfuncs; f != NULL; f = f->next) {
			if (f->func == vitals[v] && (f->menus & inmenus[v])) {
				if (first_sc_for(inmenus[v], f->func) == NULL) {
					jot_error(N_("No key is bound to function '%s' in menu '%s'. "
								" Exiting.\n"), f->tag, menu_to_name(inmenus[v]));
					die(_("If needed, use nano with the -I option "
								"to adjust your nanorc settings.\n"));
				} else
					break;
			}
		}
	}
}

/* Parse the rcfile, once it has been opened successfully at rcstream,
 * and close it afterwards.  If just_syntax is TRUE, allow the file to
 * contain only color syntax commands. */
void parse_rcfile(FILE *rcstream, bool just_syntax, bool intros_only)
{
	char *buffer = NULL;
	size_t size = 0;
	ssize_t length;

	while ((length = getline(&buffer, &size, rcstream)) > 0) {
		char *ptr, *keyword, *option, *argument;
#ifdef ENABLE_COLOR
		bool drop_open = FALSE;
#endif
		int set = 0;
		size_t i;

		lineno++;

#ifdef ENABLE_COLOR
		/* If doing a full parse, skip to after the 'syntax' command. */
		if (just_syntax && !intros_only && lineno <= live_syntax->lineno)
			continue;
#endif
		/* Strip the terminating newline and possibly a carriage return. */
		if (buffer[length - 1] == '\n')
			buffer[--length] = '\0';
		if (length > 0 && buffer[length - 1] == '\r')
			buffer[--length] = '\0';

		ptr = buffer;
		while (isblank((unsigned char)*ptr))
			ptr++;

		/* If the line is empty or a comment, skip to next line. */
		if (*ptr == '\0' || *ptr == '#')
			continue;

		/* Otherwise, skip to the next space. */
		keyword = ptr;
		ptr = parse_next_word(ptr);

#ifdef ENABLE_COLOR
		/* Handle extending first... */
		if (!just_syntax && strcmp(keyword, "extendsyntax") == 0) {
			augmentstruct *newitem, *extra;
			char *syntaxname = ptr;
			syntaxtype *sntx;

			check_for_nonempty_syntax();

			ptr = parse_next_word(ptr);

			for (sntx = syntaxes; sntx != NULL; sntx = sntx->next)
				if (!strcmp(sntx->name, syntaxname))
					break;

			if (sntx == NULL) {
				jot_error(N_("Could not find syntax \"%s\" to extend"), syntaxname);
				continue;
			}

			keyword = ptr;
			argument = copy_of(ptr);
			ptr = parse_next_word(ptr);

			/* File-matching commands need to be processed immediately;
			 * other commands are stored for possible later processing. */
			if (strcmp(keyword, "header") == 0 || strcmp(keyword, "magic") == 0) {
				free(argument);
				live_syntax = sntx;
				opensyntax = TRUE;
				drop_open = TRUE;
			} else {
				newitem = nmalloc(sizeof(augmentstruct));;

				newitem->filename = copy_of(nanorc);
				newitem->lineno = lineno;
				newitem->data = argument;
				newitem->next = NULL;

				if (sntx->augmentations != NULL) {
					extra = sntx->augmentations;
					while (extra->next != NULL)
						extra = extra->next;
					extra->next = newitem;
				} else
					sntx->augmentations = newitem;

				continue;
			}
		}

		/* Try to parse the keyword. */
		if (strcmp(keyword, "syntax") == 0) {
			if (intros_only) {
				check_for_nonempty_syntax();
				begin_new_syntax(ptr);
			} else
				break;
		} else if (strcmp(keyword, "header") == 0) {
			if (intros_only)
				grab_and_store("header", ptr, &live_syntax->headers);
		} else if (strcmp(keyword, "magic") == 0) {
#ifdef HAVE_LIBMAGIC
			if (intros_only)
				grab_and_store("magic", ptr, &live_syntax->magics);
#endif
		} else if (just_syntax && (strcmp(keyword, "set") == 0 ||
								strcmp(keyword, "unset") == 0 ||
								strcmp(keyword, "bind") == 0 ||
								strcmp(keyword, "unbind") == 0 ||
								strcmp(keyword, "include") == 0 ||
								strcmp(keyword, "extendsyntax") == 0)) {
			if (intros_only)
				jot_error(N_("Command \"%s\" not allowed in included file"),
									keyword);
			else
				break;
		} else if (intros_only && (strcmp(keyword, "color") == 0 ||
								strcmp(keyword, "icolor") == 0 ||
								strcmp(keyword, "comment") == 0 ||
								strcmp(keyword, "tabgives") == 0 ||
								strcmp(keyword, "linter") == 0 ||
								strcmp(keyword, "formatter") == 0)) {
			if (!opensyntax)
				jot_error(N_("A '%s' command requires a preceding "
									"'syntax' command"), keyword);
			if (strstr("icolor", keyword))
				seen_color_command = TRUE;
			continue;
		} else if (parse_syntax_commands(keyword, ptr))
			;
		else if (strcmp(keyword, "include") == 0)
			parse_includes(ptr);
		else
#endif /* ENABLE_COLOR */
		if (strcmp(keyword, "set") == 0)
			set = 1;
		else if (strcmp(keyword, "unset") == 0)
			set = -1;
		else if (strcmp(keyword, "bind") == 0)
			parse_binding(ptr, TRUE);
		else if (strcmp(keyword, "unbind") == 0)
			parse_binding(ptr, FALSE);
		else if (intros_only)
			jot_error(N_("Command \"%s\" not understood"), keyword);

#ifdef ENABLE_COLOR
		if (drop_open)
			opensyntax = FALSE;
#endif
		if (set == 0)
			continue;

		check_for_nonempty_syntax();

		if (*ptr == '\0') {
			jot_error(N_("Missing option"));
			continue;
		}

		option = ptr;
		ptr = parse_next_word(ptr);

		/* Find the just parsed option name among the existing names. */
		for (i = 0; rcopts[i].name != NULL; i++) {
			if (strcmp(option, rcopts[i].name) == 0)
				break;
		}

		if (rcopts[i].name == NULL) {
			jot_error(N_("Unknown option: %s"), option);
			continue;
		}

		/* If the option has a flag, set it or unset it, as requested. */
		if (rcopts[i].flag) {
			if (set == 1)
				SET(rcopts[i].flag);
			else
				UNSET(rcopts[i].flag);
			continue;
		}

		/* An option that takes an argument cannot be unset. */
		if (set == -1) {
			jot_error(N_("Cannot unset option \"%s\""), option);
			continue;
		}

		if (*ptr == '\0') {
			jot_error(N_("Option \"%s\" requires an argument"), option);
			continue;
		}

		argument = ptr;
		if (*argument == '"')
			argument++;
		ptr = parse_argument(ptr);

#ifdef ENABLE_UTF8
		/* When in a UTF-8 locale, ignore arguments with invalid sequences. */
		if (using_utf8 && mbstowcs(NULL, argument, 0) == (size_t)-1) {
			jot_error(N_("Argument is not a valid multibyte string"));
			continue;
		}
#endif
#ifdef ENABLE_COLOR
		if (strcmp(option, "titlecolor") == 0)
			set_interface_color(TITLE_BAR, argument);
		else if (strcmp(option, "numbercolor") == 0)
			set_interface_color(LINE_NUMBER, argument);
		else if (strcmp(option, "stripecolor") == 0)
			set_interface_color(GUIDE_STRIPE, argument);
		else if (strcmp(option, "scrollercolor") == 0)
			set_interface_color(SCROLL_BAR, argument);
		else if (strcmp(option, "selectedcolor") == 0)
			set_interface_color(SELECTED_TEXT, argument);
		else if (strcmp(option, "spotlightcolor") == 0)
			set_interface_color(SPOTLIGHTED, argument);
		else if (strcmp(option, "minicolor") == 0)
			set_interface_color(MINI_INFOBAR, argument);
		else if (strcmp(option, "promptcolor") == 0)
			set_interface_color(PROMPT_BAR, argument);
		else if (strcmp(option, "statuscolor") == 0)
			set_interface_color(STATUS_BAR, argument);
		else if (strcmp(option, "errorcolor") == 0)
			set_interface_color(ERROR_MESSAGE, argument);
		else if (strcmp(option, "keycolor") == 0)
			set_interface_color(KEY_COMBO, argument);
		else if (strcmp(option, "functioncolor") == 0)
			set_interface_color(FUNCTION_TAG, argument);
		else
#endif
#ifdef ENABLE_OPERATINGDIR
		if (strcmp(option, "operatingdir") == 0)
			operating_dir = mallocstrcpy(operating_dir, argument);
		else
#endif
#ifdef ENABLED_WRAPORJUSTIFY
		if (strcmp(option, "fill") == 0) {
			if (!parse_num(argument, &fill)) {
				jot_error(N_("Requested fill size \"%s\" is invalid"), argument);
				fill = -COLUMNS_FROM_EOL;
			}
		} else
#endif
#ifndef NANO_TINY
		if (strcmp(option, "matchbrackets") == 0) {
			if (has_blank_char(argument))
				jot_error(N_("Non-blank characters required"));
			else if (mbstrlen(argument) % 2 != 0)
				jot_error(N_("Even number of characters required"));
			else
				matchbrackets = mallocstrcpy(matchbrackets, argument);
		} else if (strcmp(option, "whitespace") == 0) {
			if (mbstrlen(argument) != 2 || breadth(argument) != 2)
				jot_error(N_("Two single-column characters required"));
			else {
				whitespace = mallocstrcpy(whitespace, argument);
				whitelen[0] = char_length(whitespace);
				whitelen[1] = char_length(whitespace + whitelen[0]);
			}
		} else
#endif
#ifdef ENABLE_JUSTIFY
		if (strcmp(option, "punct") == 0) {
			if (has_blank_char(argument))
				jot_error(N_("Non-blank characters required"));
			else
				punct = mallocstrcpy(punct, argument);
		} else if (strcmp(option, "brackets") == 0) {
			if (has_blank_char(argument))
				jot_error(N_("Non-blank characters required"));
			else
				brackets = mallocstrcpy(brackets, argument);
		} else if (strcmp(option, "quotestr") == 0)
			quotestr = mallocstrcpy(quotestr, argument);
		else
#endif
#ifdef ENABLE_SPELLER
		if (strcmp(option, "speller") == 0)
			alt_speller = mallocstrcpy(alt_speller, argument);
		else
#endif
#ifndef NANO_TINY
		if (strcmp(option, "backupdir") == 0)
			backup_dir = mallocstrcpy(backup_dir, argument);
		else if (strcmp(option, "wordchars") == 0)
			word_chars = mallocstrcpy(word_chars, argument);
		else if (strcmp(option, "guidestripe") == 0) {
			if (!parse_num(argument, &stripe_column) || stripe_column <= 0) {
				jot_error(N_("Guide column \"%s\" is invalid"), argument);
				stripe_column = 0;
			}
		} else if (strcmp(option, "tabsize") == 0) {
			if (!parse_num(argument, &tabsize) || tabsize <= 0) {
				jot_error(N_("Requested tab size \"%s\" is invalid"), argument);
				tabsize = -1;
			}
		}
#else
		;  /* Properly terminate any earlier 'else'. */
#endif
	}

	if (intros_only)
		check_for_nonempty_syntax();

	fclose(rcstream);
	free(buffer);
	lineno = 0;

	return;
}

/* Read and interpret one of the two nanorc files. */
void parse_one_nanorc(void)
{
	FILE *rcstream = fopen(nanorc, "rb");

	/* If opening the file succeeded, parse it.  Otherwise, only
	 * complain if the file actually exists. */
	if (rcstream != NULL)
		parse_rcfile(rcstream, FALSE, TRUE);
	else if (errno != ENOENT)
		jot_error(N_("Error reading %s: %s"), nanorc, strerror(errno));
}

/* Return TRUE when path-plus-name denotes a readable, normal file. */
bool have_nanorc(const char *path, const char *name)
{
	if (path == NULL)
		return FALSE;

	free(nanorc);
	nanorc = concatenate(path, name);

	return is_good_file(nanorc);
}

/* Process the nanorc file that was specified on the command line (if any),
 * and otherwise the system-wide rcfile followed by the user's rcfile. */
void do_rcfiles(void)
{
	if (custom_nanorc) {
		nanorc = get_full_path(custom_nanorc);
		if (nanorc == NULL || access(nanorc, F_OK) != 0)
			die(_("Specified rcfile does not exist\n"));
	} else {
		/* First process the system-wide nanorc, if it exists and is suitable. */
		const char* allusers = getenv("ALLUSERSPROFILE");
		if(!allusers)
			allusers = "C:/Windows";
		nanorc = free_and_assign(nanorc, measured_copy(allusers, strlen(allusers) + 8));
		strcat(nanorc, "\\nanorc");
	}

	if (is_good_file(nanorc))
		parse_one_nanorc();

	if (custom_nanorc == NULL) {
		const char *xdgconfdir = getenv("XDG_CONFIG_HOME");

		get_homedir();

		/* Now try to find a nanorc file in the user's home directory or in the
		 * XDG configuration directories, and process the first one found. */
		if (have_nanorc(homedir, "/" HOME_RC_NAME) ||
					have_nanorc(xdgconfdir, "/nano/" RCFILE_NAME) ||
					have_nanorc(homedir, "/.config/nano/" RCFILE_NAME))
			parse_one_nanorc();
		else if (homedir == NULL && xdgconfdir == NULL)
			jot_error(N_("I can't find my home directory!  Wah!"));
	}

	check_vitals_mapped();

	free(nanorc);
	nanorc = NULL;
}

#endif /* ENABLE_NANORC */
