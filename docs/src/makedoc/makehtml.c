/*         ______   ___    ___ 
 *        /\  _  \ /\_ \  /\_ \ 
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___ 
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      Makedoc's html output routines.
 *
 *      By Shawn Hargreaves.
 *
 *      Grzegorz Adam Hankiewicz made the output valid HTML 4.0, and added
 *      support for cross references and optional CSS to color output.
 *
 *      See readme.txt for copyright information.
 *
 *      See allegro/docs/src/makedoc/format.txt for a brief description of
 *      the source of _tx files.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "makehtml.h"
#include "makemisc.h"
#include "makedoc.h"

typedef struct t_post POST;

struct t_post
{
   char filename[1024];
   char **token;
   int num;
};

int html_flags;
char charset[256]="iso-8859-1";
const char *html_extension = "html";
char *document_title;
char *html_footer;

static POST **_post;
static FILE *_file;
static char _filename[1024];
static char *_xref[256];
static int _xrefs;
static int _empty_count;

/* Internal functions */

static void _write_html_xref_list(char **xref, int *xrefs);
static void _write_html_xref(char *xref);
static void _output_html_footer(char *main_filename);
static void _output_toc(char *filename, int root, int body, int part);
static void _hfprintf(char *format, ...);
static void _hfputs(char *string);
static int _toc_scmp(const void *e1, const void *e2);
static int _output_section_heading(LINE *line, char *filename, int section_number);
static void _output_custom_markers(LINE *line);
static void _output_buffered_text(void);
static void _output_html_header(char *section);
static void _add_post_process_xref(const char *token);
static void _post_process_pending_xrefs(void);
static POST *_search_post_section(const char *filename);
static POST *_search_post_section_with_token(const char *token);
static POST *_create_post_section(const char *filename);
static void _destroy_post_page(POST *p);
static const char *_get_clean_xref_token(const char *text);
static void _post_process_filename(char *filename);
static int _verify_correct_input(void);
static void _close_html_file(FILE *file);
static void _output_sorted_nested_toc(TOC **list, unsigned int num_items);



/* write_html:
 * Entry point to the function which translates makedoc's format
 * to correct html output.
 */
int write_html(char *filename)
{
   int block_empty_lines = 0, section_number = 0;
   LINE *line = head;

   if (_verify_correct_input())
      return 1;
   
   printf("writing %s\n", filename);

   _file = fopen(filename, "w");
   if (!_file)
      return 1;



   strcpy(_filename, filename);
   _output_html_header(0);

   while (line) {
      if (line->flags & HTML_FLAG) {
	 if (line->flags & (HEADING_FLAG | NODE_FLAG | DEFINITION_FLAG))
	    _write_html_xref_list(_xref, &_xrefs);

	 if (line->flags & HEADING_FLAG) {
	    _output_buffered_text();
	    if (_output_section_heading(line, filename, section_number))
	       return 1;
	    section_number++;
	 }
	 else if (line->flags & NODE_FLAG) {
	    _output_buffered_text();
	    /* output a node marker, which will be followed by a chunk of text inside a chapter */
	    fprintf(_file, "<br><center><h2><a name=\"%s\">%s</a></h2></center><p>\n",
	       line->text, line->text);
	    _add_post_process_xref(line->text);
	 }
	 else if (line->flags & DEFINITION_FLAG) {
	    if (_empty_count && !block_empty_lines) _empty_count++;
	    _output_buffered_text();
	    /* output a function definition */
	    _add_post_process_xref(line->text);
	    _hfprintf("<b>%s</b>", line->text);
	    if (!(line->flags & CONTINUE_FLAG))
	       fputs("<br>", _file);
	    fputs("\n", _file);
	 }
	 else if ((!(line->flags & NO_EOL_FLAG)) &&
		  (is_empty(strip_html(line->text))) &&
		  (!strstr(line->text, "<li>"))) {
	    _hfprintf("%s\n", line->text);
	    if (!block_empty_lines)
	       _empty_count++;
	 }
	 else if (strstr(line->text, "<email>") || strstr(line->text, "<link>")) {
	    _output_buffered_text();
	    _output_custom_markers(line);
	 }
	 else {
	    _output_buffered_text();
	    /* output a normal line */
	    _hfputs(line->text);
	    fputs("\n", _file);
	 }

	 /* mutex to control extra line output in preformated sections */
	 if (strstr(line->text, "<pre>") || strstr(line->text, "<ul>")) {
	    block_empty_lines++;
	    _empty_count = 0;
	 }
	 else if (strstr(line->text, "</pre>") || strstr(line->text, "</ul>")) {
	    block_empty_lines--;
	 }
      }
      else if (line->flags & TOC_FLAG) {
	 _write_html_xref_list(_xref, &_xrefs);
	 if (flags & MULTIFILE_FLAG) {
	    _output_buffered_text();
	    _output_toc(filename, 1, 0, -1);
	 }
	 else if (line->flags & SHORT_TOC_FLAG) {
	    _output_buffered_text();
	    _output_toc(filename, 0, 1, -1);
	 }
	 else {
	    _output_buffered_text();
	    _output_toc(filename, 1, 1, -1);
	 }
      }
      else if (line->flags & XREF_FLAG)
	 _xref[_xrefs++] = line->text; /* buffer cross reference */

      line = line->next;
   }

   if ((flags & MULTIFILE_FLAG) && (section_number > 1))
      _output_html_footer(filename);

   _close_html_file(_file);
   
   _post_process_pending_xrefs();
   
   return 0;
}



/* _output_custom_markers:
 * Converts the <email> and <link> custom markers to proper html code.
 */
static void _output_custom_markers(LINE *line)
{
   char *s2, *s3;
   /* special munging for links */
   char *s = line->text;

   do {
      s2 = strstr(s, "<email>");
      s3 = strstr(s, "<link>");

      if ((s2) || (s3)) {
	 int i;
	 char buf[256];
	 
	 if ((s3) && ((!s2) || (s3 < s2)))
	    s2 = s3;

	 i = 0;
	 while (s < s2)
	    buf[i++] = *(s++);

	 buf[i] = 0;
	 _hfputs(buf);

	 while (*s != '>')
	    s++;

	 s++;
	 i = 0;

	 while ((*s) && (*s != '<'))
	    buf[i++] = *(s++);

	 buf[i] = 0;

	 if (s2[1] == 'e')
	    _hfprintf("<a href=\"mailto:%s\">%s", buf, buf);
	 else
	    _hfprintf("<a href=\"%s\">%s", buf, buf);
      }
   } while (s2);

   _hfputs(s);
   fputs("\n", _file);
}



/* _write_html_xref_list:
 * Recieves an array of pointers and a pointer to the variable holding
 * the number of buffered text lines. Returns if there are none waiting
 * to be printed, otherwise sorts the list and writes a block of cross
 * references.
 */
static void _write_html_xref_list(char **xref, int *xrefs)
{
   int i;

   if (!(*xrefs))
      return ;

   qsort(xref, *xrefs, sizeof(char *), scmp);
   
   fputs("\n<blockquote", _file);
   if (!(html_flags & HTML_IGNORE_CSS))
      fputs(" class=\"xref\"", _file);
   fputs("><em>See also:</em>\n", _file);
   for (i=0; i<(*xrefs); i++) {
      if (i) _hfprintf(",\n");
      _write_html_xref(xref[i]);
   }
   *xrefs = 0;
   _hfprintf(".</blockquote>\n");
   _empty_count = 0;
}



/* _write_html_xref:
 * Writes a single cross reference splitting xref if needed
 */
static void _write_html_xref(char *xref)
{
   char *tok, first = 1;

   tok = strtok(xref, ",;");

   while (tok) {
      while ((*tok) && (myisspace(*tok)))
	 tok++;
      if (!first) _hfprintf(",\n");
      first = 0;
      _hfprintf("<a ");
      if (!(html_flags & HTML_IGNORE_CSS))
	 _hfprintf("class=\"xref\" ");
      
      if (flags & MULTIFILE_FLAG)
	 _hfprintf("href=\"post_process#%s\">%s</a>", tok, tok);
      else
	 _hfprintf("href=\"#%s\">%s</a>", tok, tok);
	 
      tok = strtok(NULL, ",;");
   }
}



/* _output_html_footer:
 * Writes that "Back to contets" message at the end of a file with a link.
 */
static void _output_html_footer(char *main_filename)
{
   fprintf(_file, "<hr><a href=\"%s\">%s</a>\n",
      get_filename(main_filename), html_footer);
}



/* _output_toc:
 */
static void _output_toc(char *filename, int root, int body, int part)
{
   char name[256];
   char *s;
   TOC *toc;
   int nested = 0;
   
   #define ALT_TEXT(toc)   ((toc->alt) ? toc->alt : toc->text)

   if (root) {
      int section_number = 0;
      toc = tochead;
      if (toc)
	 toc = toc->next;

      fprintf(_file, "<ul>\n");

      while (toc) {
	 if ((toc->root) && (toc->htmlable)) {
	    if (toc->otherfile) {
	       sprintf(name, "%s.%s", toc->text, html_extension);
	       mystrlwr(name);
	       _hfprintf("<li><a href=\"%s\">%s</a>\n", name, ALT_TEXT(toc));
	    }
	    else if (body) {
	       _hfprintf("<li><a href=\"#%s\">%s</a>\n", toc->text, ALT_TEXT(toc));
	       section_number++;
	    }
	    else {
	       strcpy(name, filename);
	       s = extension(name)-1;
	       if ((int)s - (int)get_filename(name) > 5)
		  s = get_filename(name)+5;
	       sprintf(s, "%03d.%s", section_number, html_extension);
	       _hfprintf("<li><a href=\"%s\">%s</a>\n", get_filename(name), ALT_TEXT(toc));
	       section_number++;
	    }
	 }
	 toc = toc->next;
      }

      fprintf(_file, "</ul>\n");
   }

   if (body) {
      toc = tochead;
      if (toc)
	 toc = toc->next;

      if (part <= 0) {
	 TOC *ptr[TOC_SIZE];
	 int i = 0;
	 if (root)
	    fprintf(_file, "<ul>\n");
	 else
	    fprintf(_file, "<ul>\n");

	 while (toc) {
	    if ((toc->htmlable) && (!toc->otherfile)) {
	       if (!toc->root) {
		  if (!nested) {
		     fprintf(_file, "<ul>\n");
		     nested = 1;
		  }
	       }
	       else {
		  if (nested) {
		     _output_sorted_nested_toc(ptr, i);
		     nested = i = 0;
		  }
	       }

	       if (nested) {
		  if (i < TOC_SIZE)
		     ptr[i++] = toc;
	       }
	       else
		  _hfprintf("<li><a href=\"#%s\">%s</a>\n", toc->text, ALT_TEXT(toc));
	    }

	    toc = toc->next;
	 }

	 if (nested)
	    _output_sorted_nested_toc(ptr, i);

	 if (root)
	    fprintf(_file, "</ul>\n");
	 else
	    fprintf(_file, "</ul>\n");
      }
      else {
	 TOC *ptr[TOC_SIZE];
	 int j, i = 0;
	 int section_number = 0;
	 fprintf(_file, "\n<ul>\n");

	 while ((toc) && (section_number < part)) {
	    if ((toc->root) && (!toc->otherfile))
	       section_number++;
	    toc = toc->next;
	 }

	 while ((toc) && (!toc->root) && (i < TOC_SIZE)) {
	    if (toc->htmlable)
	       ptr[i++] = toc;
	    toc = toc->next;
	 }

	 if (i > 1)
	    qsort(ptr, i, sizeof(TOC *), _toc_scmp);

	 for (j = 0; j < i; j++)
	    _hfprintf("<li><a href=\"#%s\">%s</a>\n", ptr[j]->text, ALT_TEXT(ptr[j]));

	 fprintf(_file, "</ul>\n");
      }
   }
}



/* _hfprintf:
 */
static void _hfprintf(char *format, ...)
{
   char buf[1024];

   va_list ap;
   va_start(ap, format);
   vsprintf(buf, format, ap);
   va_end(ap);

   _hfputs(buf);
}



/* _hfputs:
 */
static void _hfputs(char *string)
{
   while (*string) {
      if ((string[0] == '&') &&
	  ((string[1] == 'l') || (string[1] == 'g')) &&
	  (string[2] == 't')) {

	 fputc('&', _file);
	 fputc(string[1], _file);
	 fputc('t', _file);
	 fputc(';', _file);

	 string += 3;
      }
      else if ((string[0] == '&') && (string[1] != 'l') &&
	  (string[1] != 'g')) {

	 fputs("&amp;", _file);
	 string++;
      }
      else {
	 fputc(*string, _file);
	 string++;
      }
   }
}



/* _toc_scmp:
 * qsort helper which compares two TOC nodes alphabeticaly.
 */
static int _toc_scmp(const void *e1, const void *e2)
{
   TOC *t1 = *((TOC **)e1);
   TOC *t2 = *((TOC **)e2);

   return mystricmp(t1->text, t2->text);
}



/* _output_section_heading:
 * Used to mark a new section of the document. In multifile output closes
 * the actual file and writes a new one.
 */
static int _output_section_heading(LINE *line, char *filename, int section_number)
{
   char buf[256], *s;
   
   /* output a section heading, switching file as required */
   if ((flags & MULTIFILE_FLAG) && (section_number > 0)) {
      if (section_number > 1)
	 _output_html_footer(filename);

      _close_html_file(_file);
      strcpy(buf, filename);
      s = extension(buf)-1;
      if ((int)s - (int)get_filename(buf) > 5)
	 s = get_filename(buf)+5;
      sprintf(s, "%03d.%s", section_number-1, html_extension);
      printf("writing %s\n", buf);
      _file = fopen(buf, "w");
      if (!_file)
	 return 1;
      strcpy(_filename, buf);
      _output_html_header(line->text);
   }
   _hfprintf("<h1>%s</h1>\n", line->text);
   if ((flags & MULTIFILE_FLAG) && 
       (!(line->flags & NOCONTENT_FLAG)) &&
       (section_number > 0)) {
      _output_toc(filename, 0, 1, section_number);
   }
   return 0;
}



/* _output_html_header:
 * The header of an HTML document is a little bit complex, so it deserves
 * it's own function. In the header you have the title of the document,
 * the character set definition and the style sheet specification. This
 * function just outputs to _file the correct tags, it presumes _file
 * has just been opened. The title is specified in the source document
 * with a @document_title= line, and section can be another text which will
 * be appended to the title.
 */
static void _output_html_header(char *section)
{
   fputs("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\" \"http://www.w3.org/TR/REC-html40/loose.dtd\">\n", _file);
   fputs("<html><head><title>\n", _file);
   fputs(document_title, _file);
   if (section && strlen(strip_html(section))) {
      fputs(": ", _file);
      fputs(strip_html(section), _file);
   }
   fputs("\n</title>\n", _file);
   fputs("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=", _file);
   fputs(charset, _file);
   fputs("\">\n", _file);

   /* optional style sheet output */
   if (!(html_flags & HTML_IGNORE_CSS)) {
      fputs("<meta http-equiv=\"Content-Style-Type\" content=\"text/css\">\n", _file);
      fputs("<style type=\"text/css\">\n", _file);
      fputs("<!--\n", _file);
      fputs("A.xref:link    {color: blue; text-decoration: none; background: rgb(255, 204, 50);}\n", _file);
      fputs("A.xref:visited {color: blue; text-decoration: none; background: rgb(255, 204, 50);}\n", _file);
      fputs("A.xref:hover   {color: blue; text-decoration: underline; background: rgb(255, 224, 150);}\n", _file);
      fputs("A.xref:active  {color: red; text-decoration: none; background: rgb(255, 204, 50);}\n", _file);
      fputs("blockquote.xref {border: medium solid rgb(255, 204, 51); color: black; background: rgb(255, 204, 50);}\n", _file);
      fputs("blockquote.code {border: medium solid rgb(255, 204, 50); color: black; background: rgb(255, 255, 155);}\n", _file);
      fputs("blockquote.text {border: medium solid rgb(175, 235, 255); color: black; background: rgb(210, 244, 255);}\n", _file);
      fputs("-->\n", _file);
      fputs("</style>\n", _file);
   }

   /* header end and body start */
   fputs("</head><body bgcolor=\"#ffffff\" text=\"#000000\" link=\"#0000ee\" alink=\"#ff0000\" vlink=\"#551a8b\">\n", _file);
}



/* _output_buffered_text:
 * Outputs buffered text, which might be a set of cross references
 * followed by a number of empty lines, which will be transformed
 * to apropiate HTML tags.
 */
static void _output_buffered_text(void)
{
   if (_empty_count) {
      if (_empty_count > 1)
	 fputs("<p><br>\n", _file);
      else if (html_flags & HTML_BR_AS_P)
	 fputs("<p>\n", _file);
      else
	 fputs("<br>\n", _file);

      _empty_count = 0;
   }
}



/* _post_process_pending_xrefs:
 * Reopens all the created files and scans for post process tags, which
 * will be replaced with correct filenames according to the post process
 * info in memory.
 */
static void _post_process_pending_xrefs(void)
{
   int f;
   if (!_post) return ;

   for(f = 0; _post[f]; f++)
      _post_process_filename(_post[f]->filename);
   
   for(f = 0; _post[f]; f++)
      _destroy_post_page(_post[f]);
      
   free(_post);
}



/* _add_post_process_xref:
 * Adds a token to a buffer with the current filename for a post process.
 */
static void _add_post_process_xref(const char *token)
{
   POST *p;

   token = _get_clean_xref_token(token);
   if (!token[0])
      return ;

   p = _search_post_section(_filename);
   if (!p)
      p = _create_post_section(_filename);

   p->token = m_xrealloc(p->token, sizeof(char*) * (1 + p->num));
   p->token[p->num++] = m_strdup(token);
}



/* _get_clean_xref_token:
 * Given a text, extracts the clean xref token, which either has to be
 * surrounded by #..", inside a <a name=".." tag, or without any
 * html characters around. Otherwise returns an empty string.
 */
static const char *_get_clean_xref_token(const char *text)
{
   static char buf[256], *t;
   const char *p;

   if ((p = strstr(text, "<a name=\""))) {
      strcpy(buf, p + 9);
      t = strchr(buf, '"');
      *t = 0;
   }
   else if ((p = strchr(text, '#'))) {
      strcpy(buf, p+1);
      t = strchr(buf, '"');
      *t = 0;
   }
   else if (!strchr(text, '<') && !strchr(text, '>'))
      strcpy(buf, text);
   else { /* this is mainly for debugging */
      printf("'%s' was rejected as clean xref token\n", text);
      buf[0] = 0;
   }
   return buf;
}



/* _search_post_section:
 * Searches the globals for an existant POST page which matches filename.
 * Returns the POST page or NULL if none was found.
 */
static POST *_search_post_section(const char *filename)
{
   int f;
   
   if (!_post)
      return 0;

   for(f = 0; _post[f]; f++)
      if (!strcmp(_post[f]->filename, filename))
	 return _post[f];
	 
   return 0;
}



/* _search_post_section_with_token:
 * Similar to _search_post_section, but instead of matching filenames,
 * returns the page containing the specified token, NULL if none was found.
 */
static POST *_search_post_section_with_token(const char *token)
{
   int f, g;
   if (!_post)
      return 0;

   for(f = 0; _post[f]; f++)
      for(g = 0; g < _post[f]->num; g++)
	 if (!strcmp(_post[f]->token[g], token))
	    return _post[f];
   return 0;
}



/* _create_post_section:
 * Adds one POST section matching filename to the global array of pages
 * after creating a new POST page, which will be returned.
 */
static POST *_create_post_section(const char *filename)
{
   int num;
   POST *p;

   p = m_xmalloc(sizeof(POST));
   p->token = m_xmalloc(sizeof(char*));
   p->num = 0;
   strcpy(p->filename, filename);

   if (!_post) {
      _post = m_xmalloc(sizeof(POST*));
      _post[0] = 0;
   }

   for(num = 0; _post[num]; num++)
      ;

   _post = m_xrealloc(_post, sizeof(POST*) * (2 + num));
   _post[num++] = p;
   _post[num] = 0;

   return p;
}



/* _destroy_post_page:
 * Frees the memory used by a post page.
 */
static void _destroy_post_page(POST *p)
{
   int f;
   for(f = 0; f < p->num; f++)
      free(p->token[f]);

   free(p->token);
   free(p);
}



/* _post_process_filename:
 * Opens the filename and replaces the post process tags.
 */
static void _post_process_filename(char *filename)
{
   char *new_filename, *p;
   char *line;
   FILE *f1 = 0, *f2 = 0;

   new_filename = m_strdup(filename);
   if (!new_filename || strlen(new_filename) < 2) goto cancel;
   new_filename[strlen(filename)-1] = 'x';

   f1 = fopen(filename, "rt");
   f2 = fopen(new_filename, "wt");
   if (!f1 || !f2) goto cancel;

   while ((line = m_fgets(f1))) {
      if ((p = strstr(line, "\"post_process#"))) {
	 POST *page = _search_post_section_with_token(_get_clean_xref_token(p + 2));
	 if (!page) {
	    printf("Didn't find xref for %s", line);
	    fputs(line, f2);
	 }
	 else {
	    char *temp = m_xmalloc(1 + strlen(line) + 13 +
			 strlen(get_filename(page->filename)));
	    strncpy(temp, line, p - line + 1);

	    if (!strcmp(filename, page->filename))
	       strcpy(&temp[p - line + 1], p + 13);
	    else {
	       strcpy(&temp[p - line + 1], get_filename(page->filename));
	       strcat(temp, p + 13);
	    }
	    fputs(temp, f2);
	    free(temp);
	 }
      }
      else
	 fputs(line, f2);
	 
      free(line);
   }

   fclose(f1);
   fclose(f2);
   f1 = f2 = 0;

   if (remove(filename)) goto cancel;
   rename(new_filename, filename);

   cancel:
   if (f1) fclose(f1);
   if (f2) fclose(f2);
   if (new_filename) {
      remove(new_filename);
      free(new_filename);
   }
}



/* _verify_correct_input:
 * Goes through some bits marked during _tx read and verifies that the
 * input file is ok, returning non-zero with bad files.
 */
static int _verify_correct_input(void)
{
   int ret = 0;
   
   if (html_flags & HTML_OLD_H_TAG_FLAG) {
      printf("The tag '@h=<html><head><title>#</title></head><body>' is obsolete.\n");
      printf("Please use '@document_title blah blah blah' instead.\n");
      printf("And make sure you aren't using any <head>,<title> or <body> tags!.\n");
      ret++;
   }

   if (!(html_flags & HTML_DOCUMENT_TITLE_FLAG)) {
      printf("Missing tag '@document_title=blah blah blah'.\n");
      ret++;
   }

   if (html_flags & HTML_OLD_F_TAG_FLAG) {
      printf("The tags '@f=blah', '@f1=blah' and '@f2=blah' are obsolete.\n");
      printf("Please use '@html_footer=Back to contents' or something similar instead.\n");
      printf("And make sure you aren't using any <head>,<title> or <body> tags!.\n");
      ret++;
   }

   if (!(html_flags & HTML_FOOTER_FLAG) && (flags & MULTIFILE_FLAG)) {
      printf("For multifile documents please use '@html_footer=Back to contents'.\n");
      ret++;
   }

   return ret;
}



/* _close_html_file:
 * Closes the stream after writting the apropiate html close tags.
 */
static void _close_html_file(FILE *file)
{
   fputs("\n</body>\n</html>\n", file);
   fclose(file);
}



/* _output_sorted_nested_toc:
 * Outputs the buffered TOC list, according to num_items. After the
 * list a </ul> is appended to the text.
 */
static void _output_sorted_nested_toc(TOC **list, unsigned int num_items)
{
   unsigned int f;
   
   if (num_items > 1)
      qsort(list, num_items, sizeof(TOC *), _toc_scmp);
   for (f = 0; f < num_items; f++)
      _hfprintf("<li><a href=\"#%s\">%s</a>\n", list[f]->text, ALT_TEXT(list[f]));
   
   fprintf(_file, "</ul>\n");
}
