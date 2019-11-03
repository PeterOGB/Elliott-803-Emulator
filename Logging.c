#define G_LOG_USE_STRUCTURED
#define G_LOG_DOMAIN "803 Emulator"

#include <gtk/gtk.h>
#include "Logging.h"


static GLogWriterOutput
log_writer(GLogLevelFlags log_level,
	   const GLogField *fields,
	   __attribute__((unused)) gsize            n_fields,
	   __attribute__((unused))gpointer         user_data)
{
    gsize n;
    const gchar *file,*line,*func,*message,*level;
    const GLogField *field;

    g_return_val_if_fail (fields != NULL, G_LOG_WRITER_UNHANDLED);
    g_return_val_if_fail (n_fields > 0, G_LOG_WRITER_UNHANDLED);

    file=line=func=message=level=NULL;
    
    for(n=0;n<n_fields;n++)
    {
	field = &fields[n];

	if (g_strcmp0 (field->key, "MESSAGE") == 0)
	    message = field->value;
	else if (g_strcmp0 (field->key, "CODE_FILE") == 0)
	    file = field->value;
	else if (g_strcmp0 (field->key, "CODE_LINE") == 0)
	    line = field->value;
	else if (g_strcmp0 (field->key, "CODE_FUNC") == 0)
	    func = field->value;
	else if (g_strcmp0 (field->key, "GLIB_DOMAIN") == 0)
	    func = field->value;
	// These next two are ignored (probably NOT the right thing to do !)
	else if (g_strcmp0 (field->key, "PRIORITY") == 0)
	    continue;
	else if (g_strcmp0 (field->key, "GLIB_OLD_LOG_API") == 0)
	    continue;

	// Assumes value is a string !
	else printf("\nKEY (%s) VALUE(%s) not handled\n",field->key,(const char *)field->value);
    }

    switch (log_level & G_LOG_LEVEL_MASK)
    {
    case G_LOG_LEVEL_ERROR:
	level = "ERROR  ";
	break;
    case G_LOG_LEVEL_CRITICAL:
	level = "CRITICAL";
	break;
    case G_LOG_LEVEL_WARNING:
	level = "WARNING";
	break;
    case G_LOG_LEVEL_MESSAGE:
	level = "MESSAGE";
	break;
    case G_LOG_LEVEL_INFO:
	level = "INFO   ";
	break;
    case G_LOG_LEVEL_DEBUG:
	level = "DEBUG  ";
	break;
    default:
	level = "UNKNOWN";
	break;
    }

    printf("%s:%s:%s:%s:%s",level,file,line,func,message);

    // Add newline if message diesn't have one at the end.
    {
	size_t len = strlen(message);
	if((len > 0) && (message[len-1] != '\n')) printf("\n");
    }
    

    return G_LOG_WRITER_HANDLED;
}


void LoggingInit(void)
{
     g_log_set_writer_func (log_writer,
			   NULL,NULL);

}
