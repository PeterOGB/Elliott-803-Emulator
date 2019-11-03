
/*  This file is part of the Elliott 803 emulator.

    Copyright © 2019  Peter Onon

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define G_LOG_USE_STRUCTURED
#include <fcntl.h>
#include <libiberty/libiberty.h>
#include <gtk/gtk.h>


#include "Common.h"
#include "Sound.h"

/* Functions for parsing simple text files */
static int fieldCount;
static char **fields = NULL;       // Fields parsed from a line
static Token *currentToken = NULL;
static char *unparsedMessage = NULL;

void postParse(void);             // Forward declaration of use in preParse().

gboolean preParse(char *line)
{
    char **targs;

    if (fields != NULL)
    {
	//g_debug("preParse: postParse not called, freeing previous fields\n");
	postParse();
    }

    if ((fields = buildargv(line)) == NULL)
    {
	//g_debug("buildargv failed to parse!\n\n");
	return FALSE;
    }
    else
    {
	fieldCount = 0;
	for (targs = fields; *targs != NULL; targs++)
	{
	    fieldCount += 1;
	}
    }
    return TRUE;
}

/* Free up the fields array. */

void postParse(void)
{
    if (fields != NULL)
    {
	freeargv(fields);
	fields = NULL;
    }
}

int parse(Token *Tokens, int index)
{
	int m, ret;
	char *tok;

	if ((index >= fieldCount) || (index < 0))
	{
		g_error("parse: index (%d) out or range [0..%d]\n", index, fieldCount-1);
		return FALSE;
	}

	if (fields == NULL)
	{
		g_error("parse: parse called before preParse\n");
		return FALSE;
	}

	tok = fields[index];
	if (tok == NULL)
	{
		g_error("empty message\n");
		return (-1);
	}
	if (Tokens == NULL)
	{
		m = sscanf(tok, "%d", &ret);
		if (m != 1)
		{
			g_error("parse: failed to convert %s to an integer\n", tok);
			return FALSE;
		}
		return (ret);
	}

	/* Use a NULL sting for the end of the table which means the
	 handler can be called as a default if no match has been found.
	 */

	currentToken = NULL;
	while (Tokens->string != NULL)
	{
		if (!strcmp(tok, Tokens->string))
		{

			/* If the handler is NULL, just return the vlaue field.
			 This removes the need for "tokenToValue"  06/11/05 */
			currentToken = Tokens;

			if (Tokens->handler != NULL)
				ret = (Tokens->handler)(index);
			else
				ret = Tokens->value;
			return (ret);
		}
		Tokens++;
	}
	/* No match found, so call the default if one has been defined */
	if (Tokens->handler != NULL)
	{
		ret = (Tokens->handler)(index);
		return (ret);
	}
	return FALSE;
}




int parseFile(gchar *filename,Token *Tokens,const char *comment)
{
    GIOChannel *file;
    gchar *message;
    gsize length,term;
    GError *error = NULL;
    GIOStatus status;
    char *cp;

    g_error("%s called %s\n",__FUNCTION__,filename);

    if((file = g_io_channel_new_file(filename,"r",&error)) == NULL)
    {
	g_error("failed to open file %s\n",filename);
	return FALSE;
    }

    while((status = g_io_channel_read_line(file,&message,&length,&term,&error)) == G_IO_STATUS_NORMAL)
    {
	if(message != NULL)
	{
	    //g_error("%s %s %s",__FUNCTION__,filename,message);
	    if(term != 0)
	    {
		if((cp = strchr(message,'\n')))
		{
		    *cp = ' ';
		}

		unparsedMessage = message;

		if( (comment == NULL) || (*comment != *message))
		{
		    preParse(message);
		    parse(Tokens,0);
		    postParse();
		}
	    }
	    g_free(message);
	    unparsedMessage = NULL;
	}
    }

    g_io_channel_shutdown(file,FALSE,NULL);
    g_io_channel_unref(file);
    if(status == G_IO_STATUS_EOF)
    {
	return TRUE;
    }
    else
    {
	g_error("Error reading file %s\n",filename);
	return FALSE;
    }
}

char *getField(int index)
{
	if ((index >= fieldCount) || (index < 0))
	{
		g_error("parse: index (%d) out or range [0..%d]\n", index, fieldCount-1);
		return NULL;
	}
	return (fields[index]);
}

/** Get the number of fields in the message */
int getFieldCount(void)
{
	return (fieldCount);
}



gboolean readConfigFile(const char *fileName,GString *userPath,Token *savedStateTokens)
{
    GString *configFileName;
    GIOChannel *file;
    GError *error = NULL;
    GIOStatus status;
    gchar *message;
    gsize length,term;
    char *cp;

    configFileName = g_string_new(userPath->str);
    g_string_append_printf(configFileName,"%s",fileName);

    if((file = g_io_channel_new_file(configFileName->str,"r",&error)) == NULL)
    {
	g_error("failed to open file %s\n",configFileName->str);
	return FALSE;
    }

    while((status = g_io_channel_read_line(file,&message,&length,&term,&error)) 
	  == G_IO_STATUS_NORMAL)
    {
	if(message != NULL)
	{
	    //g_error("%s %s %s",__FUNCTION__,filename,message);
	    if(term != 0)
	    {
		if((cp = strchr(message,'\n')))
		{
		    *cp = ' ';
		}

		unparsedMessage = message;

		if(message[0] != '#')
		{
		    preParse(message);
		    parse(savedStateTokens,0);
		    postParse();
		}
	    }
	    g_free(message);
	    unparsedMessage = NULL;
	}
    }

    g_io_channel_shutdown(file,FALSE,NULL);
    g_io_channel_unref(file);
    if(status == G_IO_STATUS_EOF)
    {
	return TRUE;
    }
    else
    {
	g_error("Error reading file %s\n",configFileName->str);
	return FALSE;
    }
}



void updateConfigFile(const char *fileName,GString *userPath,GString *configString)
{
    GString *oldState,*newState,*State;

    GIOChannel *file;
	
    oldState = g_string_new(userPath->str);
    newState = g_string_new(userPath->str);
    State = g_string_new(userPath->str);

    g_string_append_printf(oldState,"%s.old",fileName);
    g_string_append_printf(newState,"%s.new",fileName);
    g_string_append_printf(State,"%s",fileName);

    file = g_io_channel_new_file(newState->str,"w",NULL);
    if(file != NULL)
    {
	g_io_channel_write_chars(file,configString->str,-1,NULL,NULL);

	g_io_channel_shutdown(file,TRUE,NULL);
	g_io_channel_unref (file);
	
	unlink(oldState->str);
	rename(State->str,oldState->str);
	rename(newState->str,State->str);

    }

    g_string_free(State,TRUE);
    g_string_free(newState,TRUE);
    g_string_free(oldState,TRUE);
}


void writeConfigFile(const char *fileName,GString *userPath,GString *configString)
{
    GString *State;

    GIOChannel *file;
	
    State = g_string_new(userPath->str);

    g_string_append_printf(State,"%s",fileName);

    file = g_io_channel_new_file(State->str,"w",NULL);
    if(file != NULL)
    {
	g_io_channel_write_chars(file,configString->str,-1,NULL,NULL);

	g_io_channel_shutdown(file,TRUE,NULL);
	g_io_channel_unref (file);
    }

    g_string_free(State,TRUE);

}

struct sndEffect *readWavData(const char *wavFileName)
{
    int fd;
    ssize_t n;
    char header[40];
    guint32 length;
    char *buffer = NULL;
    struct sndEffect *se;
   
    fd = open(wavFileName,O_RDONLY);

    if(fd == -1) 
    {
	// Not all sound effect files exist, so just ignore missing ones.
	//g_debug("Failed to open file (%s)\n",wavFileName);
        return NULL;
    }

    n = read(fd,header,40);

    n = read(fd,&length,4);
    
    //g_error("length = %d\n",length);

    buffer = (char *) malloc(length);

    n = read(fd,buffer,length);

    if(n != length)
    {
        g_error("read failed for bulk of %s\n",wavFileName);
        free(buffer);
        buffer = NULL;
    }

    close(fd);

    se = (struct sndEffect *) malloc(sizeof(struct sndEffect));
    se->frames = (gint16 *) buffer;
    se->frameCount = (int) (length/2);


  
    return(se);

}


// Simple wrapped that does error checking
GdkPixbuf *
my_gdk_pixbuf_new_from_file (const char *filename)
                          
{
    GdkPixbuf *pb;
    GError *error = NULL;

    pb = gdk_pixbuf_new_from_file(filename, &error);
    if(error != NULL)
    {
	g_error("Failed to read pixbuf file due to (%s)\n",
		error->message);
	g_error_free (error);   // Won't get here if g_error used above !
    }

    return(pb);
}

// Helper function for getting referencies to widgets defined in the glade file
GObject *gtk_builder_get_object_checked(GtkBuilder *builder,const gchar *name)
{
    GObject *gotten;

    gotten = gtk_builder_get_object (builder,name);
    if(gotten == NULL)
    {
	g_error("Couldn't find widget (%s) in glade file.\n",name);
    }
    return gotten;
}
