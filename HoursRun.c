

#define G_LOG_USE_STRUCTURED
#include <gtk/gtk.h>

#include "Logging.h"
#include "Common.h"

#include "HoursRun.h"



static GdkPixbuf *digitsPixbuf;
static GdkPixbuf *maskPixbuf;


void HoursRunInit(__attribute__((unused)) GtkBuilder *builder,
		  GString *sharedPath,
		  __attribute__((unused))  GString *userPath)
{
    GString *fileName;
    
    fileName = g_string_new(NULL);

    g_string_printf(fileName,"%sgraphics/HoursRunDigits.xpm",sharedPath->str);
    digitsPixbuf = my_gdk_pixbuf_new_from_file(fileName->str);

    g_string_printf(fileName,"%sgraphics/HoursRunMask.xpm",sharedPath->str);
    maskPixbuf = my_gdk_pixbuf_new_from_file(fileName->str);

    g_info("HoursRun Initialised Ok");

}


void HoursRunTidy( __attribute__((unused)) GString *userPath)
{

    

}
