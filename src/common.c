/* common.c - common functions used by daisy-player and eBook-speaker.
 *
 * Copyright (C)2018 J. Lemmens
 *                                                                    
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "daisy.h"

char sink_info[10][100];

void open_xml_file (misc_t *misc, my_attribute_t *my_attribute,
                    daisy_t *daisy, char *xml_file, char *anchor)
{
   if (access (xml_file, R_OK))
   {
      endwin ();
      printf ("%s\n",
              gettext ("Please try to play this book with daisy-player"));
      beep ();
      quit_daisy_player (misc, my_attribute, daisy);
      _exit (EXIT_SUCCESS);
   } // if
   if (! (misc->doc = htmlParseFile (xml_file, "UTF-8")))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, "open_xml_file (%s)", xml_file);
      failure (misc, str, e);
   } // if
   if ((misc->reader = xmlReaderWalker (misc->doc)) == NULL)
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, "open_xml_file (%s)", xml_file);
      failure (misc, str, e);
   } // if
   if (! *anchor)
      return;

// skip to anchor
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         break;
      if (strcasecmp (anchor, my_attribute->id) == 0)
         break;
   } // while
} // open_xml_file

char *convert_URL_name (misc_t *misc, char *file)
{
   int i, j;

   *misc->str = j = 0;
   for (i = 0; i < (int) strlen (file) - 2; i++)
   {
      if (file[i] == '%' && isxdigit (file[i + 1]) && isxdigit (file[i + 2]))
      {
         char hex[10];

         hex[0] = '0';
         hex[1] = 'x';
         hex[2] = file[i + 1];
         hex[3] = file[i + 2];
         hex[4] = 0;
         sscanf (hex, "%x", (unsigned int *) &misc->str[j++]);
         i += 2;
      }
      else
         misc->str[j++] = file[i];
   } // for
   misc->str[j++] = file[i++];
   misc->str[j++] = file[i];
   misc->str[j] = 0;
   return misc->str;
} // convert_URL_name

void failure (misc_t *misc, char *str, int e)
{
   endwin ();
   beep ();
   printf ("\n\n%s: %s\n", str, strerror (e));
   fflush (stdout);
   remove_tmp_dir (misc);
   switch (system ("reset"));
   _exit (EXIT_FAILURE);
} // failure

void player_ended ()
{
   pid_t kidpid;
   int status;

   while ((kidpid = waitpid (-1, &status, WNOHANG)) > 0);
} // player_ended

void get_realpath_name (char *dir, char *search_str, char *found)
{
   int total, n;
   struct dirent **namelist;

   total = scandir ((const char *) dir, &namelist, NULL, alphasort);
   if (total == -1)
   {
      printf ("\nscandir: %s: %s\n", dir, strerror (errno));
      exit (EXIT_FAILURE);
   } // if
   for (n = 0; n < total; n++)
   {
      char *path, *basec;

      path = malloc (strlen (dir) + strlen (namelist[n]->d_name) + 2);
      if (dir[strlen (dir) - 1] == '/')
         sprintf (path, "%s%s", dir, namelist[n]->d_name);
      else
         sprintf (path, "%s/%s", dir, namelist[n]->d_name);
      basec = strdup (search_str);
      if (strcasecmp (namelist[n]->d_name, basename (basec)) == 0)
      {
         strcpy (found, path);
         free (path);
         free (namelist);
         free (basec);
         return;
      } // if
      free (path);
      free (basec);
      if (strcmp (namelist[n]->d_name, ".") == 0 ||
          strcmp (namelist[n]->d_name, "..") == 0)
         continue;
      if (namelist[n]->d_type == DT_DIR)
      {
         char *new_dir;

         new_dir = malloc (strlen (dir) + strlen (namelist[n]->d_name) + 2);
         if (dir[strlen (dir) - 1] != '/')
            sprintf (new_dir, "%s/%s", dir, namelist[n]->d_name);
         else
            sprintf (new_dir, "%s%s", dir, namelist[n]->d_name);
         get_realpath_name (new_dir, search_str, found);
         free (new_dir);
      } // if
   } // for
   free (namelist);
} // get_realpath_name

char *get_dir_content (misc_t *misc, char *dir_name, char *search_str)
{
   char *found;
   DIR *dir;
   struct dirent *entry;

   if (strcasestr (dir_name, search_str) && *search_str)
      return dir_name;
   if (! (dir = opendir (dir_name)))
      failure (misc, dir_name, errno);
   if (! (entry = readdir (dir)))
      failure (misc, "readdir ()", errno);
   do
   {
      if (strcmp (entry->d_name, ".") == 0 ||
          strcmp (entry->d_name, "..") == 0)
         continue;
      if (strcasestr (entry->d_name, search_str) && *search_str)
      {
         found = malloc (strlen (dir_name) + strlen (entry->d_name) + 5);
         sprintf (found, "%s/%s\n", dir_name, entry->d_name);
         found[strlen (found) - 1] = 0;
         closedir (dir);
         return found;
      } // if
      if (entry->d_type == DT_DIR)
      {
         char *path;

         if (strcmp (entry->d_name, ".") == 0 ||
             strcmp (entry->d_name, "..") == 0)
            continue;
         path = malloc (strlen (dir_name) + strlen (entry->d_name) + 10);
         sprintf (path, "%s/%s", dir_name, entry->d_name);
         found = malloc (MAX_STR);
         strcpy (found, get_dir_content (misc, path, search_str));
         if (strcasestr (found, search_str) && *search_str)
         {
            free (path);
            return found;
         } // if
         free (path);
      } // if
   } while ((entry = readdir (dir)));
   return "";
} // get_dir_content

void find_index_names (misc_t *misc)
{
   char *dirc;

   get_realpath_name (misc->daisy_mp, "ncc.html", misc->ncc_html);
   *misc->ncx_name = 0;
   strcpy (misc->ncx_name,
            get_dir_content (misc, misc->daisy_mp, ".ncx"));
   *misc->opf_name = 0;
   strcpy (misc->opf_name,
            get_dir_content (misc, misc->daisy_mp, ".opf"));
   dirc = strdup (misc->opf_name);
   free (misc->daisy_mp);
   misc->daisy_mp = strdup (dirname (dirc));
} // find_index_names

void kill_player (misc_t *misc)
{
   while (kill (misc->player_pid, SIGHUP) == 0);
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      while (kill (misc->cdda_pid, SIGKILL) == 0);
   unlink (misc->tmp_wav);
} // kill_player

void skip_right (misc_t *misc, daisy_t *daisy)
{
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      return;
   if (misc->playing == -1)
   {
      beep ();
      return;
   } // if
   misc->current = misc->displaying = misc->playing;
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   wrefresh (misc->screenwin);
   kill_player (misc);
} // skip_right

int handle_ncc_html (misc_t *misc, my_attribute_t *my_attribute,
                     daisy_t *daisy)
{
// lookfor "ncc.html"
   htmlDocPtr doc;
   xmlTextReaderPtr ncc;
   char *dirc;

   free (misc->daisy_mp);
   misc->daisy_mp = strdup (misc->ncc_html);
   dirc = strdup (misc->daisy_mp);
   misc->daisy_mp = strdup (dirname (dirc));
   free (dirc);
   strncpy (misc->daisy_version, "2.02", 4);
   doc = htmlParseFile (misc->ncc_html, "UTF-8");
   ncc = xmlReaderWalker (doc);
   misc->total_items = 0;
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, ncc))
         break;
      if (strcasecmp (misc->tag, "h1") == 0 ||
          strcasecmp (misc->tag, "h2") == 0 ||
          strcasecmp (misc->tag, "h3") == 0 ||
          strcasecmp (misc->tag, "h4") == 0 ||
          strcasecmp (misc->tag, "h5") == 0 ||
          strcasecmp (misc->tag, "h6") == 0)
      {
         misc->total_items++;
      } // if
   } // while
   xmlTextReaderClose (ncc);
   xmlFreeDoc (doc);
   if (misc->total_items == 0)
   {
      endwin ();
      printf ("%s\n",
              gettext ("Please try to play this book with daisy-player"));
      beep ();
      quit_daisy_player (misc, my_attribute, daisy);
      _exit (EXIT_SUCCESS);
   } // if
   return misc->total_items;
} // handle_ncc_html

int namefilter (const struct dirent *namelist)
{
   int  r = 0;
   char     my_pattern[] = "*.smil";

   r = fnmatch (my_pattern, namelist->d_name, FNM_PERIOD);
   return (r == 0) ? 1 : 0;
} // namefilter

int get_meta_attributes (xmlTextReaderPtr parse, xmlTextWriterPtr writer)
{
   while (1)
   {
      char *name, *content;

      name = (char *) xmlTextReaderGetAttribute (parse,
                            (const unsigned char *) "name");
      if (! name)
         return 0;
      content = (char *) xmlTextReaderGetAttribute (parse,
                     (const unsigned char *) "content");
      if (strcasecmp (name, "title") == 0)
      {
         xmlTextWriterWriteString (writer, (const xmlChar *)content);
         return 1;
      } // if
      if (xmlTextReaderRead (parse) == 0)
         return 0;
      if (xmlTextReaderRead (parse) == 0)
         return 0;
   } // while
} // get_meta_attributes

void create_ncc_html (misc_t *misc)
{
   xmlTextWriterPtr writer;
   int total, n;
   struct dirent **namelist;

   sprintf (misc->ncc_html, "%s/ncc.html", misc->daisy_mp);
   if (! (writer = xmlNewTextWriterFilename (misc->ncc_html, 0)))
      failure (misc, misc->ncc_html, errno);
   xmlTextWriterSetIndent (writer, 1);
   xmlTextWriterSetIndentString (writer, BAD_CAST "   ");
   xmlTextWriterStartDocument (writer, NULL, NULL, NULL);
   xmlTextWriterStartElement (writer, BAD_CAST "html");
   xmlTextWriterWriteString (writer, BAD_CAST "\n");
   xmlTextWriterStartElement (writer, BAD_CAST "head");
   xmlTextWriterWriteString (writer, BAD_CAST "\n   ");
   xmlTextWriterEndElement (writer);
   xmlTextWriterStartElement (writer, BAD_CAST "body");
   xmlTextWriterWriteString (writer, (const xmlChar *)"\n");
   total = scandir (get_current_dir_name (), &namelist, namefilter,
                    alphasort);
   for (n = 0; n < total; n++)
   {
      xmlTextWriterStartElement (writer, BAD_CAST "h1");
      xmlTextWriterWriteString (writer, BAD_CAST "\n");
      xmlTextWriterStartElement (writer, BAD_CAST "a");
      xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "href",
                                         "%s", namelist[n]->d_name);

// write label
      {
      xmlTextReaderPtr parse;
      char *str, *tag;

      str = malloc (strlen (misc->daisy_mp) + strlen (namelist[n]->d_name) + 5);
      sprintf (str, "%s/%s", misc->daisy_mp, namelist[n]->d_name);
      parse = xmlReaderForFile (str, "UTF-8", 0);
      while (1)
      {
         if (xmlTextReaderRead (parse) == 0)
            break;
         tag = (char *) xmlTextReaderConstName (parse);
         if (strcasecmp (tag, "/head") == 0)
            break;
         if (strcasecmp (tag, "body") == 0)
            break;
         if (strcasecmp (tag, "meta") == 0)
         {
            if (get_meta_attributes (parse, writer))
               break;
         } // if
      } // while
      xmlTextReaderClose (parse);
      free (str);
      } // write label

      xmlTextWriterWriteString (writer, BAD_CAST "\n         ");
      xmlTextWriterEndElement (writer);
      xmlTextWriterEndElement (writer);
   } // for
   xmlTextWriterEndElement (writer);
   xmlTextWriterEndElement (writer);
   xmlTextWriterEndDocument (writer);
   xmlFreeTextWriter (writer);
   free (namelist);
} // create_ncc_html

daisy_t *create_daisy_struct (misc_t *misc,
         my_attribute_t *my_attribute, daisy_t *daisy)
{
   htmlDocPtr doc;
   xmlTextReaderPtr ptr;

   misc->total_pages = 0;
   *misc->daisy_version = 0;\
   *misc->ncc_html = *misc->ncx_name = *misc->opf_name = 0;
   find_index_names (misc);

// handle "ncc.html"
   if (*misc->ncc_html)
   {
      if (misc->verbose)
      {
         printf ("\r\n\ncount items in %s... ", misc->ncc_html);
         fflush (stdout);
      } // if
      misc->total_items = handle_ncc_html (misc, my_attribute, daisy);
      return calloc (misc->total_items + 1, sizeof (daisy_t));
   } // if ncc.html

// handle *.ncx
   if (*misc->ncx_name)
      strncpy (misc->daisy_version, "3", 2);
   if (strlen (misc->ncx_name) < 4)
      *misc->ncx_name = 0;

// handle "*.opf""
   if (*misc->opf_name)
      strncpy (misc->daisy_version, "3", 2);
   if (strlen (misc->opf_name) < 4)
      *misc->opf_name = 0;

   if (*misc->ncc_html == 0 && *misc->ncx_name == 0 && *misc->opf_name == 0)
   {
      create_ncc_html (misc);
      misc->total_items = handle_ncc_html (misc, my_attribute, daisy);
      return (daisy_t *) calloc (misc->total_items + 1, sizeof (daisy_t));
   } // if

// count items in opf
   if (misc->verbose)
   {
      printf ("\r\n\ncount items in OPF... ");
      fflush (stdout);
   } // if
   misc->items_in_opf = 0;
   doc = htmlParseFile (misc->opf_name, "UTF-8");
   ptr = xmlReaderWalker (doc);
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, ptr))
         break;
      if (strcasecmp (misc->tag, "itemref") == 0)
      {
         misc->items_in_opf++; //
      } // if
   } // while
   xmlTextReaderClose (ptr);
   xmlFreeDoc (doc);                             
   if (misc->verbose)
      printf ("%d\n", misc->items_in_opf);

// count items in ncx
   if (misc->verbose)
   {
      printf ("\rcount items in NCX... ");
      fflush (stdout);
   } // if
   misc->items_in_ncx = 0;
   doc = htmlParseFile (misc->ncx_name, "UTF-8");
   ptr = xmlReaderWalker (doc);
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, ptr))
         break;
      if (strcasecmp (misc->tag, "navpoint") == 0)
         misc->items_in_ncx++;
   } // while
   xmlTextReaderClose (ptr);
   xmlFreeDoc (doc);
   if (misc->verbose)
      printf ("%d\n", misc->items_in_ncx);

   if (misc->items_in_ncx == 0 && misc->items_in_opf == 0)
   {
      endwin ();
      printf ("%s\n",
              gettext ("Please try to play this book with daisy-player"));
      beep ();
      quit_daisy_player (misc, my_attribute, daisy);
      _exit (EXIT_SUCCESS);
   } // if

   misc->total_items = misc->items_in_ncx;
   if (misc->items_in_opf > misc->items_in_ncx)
      misc->total_items = misc->items_in_opf;
   switch (chdir (misc->daisy_mp));
   return (daisy_t *) calloc (misc->total_items + 1, sizeof (daisy_t));
} // create_daisy_struct

void make_tmp_dir (misc_t *misc)
{
   free (misc->tmp_dir);
   misc->tmp_dir = strdup ("/tmp/" PACKAGE ".XXXXXX");
   if (! mkdtemp (misc->tmp_dir))
   {
      int e;                                        

      e = errno;
      beep ();
      failure (misc, misc->tmp_dir, e);
   } // if
} // make_tmp_dir

void remove_tmp_dir (misc_t *misc)
{
   if (! misc->tmp_dir)
      return;
   if (strncmp (misc->tmp_dir, "/tmp/", 5) != 0)
      return;

// Be sure not to remove wrong files
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      rmdir (misc->tmp_dir);
      return;
   } // if

   snprintf (misc->cmd, MAX_CMD - 1, "rm -rf %s", misc->tmp_dir);
   if (system (misc->cmd) != 0)
   {
      int e;
      char *str;

      e = errno;
      str = malloc (strlen (misc->cmd) + strlen (strerror (e)) + 10);
      sprintf (str, "%s: %s\n", misc->cmd, strerror (e));
      failure (misc, str, e);
   } // if
} // remove_tmp_dir

void get_attributes (misc_t *misc, my_attribute_t *my_attribute,
                     xmlTextReaderPtr ptr)
{
   char attr[MAX_STR - 1];

   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "class"));
   if (strcmp (attr, "(null)"))
      sprintf (my_attribute->class, "%s", attr);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "clip-begin"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->clip_begin, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "clipbegin"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->clip_begin, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "clip-end"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->clip_end, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "clipend"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->clip_end, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "href"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->href, attr, MAX_STR - 1);
   if (xmlTextReaderGetAttribute (ptr, (const xmlChar *) "id") != NULL)
   {
      free (my_attribute->id);
      my_attribute->id = strdup ((char *) xmlTextReaderGetAttribute (ptr,
               (const xmlChar *) "id"));
      if (strcmp (my_attribute->id, misc->current_id) != 0)
      {
         free (misc->current_id);
         misc->current_id = strdup (my_attribute->id);
      } // if
   } // if
   if (xmlTextReaderGetAttribute (ptr, (const xmlChar *) "idref") != NULL)
   {
      free (my_attribute->idref);
      my_attribute->idref = strdup ((char *) xmlTextReaderGetAttribute (ptr,
               (const xmlChar *) "idref"));
   } // if
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "item"));
   if (strcmp (attr, "(null)"))
      misc->current = atoi (attr);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "level"));
   if (strcmp (attr, "(null)"))
      misc->level = atoi ((char *) attr);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
          xmlTextReaderGetAttribute (ptr, (const xmlChar *) "media-type"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->media_type, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "name"));
   if (strcmp (attr, "(null)"))
   {
      char name[MAX_STR], content[MAX_STR];

      *name = 0;
      strncpy (attr, (char *) xmlTextReaderGetAttribute
               (ptr, (const xmlChar *) "name"), MAX_STR - 1);
      if (strcmp (attr, "(null)"))
         strncpy (name, attr, MAX_STR - 1);
      *content = 0;
      snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "content"));
      if (strcmp (attr, "(null)"))
         strncpy (content, attr, MAX_STR - 1);
      if (strcasestr (name, "dc:format"))
         strncpy (misc->daisy_version, content, MAX_STR - 1);
      if (strcasestr (name, "dc:title") && ! *misc->daisy_title)
      {
         strncpy (misc->daisy_title, content, MAX_STR - 1);
         strncpy (misc->bookmark_title, content, MAX_STR - 1);
         if (strchr (misc->bookmark_title, '/'))
            *strchr (misc->bookmark_title, '/') = '-';
      } // if
/* don't use it!!!
      if (strcasestr (name, "dtb:misc->depth"))
         misc->depth = atoi (content);
*/
      if (strcasestr (name, "dtb:totalPageCount"))
         misc->total_pages = atoi (content);
/* don't use it!!!
      if (strcasestr (name, "ncc:misc->depth"))
         misc->depth = atoi (content);
*/
      if (strcasestr (name, "ncc:maxPageNormal"))
         misc->total_pages = atoi (content);
      if (strcasestr (name, "ncc:pageNormal"))
         misc->total_pages = atoi (content);
      if (strcasestr (name, "ncc:page-normal"))
         misc->total_pages = atoi (content);
      if (strcasestr (name, "dtb:totalTime")  ||
          strcasestr (name, "ncc:totalTime"))
      {
         strncpy (misc->ncc_totalTime, content, MAX_STR - 1);
         if (strchr (misc->ncc_totalTime, '.'))
            *strchr (misc->ncc_totalTime, '.') = 0;
      } // if
   } // if
   snprintf (attr, MAX_STR - 1, "%s", (char *)
           xmlTextReaderGetAttribute (ptr, (const xmlChar *) "playorder"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->playorder, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "seconds"));
   if (strcmp (attr, "(null)"))
   {
      misc->seconds = atoi (attr);
      if (misc->seconds < 0)
         misc->seconds = 0;
   } // if
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "smilref"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->smilref, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
           xmlTextReaderGetAttribute (ptr,
                                      (const xmlChar *) "pulseaudio_device"));
   if (strcmp (attr, "(null)"))
      strcpy (misc->pulseaudio_device, attr);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
        xmlTextReaderGetAttribute (ptr, (const xmlChar *) "ocr_language"));
   if (strcmp (attr, "(null)"))
      strncpy (misc->ocr_language, attr, 5);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "cd_dev"));
   if (strcmp (attr, "(null)"))
      strncpy (misc->cd_dev, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
           xmlTextReaderGetAttribute (ptr, (const xmlChar *) "cddb_flag"));
   if (strcmp (attr, "(null)"))
      misc->cddb_flag = (char) attr[0];
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "speed"));
   if (strcmp (attr, "(null)"))
   {
      misc->speed = atof (attr);
      if (misc->speed <= 0.1 || misc->speed > 2)
         misc->speed = 1.0;
   } // if
   if (xmlTextReaderGetAttribute (ptr, (const xmlChar *) "src") != NULL)
   {
      free (my_attribute->src);
      my_attribute->src = strdup ((char *) xmlTextReaderGetAttribute (ptr,
               (const xmlChar *) "src"));
   } // if
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "toc"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->toc, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "value"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->value, attr, MAX_STR - 1);
} // get_attributes

int get_tag_or_label (misc_t *misc, my_attribute_t *my_attribute,
                      xmlTextReaderPtr reader)
{
   int type;

   *misc->tag = 0;
   misc->label = realloc (misc->label, 1);
   *misc->label = 0;
   *my_attribute->class = 0;
   *my_attribute->clip_begin = *my_attribute->clip_end = 0;
   *my_attribute->href = *my_attribute->media_type =
   *my_attribute->playorder = *my_attribute->smilref = 0;
   *my_attribute->toc = *my_attribute->value = 0;
   free (my_attribute->id);
   my_attribute->id = strdup ("");
   free (my_attribute->idref);
   my_attribute->idref = strdup ("");
   free (my_attribute->src);
   my_attribute->src = strdup ("");

   if (! reader)
   {
      return 0;
   } // if
   switch (xmlTextReaderRead (reader))
   {
   case -1:
   {
      failure (misc, "xmlTextReaderRead ()\n"
               "Can't handle this DTB structure!\n"
               "Don't know how to handle it yet, sorry. (-:\n", errno);
      break;
   }
   case 0:
   {
      return 0;
   }
   case 1:
      break;
   } // swwitch
   type = xmlTextReaderNodeType (reader);
   switch (type)
   {
   int n, i;

   case -1:
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, gettext ("Cannot read type: %d"), type);
      failure (misc, str, e);
      break;
   }
   case XML_READER_TYPE_ELEMENT:
      strncpy (misc->tag, (char *) xmlTextReaderConstName (reader),
               MAX_TAG - 1);
      n = xmlTextReaderAttributeCount (reader);
      for (i = 0; i < n; i++)
         get_attributes (misc, my_attribute, reader);
      if (strcasecmp (misc->tag, "audio") == 0)
      {
         free (misc->prev_id);
         misc->prev_id = strdup (misc->audio_id);
         free (misc->audio_id);
         misc->audio_id = strdup (misc->current_id);
      } // if
      return 1;
   case XML_READER_TYPE_END_ELEMENT:
      snprintf (misc->tag, MAX_TAG - 1, "/%s",
                (char *) xmlTextReaderName (reader));
      return 1;
   case XML_READER_TYPE_TEXT:
   case XML_READER_TYPE_CDATA:
   case XML_READER_TYPE_SIGNIFICANT_WHITESPACE:
   {
      int x;
      size_t phrase_len;

      phrase_len = strlen ((char *) xmlTextReaderConstValue (reader));
      x = 0;
      while (1)
      {
         if (isspace (xmlTextReaderValue (reader)[x]))
            x++;
         else
            break;
      } // while
      misc->label = realloc (misc->label, phrase_len + 1);
      snprintf (misc->label, phrase_len - x + 1, "%s", (char *)
               xmlTextReaderValue (reader) + x);
      for (x = strlen (misc->label) - 1; x >= 0 && isspace (misc->label[x]);
           x--)
         misc->label[x] = 0;
      for (x = 0; misc->label[x] > 0; x++)
         if (! isprint (misc->label[x]))
            misc->label[x] = ' ';
      return 1;
   }
   case XML_READER_TYPE_ENTITY_REFERENCE:
      snprintf (misc->tag, MAX_TAG - 1, "/%s",
                (char *) xmlTextReaderName (reader));
      return 1;
   default:
      return 1;
   } // switch
   return 0;
} // get_tag_or_label

void go_to_page_number (misc_t *misc, my_attribute_t *my_attribute,
                          daisy_t *daisy)
{
   char pn[15];

   kill_player (misc);
   if (misc->cd_type != CDIO_DISC_MODE_CD_DA)
      misc->player_pid = -2;
   misc->playing = misc->just_this_item = -1;
   view_screen (misc, daisy);
   unlink ("old.wav");
   mvwprintw (misc->titlewin, 1, 0,
              "----------------------------------------");
   wprintw (misc->titlewin, "----------------------------------------");
   mvwprintw (misc->titlewin, 1, 0, "%s ", gettext ("Go to page number:"));
   echo ();
   wgetnstr (misc->titlewin, pn, 5);
   noecho ();
   view_screen (misc, daisy);
   if (atoi (pn) == 0 || atoi (pn) > misc->total_pages)
   {
      beep ();
      pause_resume (misc, my_attribute, daisy);
      pause_resume (misc, my_attribute, daisy);
      return;
   } // if

// start searching
   mvwprintw (misc->titlewin, 1, 0,
              "----------------------------------------");
   wprintw (misc->titlewin, "----------------------------------------");
   wattron (misc->titlewin, A_BOLD);
   mvwprintw (misc->titlewin, 1, 0, "%s", gettext ("Please wait..."));
   wattroff (misc->titlewin, A_BOLD);
   wrefresh (misc->titlewin);
   misc->playing = misc->current_page_number = 0;
   for (misc->current = 0; misc->current < misc->total_items; misc->current++)
   {
      if (daisy[misc->current].page_number == atoi (pn))
      {
// at the start of an item
         misc->displaying = misc->playing = misc->current;
         open_clips_file (misc, my_attribute, daisy[misc->playing].clips_file,
                          daisy[misc->playing].clips_anchor);
         misc->elapsed_seconds = time (NULL);
         return;
      } // if
   } // for

   for (misc->playing = misc->total_items - 1; misc->playing >= 0;
        misc->playing--)
   {
      if (daisy[misc->playing].page_number < atoi (pn))
         break;
   } // for

// start searching here
   open_clips_file (misc, my_attribute, daisy[misc->playing].clips_file,
                    daisy[misc->playing].clips_anchor);
   while (1)
   {
      if (misc->current_page_number == atoi (pn))
      {
// found it
         misc->displaying = misc->current = misc->playing;
         misc->player_pid = -2;
         view_screen (misc, daisy);
         return;
      } // if current_page_number == pn
      if (misc->current_page_number > atoi (pn))
      {
         misc->current = misc->playing = 0;
         misc->current_page_number = daisy[misc->playing].page_number;
         open_clips_file (misc, my_attribute, daisy[misc->playing].clips_file,
                          daisy[misc->playing].clips_anchor);
         return;
      } // if current_page_number >  pn
      get_next_clips (misc, my_attribute, daisy);
   } // while
} // go_to_page_number

void select_next_output_device (misc_t *misc, daisy_t *daisy)
{
   int c, n, found;
   struct group *grp;

   grp = getgrnam ("audio");
   found = 0;
   for (n = 0; grp->gr_mem[n]; n++)
   {
      if (strcmp (grp->gr_mem[n], cuserid (NULL)) == 0)
      {
         found = 1;
         break;
      } // if
   } // for
   if (found == 0)
   {
      beep ();
      endwin ();
      printf ("You need to be a member of the group \"audio\".\n");
      printf ("Please give the following command:\n");
      printf ("\n   sudo gpasswd -a <login-name> audio\n\n");
      printf ("Logout and login again and you can use daisy-player.\n");
      _exit (EXIT_FAILURE);
   } // if

   wclear (misc->screenwin);
   wprintw (misc->screenwin, "\n%s\n\n", gettext ("Select a soundcard:"));

   if (*misc->pulseaudio_device)
      c = atoi (misc->pulseaudio_device);
   else
      c = 0;
   nodelay (misc->screenwin, FALSE);
   while (! misc->term_signaled)
   {
      char dev[5], *str;
      size_t len = 0;
      int total;
      FILE *p;

      if ((p = popen ("LANG=C /usr/bin/pactl list sinks", "r")) == NULL)
      {
         beep ();
         endwin ();
         printf ("%s\n",
      "Be sure the package pulseaudio-utils is installed onto your system.");
         _exit (EXIT_FAILURE);
      } // if

      n = -1;
      str = NULL;
      len = 0;
      while (! misc->term_signaled)
      {
         if (getline (&str, &len, p) == -1)
            break;
         if (strncasecmp (str, "Sink", 4) == 0)
         {
            memset (sink_info[++n], ' ', 70);
         } // if

/* disabled.
   Requested by Didier Spaier <didier@slint.fr>
         if (strstr (str, "Description:"))
         {
            int x;

            strcpy (sink_info[n], str + 14);
            for (x = strlen (sink_info[n]) - 1; x < 70; x++)
               sink_info[n][x] = ' ';
            sink_info[n][x] = 0 ;
         } // if
*/
         if (strcasestr (str, "alsa.card_name"))
         {
            len = strlen (str + 20);
            memcpy (sink_info[n], str + 20, len);
            memset (sink_info[n] + len - 2, ' ', 55 - len);
         } // if
         if (strcasestr (str, "Mute:"))
         {
            strcpy (sink_info[n] + 53, strstr (str, "Mute:"));
            sink_info[n][52] = ' ';
         } // if
         if (strcasestr (str, "Volume:"))
         {
            int x;

            if (strcasestr (str, "Base"))
               continue;
            sink_info[n][strlen (sink_info[n]) - 1] = ' ';
            sink_info[n][strlen (sink_info[n])] = ' ';
            strcpy (sink_info[n] + 63, strstr (str, "Volume:"));
            sink_info[n][70] = 0;
            for (x = strlen (str); x >= 0; x--)
               if (str[x] == '%')
                  break;
            str[x + 1] = 0;
            for (x = strlen (str); x >= 0; x--)
               if (str[x] == ' ')
                  break;
            strcat (sink_info[n], str + x);
         } // if
      } // while
      total = n + 1;
      free (str);
      pclose (p);

/* The build-in pactl function with argument list gives problems.
   So for the time being the external application pactl is used instead.

      memcpy (sink_info, pactl ("list", "0", "0"), 1000);
*/

      for (n = 0; n < total; n++)
         mvwprintw (misc->screenwin, n + 3, 0, "%2d %s ", n, sink_info[n]);
      wmove (misc->screenwin, c + 3, 2);
      wrefresh (misc->screenwin);

      switch (wgetch (misc->screenwin))
      {
      case 13:
         sprintf (misc->pulseaudio_device, "%d", c);
         view_screen (misc, daisy);
         nodelay (misc->screenwin, TRUE);
         return;
      case KEY_DOWN:
         if (++c >= total)
            c = 0;
         break;
      case KEY_UP:
         if (--c == -1)
            c = total - 1;
         break;
      case 'm':
         if (fork () == 0)
         {
            sprintf (dev, "%d", c);
            pactl ("set-sink-mute", dev, "toggle");
            _exit (EXIT_SUCCESS);
         } // if
         break;
      case 'q':
         view_screen (misc, daisy);
         return;
      case 'v':
      case '1':
         if (fork () == 0)
         {
            sprintf (dev, "%d", c);
            pactl ("set-sink-volume", dev, "-5%");
            _exit (EXIT_SUCCESS);
         } // if
         break;
      case 'V':
      case '7':
         if (fork () == 0)
         {
            sprintf (dev, "%d", c);
            pactl ("set-sink-volume", dev, "+5%");
            _exit (EXIT_SUCCESS);
         } // if
         break;
      default:
         beep ();
         break;
      } // switch

      fd_set rfds;
      struct timeval tv;

      FD_ZERO (&rfds);
      FD_SET (0, &rfds);
      tv.tv_sec = 0;
      tv.tv_usec = 100000;
// pause till a key has been pressed or 0.1 seconds are elapsed
      select (1, &rfds, NULL, NULL, &tv);
   } // while
} // select_next_output_device

// Useful to reset handlers in fork()ed children
void reset_term_signal_handlers_after_fork (void)
{
   signal (SIGINT, SIG_DFL);
   signal (SIGTERM, SIG_DFL);
   signal (SIGHUP, SIG_DFL);
} // reset_term_signal_handlers

void free_all (misc_t *misc, my_attribute_t *my_attribute, daisy_t *daisy)
{
   free (misc->pause_resume_id);
   free (misc->prev_id);
   free (misc->current_id);
   free (misc->audio_id);
   free (misc->current_audio_file);
   free (misc->daisy_mp);
   free (misc->tmp_dir);
   free (misc->label);
   free (my_attribute->id);
   free (my_attribute->idref);
   free (my_attribute->src);
   for (misc->current = 0; misc->current < misc->total_items;
        misc->current++)
   {
      free (daisy[misc->current].xml_file);
      free (daisy[misc->current].xml_anchor);
      free (daisy[misc->current].clips_file);
      free (daisy[misc->current].clips_anchor);
   } // for
   delwin (misc->titlewin);
   delwin (misc->screenwin);
} // free_all
