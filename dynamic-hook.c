static char *version = "@(!--#) @(#) dynamic-hook.c, sversion 0.1.0, fversion 008, 03-august-2023";

/*
 *  dynamic-hook.c
 *
 *  dynamic TFTP read code
 *
 */

#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define MAX_TFTP_FILENAME_LENGTH 1024
#define PADDING 10
#define MAX_LINE_LENGTH 8192

#define COMMENT   "$rem"
#define DEFINE    "$def"
#define INCLUDE   "$inc"
#define VAR_START "${"
#define VAR_END   "}"

/*********************************************************************/

int dynamic_str_find(haystack, offset, needle)
	char	*haystack;
	int	offset;
	char	*needle;
{
	int	lh;
	int	ln;
	int	i;
	int	j;
	int	found;

	lh = strlen(haystack);
	ln = strlen(needle);

	if (ln > (lh - offset)) {
		return -1;
	}

#ifdef DYNAMIC_DEBUG
	printf("H %s %d\n", haystack, lh);
	printf("N %s %d\n", needle, ln);
	printf("O %d\n", offset);
#endif

	for (i = offset; i <= (lh - ln); i++) {
		found = TRUE;

		for (j = 0; j < ln; j++) {
#ifdef DYNAMIC_DEBUG
			printf("i=%d j=%d %c %c\n", i, j, haystack[i], needle[j]);
#endif
			if (haystack[i+j] != needle[j]) {
				found = FALSE;
				break;
			}
		}

#ifdef DYNAMIC_DEBUG
		printf("i=%d found=%d\n", i, found);
#endif

		if (found) {
			return i;
		}
	}

	return -1;
}

/*********************************************************************/

void dynamic_str_copyslice(dest, src, doffset, start, end)
	char	*dest;
	char	*src;
	int	doffset;
	int	start;
	int	end;
{
	int	i;

	for (i = start; i <= end; i++) {
		dest[doffset] = src[i];
		doffset++;
	}

	return;
}

/*********************************************************************/

void dynamic_str_substitute(line)
	char	*line;
{
	char	linecopy[MAX_LINE_LENGTH];
	int	doffset;
	int	soffset;
	int	i;
	int	pos;
	char	*envname;

#ifdef DYNAMIC_DEBUG
	printf("Line=[%s]\n", line);
	printf("VAR_START=[%s]\n", VAR_START);
#endif

	for (i = 0; i < MAX_LINE_LENGTH; i++) {
		linecopy[i] = '\0';
	}

	doffset = 0;
	soffset = 0;

	while (TRUE) {
		pos = dynamic_str_find(line, soffset, VAR_START);

#ifdef DYNAMIC_DEBUG
		printf("doffset=%d soffset=%d pos=%d\n", doffset, soffset, pos);
#endif

		if (pos == -1) {
			dynamic_str_copyslice(linecopy, line, doffset, soffset, strlen(line) - 1);
			doffset = doffset + strlen(line);
			break;
		}

		dynamic_str_copyslice(linecopy, line, doffset, soffset, pos - 1);
		doffset = doffset + (pos - soffset);

		soffset = pos + strlen(VAR_START);

		pos = dynamic_str_find(line, soffset, VAR_END);

		if (pos == -1) {
			continue;
		}

		line[pos] = '\0';

		envname = getenv(line + soffset);

		if (envname == NULL) {
			envname = "<null>";
		}

		dynamic_str_copyslice(linecopy, envname, doffset, 0, strlen(envname) - 1);
		doffset = doffset + strlen(envname);

		line[pos] = '@';

		soffset = pos + strlen(VAR_END);
	}

	linecopy[doffset] = '\0';

#ifdef DYNAMIC_DEBUG
	printf("Origline=[%s]\n", line);
	printf("Linecopy=[%s]\n", linecopy);
#endif

	strncpy(line, linecopy, MAX_LINE_LENGTH - sizeof(char));

#ifdef DYNAMIC_DEBUG
	printf("Copy completed");
#endif

	return;
}
	
/*********************************************************************/

void dynamic_create_dynfilename(filename, dynfilename)
	char	*filename;
	char	*dynfilename;
{
	int	i;

	i = 0;

	while (filename[i] != '\0') {
		dynfilename[i] = filename[i];
		i++;
	}
	dynfilename[i++] = '.';
	dynfilename[i++] = 'd';
	dynfilename[i++] = 'y';
	dynfilename[i++] = 'n';
	dynfilename[i++] = '\0';

	return;
}

/*********************************************************************/

void dynamic_swap_char(s, c1, c2)
	char	*s;
	char	c1;
	char	c2;
{
	while (*s != '\0') {
		if (*s == c1) {
			*s = c2;
		}

		s++;
	}

	return;
}

/*********************************************************************/

char *dynamic_skip_whitespace(s)
	char	*s;
{
	while (*s != '\0') {
		if (*s == ' ') {
			s++;
			continue;
		} else {
			break;
		}
	}

	return s;
}

/*********************************************************************/

char *dynamic_second_word(s)
	char	*s;
{
	/* skip any white space at the start */
	while ((*s != '\0') && (*s == ' ')) {
		s++;
	}

	/* skip non-white space (i.e. the first word) */
	while ((*s != '\0') && (*s != ' ')) {
		s++;
	}

	/* skip any more whitespace separating the first and second word in the line */
	while ((*s != '\0') && (*s == ' ')) {
		s++;
	}

	return s;
}

/*********************************************************************/

void dynamic_define(line)
	char	*line;
{
	int	lenline;
	char	envname[MAX_LINE_LENGTH];
	int	pos;
	int	i;

#ifdef DYNAMIC_DEBUG
	printf("line=[%s]\n", line);
#endif

	lenline = strlen(line);

#ifdef DYNAMIC_DEBUG
	printf("lenline=%d\n", lenline);
#endif

	if (lenline > 0) {
		if (line[lenline - 1] == '\n') {
			line[lenline - 1] = '\0';
			lenline--;
		}
	}

#ifdef DYNAMIC_DEBUG
	printf("line=[%s]\n", line);
	printf("lenline=%d\n", lenline);
#endif

#ifdef DYNAMIC_DEBUG
	printf("lost newline\n");
#endif

	dynamic_swap_char(line, '\t', ' ');

#ifdef DYNAMIC_DEBUG
	printf("swapped tabs\n");
#endif

	pos = dynamic_str_find(line, 0, " ");

#ifdef DYNAMIC_DEBUG
	printf("pos1=%d\n", pos);
#endif

	if (pos == -1) {
		/* error no white space - quietly ignore */
		return;
	}

	line = line + pos;

	line = dynamic_skip_whitespace(line);

	i = 0;

	while ((*line != '\0') && (*line != ' ') && (i < (MAX_LINE_LENGTH - 1))) {
		envname[i] = *line;
		i++;
		line++;
	}

	envname[i] = '\0';

	line = dynamic_skip_whitespace(line);

	dynamic_str_substitute(line);

	if (strlen(envname) > 0) {
#ifdef DYNAMIC_DEBUG
		printf("%s=%s\n", envname, line);
#endif
		setenv(envname, line, 1);
	}

	return;
}

/*********************************************************************/

void dynamic_include(line, file)
	char	*line;
	FILE	*file;
{
	int	lenline;
	char	incfilename[MAX_LINE_LENGTH];
	FILE	*inc;

	lenline = strlen(line);

	if (lenline > 0) {
		if (line[lenline - 1] == '\n') {
			line[lenline - 1] = '\0';
			lenline--;
		}
	}

	dynamic_swap_char(line, '\t', ' ');

	line = dynamic_second_word(line);

	dynamic_str_substitute(line);

	lenline = strlen(line);

#ifdef DYNAMIC_DEBUG
	printf("Include filename is [%s]\n", line);
#endif

	if (lenline == 0) {
		/* no file name specified after include directive - return now */
		return;
	}

	if ((inc = fopen(line, "r")) == NULL) {
		/* unable to open file - not necessarily an error - but return now */
		return;
	}

	while (fgets(line, MAX_LINE_LENGTH-sizeof(char), inc) != NULL) {
		if (strncmp(line, COMMENT, strlen(COMMENT)) == 0) {
			continue;
		} else if (strncmp(line, DEFINE, strlen(DEFINE)) == 0) {
			dynamic_define(line);
		} else {
			dynamic_str_substitute(line);

			fprintf(file, "%s", line);
		}
	}

	fclose(inc);

	return;
}

/*********************************************************************/

void dynamic_preproc(filename, dynfilename)
	char	*filename;
	char	*dynfilename;
{
	FILE	*file;
	FILE	*dyn;
	char	line[MAX_LINE_LENGTH];

	if ((dyn = fopen(dynfilename, "r")) == NULL) {
		syslog(LOG_INFO, "cannot open dynamic file %s for reading", dynfilename);
		return;
	}

	if ((file = fopen(filename, "w")) == NULL) {
		syslog(LOG_INFO, "cannot open file %s for writing", filename);
		fclose(dyn);
		return;
	}
	
	while (fgets(line, MAX_LINE_LENGTH-sizeof(char), dyn) != NULL) {
		if (strncmp(line, COMMENT, strlen(COMMENT)) == 0) {
			continue;
		} else if (strncmp(line, DEFINE, strlen(DEFINE)) == 0) {
			dynamic_define(line);
		} else if (strncmp(line, INCLUDE, strlen(INCLUDE)) == 0) {
			dynamic_include(line, file);
		} else {
			dynamic_str_substitute(line);

			fprintf(file, "%s", line);
		}
	}

	fflush(file);
	fflush(file);
	fflush(file);
	fclose(file);

	fclose(dyn);

	return;
}

/*********************************************************************/

void dynamic_hook(filename)
	char	*filename;
{
	char	dynfilename[MAX_TFTP_FILENAME_LENGTH];
	struct	stat statbuf;

	syslog(LOG_INFO, "preprocessing file %s", filename);

	if (strlen(filename) > (MAX_TFTP_FILENAME_LENGTH - PADDING)) {
		syslog(LOG_INFO, "file name %s is too long - no preprocessing", filename);
		return;
	}

	dynamic_create_dynfilename(filename, dynfilename);

	syslog(LOG_INFO, "looking for dynamic file file %s", dynfilename);

	if (stat(dynfilename, &statbuf) != 0) {
		syslog(LOG_INFO, "cannot stat dynamic file %s", dynfilename);
		return;
	}

	if (statbuf.st_size == 0) {
		syslog(LOG_INFO, "dynamic file %s has file size of zero", dynfilename);
		return;
	}

	if (access(dynfilename, R_OK) != 0) {
		syslog(LOG_INFO, "no read access to dynamic file %s", dynfilename);
		return;
	}

	dynamic_preproc(filename, dynfilename);

	return;
}

/* end of file */
