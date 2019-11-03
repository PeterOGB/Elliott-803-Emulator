/* The hours run clock in the power cabinet of the Elliott 803 emulator.

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
#include <gtk/gtk.h>

#include "Logging.h"
#include "Wiring.h"
#include "Common.h"

#include "HoursRun.h"

static GtkWidget *HoursRunWindow;
static GtkWidget *HoursRunDrawingArea;
static GdkPixbuf *digitsPixbuf;
static int digitPositions[6] = { 0,0,0,0,0,0 };

#define STEP 100
#define NINE 324000
#define TEN  360000

__attribute__((used))
gboolean
on_HoursRunDrawingArea_draw(__attribute__((unused)) GtkWidget *drawingArea,
			  __attribute__((unused)) cairo_t *cr,
			    __attribute__((unused)) gpointer data);


// Repositon the window from config file
static int savedWindowPositionHandler(int nn)
{
    int windowXpos,windowYpos;
    windowXpos = atoi(getField(nn+1));
    windowYpos = atoi(getField(nn+2));
    gtk_window_move(GTK_WINDOW(HoursRunWindow),windowXpos,windowYpos);
    return TRUE;
}

static int savedRunTimeHandler(int nn)
{
    int n;
    for(n=0;n<6;n++)
    {
	 digitPositions[n] = atoi(getField(nn+n+1));
    }

    return TRUE;
}

static Token savedStateTokens[] = {
    {"RunTime",0,savedRunTimeHandler},
    {"WindowPosition",0,savedWindowPositionHandler},
    {NULL,0,NULL}
};

__attribute__((used))
gboolean
on_HoursRunDrawingArea_draw(__attribute__((unused)) GtkWidget *drawingArea,
			  __attribute__((unused)) cairo_t *cr,
			  __attribute__((unused)) gpointer data)
{
    int digit;
    printf("%s called\n",__FUNCTION__);

    cairo_set_source_rgba (cr, 0.5,0.5,0.5,1.0);
   
    cairo_paint(cr);

    cairo_set_source_rgba (cr, 1.0,0.0,0.0,1.0);
    cairo_rectangle(cr,145,0,31,50);
    cairo_fill(cr);

    for(digit = 0; digit < 6; digit+= 1)
    {
	gdk_cairo_set_source_pixbuf (cr,digitsPixbuf,30*digit,8-digitPositions[5-digit]/1200.0);
	cairo_rectangle(cr,30*digit,10,21,30);
	cairo_fill(cr);
    }
    return TRUE;
}


// Keep track of mains availablity to the hours run clock
static gboolean Running = FALSE;

static void HoursRunStart(__attribute__((unused)) unsigned int dummy)
{
    Running = TRUE;

}

static void HoursRunStop(__attribute__((unused)) unsigned int dummy)
{
    Running = FALSE;

}

// Move the numbers on and when 9 showing move on the next digit.
static void HoursRunTimerTick(__attribute__((unused)) unsigned int dummy)
{

    static int counter = 0;

    if(Running) counter += 1;
    if(counter < 100) return;
    counter = 0;

    digitPositions[0] += STEP;
   
    if( digitPositions[0] > NINE ) 
    {
	digitPositions[1] += STEP;

	if( digitPositions[1] > NINE ) 
	{
	    digitPositions[2] += STEP;
	   
	    if( digitPositions[2] > NINE ) 
	    {
		digitPositions[3] += STEP;

		if( digitPositions[3] > NINE ) 
		{
		    digitPositions[4] += STEP;

		    if( digitPositions[4] > NINE ) 
		    {
			digitPositions[5] += STEP;
		
		    }
		}
	    }
	}
	// Wrap positions back to the start of the image
	if( digitPositions[0] >= TEN )  digitPositions[0] -= TEN;
	if( digitPositions[1] >= TEN )  digitPositions[1] -= TEN;
	if( digitPositions[2] >= TEN )  digitPositions[2] -= TEN;
	if( digitPositions[3] >= TEN )  digitPositions[3] -= TEN;
	if( digitPositions[4] >= TEN )  digitPositions[4] -= TEN;
	if( digitPositions[5] >= TEN )  digitPositions[5] -= TEN;

    }
    gtk_widget_queue_draw(HoursRunDrawingArea);
}



void HoursRunInit(__attribute__((unused)) GtkBuilder *builder,
		  GString *sharedPath,
		  __attribute__((unused))  GString *userPath)
{
    GString *fileName;
    

    HoursRunDrawingArea = GTK_WIDGET(gtk_builder_get_object_checked(builder,"HoursRunDrawingArea"));
    HoursRunWindow =  GTK_WIDGET(gtk_builder_get_object_checked(builder,"HoursRunWindow"));

    
    fileName = g_string_new(NULL);

    g_string_printf(fileName,"%sgraphics/HoursRunDigits.xpm",sharedPath->str);
    digitsPixbuf = my_gdk_pixbuf_new_from_file(fileName->str);

    readConfigFile("HoursRunState",userPath,savedStateTokens);
    
    g_string_free(fileName,TRUE);

    connectWires(TIMER100HZ,HoursRunTimerTick);
    connectWires(CHARGER_CONNECTED,HoursRunStart);
    connectWires(CHARGER_DISCONNECTED,HoursRunStop);

    //g_info("HoursRun Initialised Ok");
    gtk_widget_show_all (HoursRunWindow);
}


void HoursRunTidy( __attribute__((unused)) GString *userPath)
{
    GString *configText;
    int windowXpos,windowYpos;

    configText = g_string_new("# HoursRUn configuration\n");

    gtk_window_get_position(GTK_WINDOW(HoursRunWindow), &windowXpos, &windowYpos);
    g_string_append_printf(configText,"WindowPosition %d %d\n",windowXpos,windowYpos);
    g_string_append_printf(configText,"RunTime %d %d %d %d %d %d\n",
			   digitPositions[0],digitPositions[1],digitPositions[2],
			   digitPositions[3],digitPositions[4],digitPositions[5]);
    

    
    updateConfigFile("HoursRunState",userPath,configText);

    g_string_free(configText,TRUE);

    gtk_widget_destroy(HoursRunWindow);
    

}
