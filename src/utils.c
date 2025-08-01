/**************************************************************************
 *   utils.c  --  This file is part of GNU nano.                          *
 *                                                                        *
 *   Copyright (C) 1999-2011, 2013-2025 Free Software Foundation, Inc.    *
 *   Copyright (C) 2016, 2017, 2019 Benno Schulenberg                     *
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

#include <errno.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <string.h>
#include <unistd.h>
#include <shlobj.h>

/* Return the user's home directory.  We use $HOME, and if that fails,
 * we fall back on the home directory of the effective user ID. */
void get_homedir(void)
{
	if (homedir == NULL) {
		const char *homenv = getenv("USERPROFILE");

		/* When HOME isn't set, or when we're root, get the home directory
		 * from the password file instead. */
		if (homenv == NULL || IsUserAnAdmin())
			homenv = getenv("ALLUSERSPROFILE");

		/* Only set homedir if some home directory could be determined,
		 * otherwise keep homedir NULL. */
		if (homenv != NULL && *homenv != '\0')
			homedir = copy_of(homenv);
	}
}

/* Return the filename part of the given path. */
const char *tail(const char *path)
{
	const char *slash = strrchr(path, '/');

	if (slash == NULL)
		return path;
	else
		return slash + 1;
}

/* Return a copy of the two given strings, welded together. */
char *concatenate(const char *path, const char *name)
{
	size_t pathlen = strlen(path);
	char *joined = nmalloc(pathlen + strlen(name) + 1);

	strcpy(joined, path);
	strcpy(joined + pathlen, name);

	return joined;
}

/* Return the number of digits that the given integer n takes up. */
int digits(ssize_t n)
{
	if (n < 100000) {
		if (n < 1000) {
			if (n < 100)
				return 2;
			else
				return 3;
		} else {
			if (n < 10000)
				return 4;
			else
				return 5;
		}
	} else {
		if (n < 10000000) {
			if (n < 1000000)
				return 6;
			else
				return 7;
		} else {
			if (n < 100000000)
				return 8;
			else
				return 9;
		}
	}
}

/* Read an integer from the given string.  If it parses okay,
 * store it in *result and return TRUE; otherwise, return FALSE. */
bool parse_num(const char *string, ssize_t *result)
{
	ssize_t value;
	char *excess;

	/* Clear the error number so that we can check it afterward. */
	errno = 0;

	value = (ssize_t)strtol(string, &excess, 10);

	if (errno == ERANGE || *string == '\0' || *excess != '\0')
		return FALSE;

	*result = value;

	return TRUE;
}

/* Read one number (or two numbers separated by comma, period, or colon)
 * from the given string and store the number(s) in *line (and *column).
 * Return FALSE on a failed parsing, and TRUE otherwise. */
bool parse_line_column(const char *string, ssize_t *line, ssize_t *column)
{
	const char *comma;
	char *firstpart;
	bool retval;

	while (*string == ' ')
		string++;

	comma = strpbrk(string, ",.:");

	if (comma == NULL)
		return parse_num(string, line);

	retval = parse_num(comma + 1, column);

	if (comma == string)
		return retval;

	firstpart = copy_of(string);
	firstpart[comma - string] = '\0';

	retval = parse_num(firstpart, line) && retval;

	free(firstpart);

	return retval;
}

/* In the given string, recode each embedded NUL as a newline. */
void recode_NUL_to_LF(char *string, size_t length)
{
	while (length > 0) {
		if (*string == '\0')
			*string = '\n';
		length--;
		string++;
	}
}

/* In the given string, recode each embedded newline as a NUL,
 * and return the number of bytes in the string. */
size_t recode_LF_to_NUL(char *string)
{
	char *beginning = string;

	while (*string != '\0') {
		if (*string == '\n')
			*string = '\0';
		string++;
	}

	return (string - beginning);
}

#if !defined(ENABLE_TINY) || defined(ENABLE_TABCOMP) || defined(ENABLE_BROWSER)
/* Free the memory of the given array, which should contain len elements. */
void free_chararray(char **array, size_t len)
{
	if (array == NULL)
		return;

	while (len > 0)
		free(array[--len]);

	free(array);
}
#endif

#ifdef ENABLE_SPELLER
/* Is the word starting at the given position in 'text' and of the given
 * length a separate word?  That is: is it not part of a longer word? */
bool is_separate_word(size_t position, size_t length, const char *text)
{
	const char *before = text + step_left(text, position);
	const char *after = text + position + length;

	/* If the word starts at the beginning of the line OR the character before
	 * the word isn't a letter, and if the word ends at the end of the line OR
	 * the character after the word isn't a letter, we have a whole word. */
	return ((position == 0 || !is_alpha_char(before)) &&
					(*after == '\0' || !is_alpha_char(after)));
}
#endif /* ENABLE_SPELLER */

/* Return the position of the needle in the haystack, or NULL if not found.
 * When searching backwards, we will find the last match that starts no later
 * than the given start; otherwise, we find the first match starting no earlier
 * than start.  If we are doing a regexp search, and we find a match, we fill
 * in the global variable regmatches with at most 9 subexpression matches. */
const char *strstrwrapper(const char *haystack, const char *needle,
		const char *start)
{
	if (ISSET(USE_REGEXP)) {
		if (ISSET(BACKWARDS_SEARCH)) {
			size_t last_find, ceiling, far_end;
			size_t floor = 0, next_rung = 0;
				/* The start of the search range, and the next start. */

			if (regexec(&search_regexp, haystack, 1, regmatches, 0) != 0)
				return NULL;

			far_end = strlen(haystack);
			ceiling = start - haystack;
			last_find = regmatches[0].rm_so;

			/* A result beyond the search range also means: no match. */
			if (last_find > ceiling)
				return NULL;

			/* Move the start-of-range forward until there is no more match;
			 * then the last match found is the first match backwards. */
			while (regmatches[0].rm_so <= ceiling) {
				floor = next_rung;
				last_find = regmatches[0].rm_so;
				/* If this is the last possible match, don't try to advance. */
				if (last_find == ceiling)
					break;
				next_rung = step_right(haystack, last_find);
				regmatches[0].rm_so = next_rung;
				regmatches[0].rm_eo = far_end;
				if (regexec(&search_regexp, haystack, 1, regmatches,
										REG_STARTEND) != 0)
					break;
			}

			/* Find the last match again, to get possible submatches. */
			regmatches[0].rm_so = floor;
			regmatches[0].rm_eo = far_end;
			if (regexec(&search_regexp, haystack, 10, regmatches,
										REG_STARTEND) != 0)
				return NULL;

			return haystack + regmatches[0].rm_so;
		}

		/* Do a forward regex search from the starting point. */
		regmatches[0].rm_so = start - haystack;
		regmatches[0].rm_eo = strlen(haystack);
		if (regexec(&search_regexp, haystack, 10, regmatches,
										REG_STARTEND) != 0)
			return NULL;
		else
			return haystack + regmatches[0].rm_so;
	}

	if (ISSET(CASE_SENSITIVE)) {
		if (ISSET(BACKWARDS_SEARCH))
			return revstrstr(haystack, needle, start);
		else
			return strstr(start, needle);
	}

	if (ISSET(BACKWARDS_SEARCH))
		return mbrevstrcasestr(haystack, needle, start);
	else
		return mbstrcasestr(start, needle);
}

/* Allocate the given amount of memory and return a pointer to it. */
void *nmalloc(size_t howmuch)
{
	void *section = malloc(howmuch);

	if (section == NULL)
		die(_("Nano is out of memory!\n"));

	return section;
}

/* Reallocate the given section of memory to have the given size. */
void *nrealloc(void *section, size_t howmuch)
{
	section = realloc(section, howmuch);

	if (section == NULL)
		die(_("Nano is out of memory!\n"));

	return section;
}

/* Return an appropriately reallocated dest string holding a copy of src.
 * Usage: "dest = mallocstrcpy(dest, src);". */
char *mallocstrcpy(char *dest, const char *src)
{
	size_t count = strlen(src) + 1;

	dest = nrealloc(dest, count);
	strncpy(dest, src, count);

	return dest;
}

/* Return an allocated copy of the first count characters
 * of the given string, and NUL-terminate the copy. */
char *measured_copy(const char *string, size_t count)
{
	char *thecopy = nmalloc(count + 1);

	memcpy(thecopy, string, count);
	thecopy[count] = '\0';

	return thecopy;
}

/* Return an allocated copy of the given string. */
char *copy_of(const char *string)
{
	return measured_copy(string, strlen(string));
}

/* Free the string at dest and return the string at src. */
char *free_and_assign(char *dest, char *src)
{
	free(dest);
	return src;
}

/* When not softwrapping, nano scrolls the current line horizontally by
 * chunks ("pages").  Return the column number of the first character
 * displayed in the edit window when the cursor is at the given column. */
size_t get_page_start(size_t column)
{
	if (column == 0 || column + 2 < editwincols || ISSET(SOFTWRAP))
		return 0;
	else if (editwincols > 8)
		return column - 6 - (column - 6) % (editwincols - 8);
	else
		return column - (editwincols - 2);
}

/* Return the placewewant associated with current_x, i.e. the zero-based
 * column position of the cursor. */
size_t xplustabs(void)
{
	return wideness(openfile->current->data, openfile->current_x);
}

/* Return the index in text of the character that (when displayed) will
 * not overshoot the given column. */
size_t actual_x(const char *text, size_t column)
{
	const char *start = text;
		/* From where we start walking through the text. */
	size_t width = 0;
		/* The current accumulated span, in columns. */

	while (*text != '\0') {
		int charlen = advance_over(text, &width);

		if (width > column)
			break;

		text += charlen;
	}

	return (text - start);
}

/* A strnlen() with tabs and multicolumn characters factored in:
 * how many columns wide are the first maxlen bytes of text? */
size_t wideness(const char *text, size_t maxlen)
{
	size_t width = 0;

	if (maxlen == 0)
		return 0;

	while (*text != '\0') {
		size_t charlen = advance_over(text, &width);

		if (maxlen <= charlen)
			break;

		maxlen -= charlen;
		text += charlen;
	}

	return width;
}

/* Return the number of columns that the given text occupies. */
size_t breadth(const char *text)
{
	size_t span = 0;

	while (*text != '\0')
		text += advance_over(text, &span);

	return span;
}

/* Append a new magic line to the end of the buffer. */
void new_magicline(void)
{
	openfile->filebot->next = make_new_node(openfile->filebot);
	openfile->filebot->next->data = copy_of("");
	openfile->filebot = openfile->filebot->next;
	openfile->totsize++;
}

#if !defined(NANO_TINY) || defined(ENABLE_HELP)
/* Remove the magic line from the end of the buffer, if there is one and
 * it isn't the only line in the file. */
void remove_magicline(void)
{
	if (openfile->filebot->data[0] == '\0' &&
				openfile->filebot != openfile->filetop) {
		if (openfile->current == openfile->filebot)
			openfile->current = openfile->current->prev;
		openfile->filebot = openfile->filebot->prev;
		delete_node(openfile->filebot->next);
		openfile->filebot->next = NULL;
		openfile->totsize--;
	}
}
#endif

#ifndef NANO_TINY
/* Return TRUE when the mark is before or at the cursor, and FALSE otherwise. */
bool mark_is_before_cursor(void)
{
	return (openfile->mark->lineno < openfile->current->lineno ||
						(openfile->mark == openfile->current &&
						openfile->mark_x <= openfile->current_x));
}

/* Return in (top, top_x) and (bot, bot_x) the start and end "coordinates"
 * of the marked region. */
void get_region(linestruct **top, size_t *top_x, linestruct **bot, size_t *bot_x)
{
	if (mark_is_before_cursor()) {
		*top = openfile->mark;
		*top_x = openfile->mark_x;
		*bot = openfile->current;
		*bot_x = openfile->current_x;
	} else {
		*bot = openfile->mark;
		*bot_x = openfile->mark_x;
		*top = openfile->current;
		*top_x = openfile->current_x;
	}
}

/* Get the set of lines to work on -- either just the current line, or the
 * first to last lines of the marked region.  When the cursor (or mark) is
 * at the start of the last line of the region, exclude that line. */
void get_range(linestruct **top, linestruct **bot)
{
	if (!openfile->mark) {
		*top = openfile->current;
		*bot = openfile->current;
	} else {
		size_t top_x, bot_x;

		get_region(top, &top_x, bot, &bot_x);

		if (bot_x == 0 && *bot != *top && !also_the_last)
			*bot = (*bot)->prev;
		else
			also_the_last = TRUE;
	}
}
#endif /* !NANO_TINY */

#if !defined(NANO_TINY) || defined(ENABLE_SPELLER) || defined (ENABLE_LINTER) || defined (ENABLE_FORMATTER)
/* Return a pointer to the line that has the given line number. */
linestruct *line_from_number(ssize_t number)
{
	linestruct *line = openfile->current;

	if (line->lineno > number)
		while (line->lineno != number)
			line = line->prev;
	else
		while (line->lineno != number)
			line = line->next;

	return line;
}
#endif

/* Count the number of characters from begin to end, and return it. */
size_t number_of_characters_in(const linestruct *begin, const linestruct *end)
{
	const linestruct *line;
	size_t count = 0;

	/* Sum the number of characters (plus a newline) in each line. */
	for (line = begin; line != end->next; line = line->next)
		count += mbstrlen(line->data) + 1;

	/* Do not count the final newline. */
	return (count - 1);
}
