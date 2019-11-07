#define G_LOG_USE_STRUCTURED
#include <gtk/gtk.h>
#include <math.h>

#include "fsm.h"
#include "Main.h"
#include "Hands.h"
#include "Contactor.h"
#include "Emulator.h"
#include "Wiring.h"
#include "Common.h"


// Widgets etc
static GtkWidget *ContactorWindow;
// Needs to be global so that timertick in Hands.c can redraw the window 
GtkWidget *ContactorDrawingArea;

// static GdkPixbuf *ContactorBackground_pixbuf;


static gboolean ContactorState = FALSE;
static double ContactorAngle = -1.57;
static double ContactorOffsetAngle = 0.0;
static gboolean HoldingContactor = FALSE;

// Window/Hand management
static gboolean InContactorWindow = FALSE;
static gboolean deferedMotion = FALSE;
static gdouble deferedMotionX,deferedMotionY;
static gdouble FingerPressedAtX = 0,FingerPressedAtY = 0;
//static gboolean warpToLeftHand = FALSE;   Deprecated
//static gboolean warpToRightHand = FALSE;   Deprecated


// Prototypes of handlers used in glade file
__attribute__((used))
gboolean
on_ContactorWindow_enter_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
				   __attribute__((unused)) GdkEventCrossing *event,
				   __attribute__((unused)) gpointer data);

__attribute__((used))
gboolean
on_ContactorWindow_leave_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
				   __attribute__((unused)) GdkEventCrossing *event,
				   __attribute__((unused)) gpointer data);


__attribute__((used))
gboolean
on_ContactorDrawingArea_button_press_event(__attribute__((unused)) GtkWidget *drawingArea,
					__attribute__((unused)) GdkEventButton *event,
					__attribute__((unused)) gpointer data);

__attribute__((used)) 
gboolean
on_ContactorDrawingArea_button_release_event(__attribute__((unused)) GtkWidget *drawingArea,
				    __attribute__((unused)) GdkEventButton *event,
					  __attribute__((unused)) gpointer data);
__attribute__((used))
gboolean
on_ContactorDrawingArea_motion_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
					 __attribute__((unused)) GdkEventMotion *event,
					 __attribute__((unused)) gpointer data);
__attribute__((used))
gboolean
on_ContactorDrawingArea_draw(__attribute__((unused)) GtkWidget *drawingArea,
			  __attribute__((unused)) cairo_t *cr,
			  __attribute__((unused)) gpointer data);

__attribute__((used))
gboolean
on_ContactorWindow_delete_event(__attribute__((unused)) GtkWidget *widget,
				__attribute__((unused)) gpointer data);


// Save the window position in the "IsolatorState" config file.
void ContactorTidy(GString *userPath)
{
    GString *configText;
    int windowXpos,windowYpos;

    //printf("%s called\n",__FUNCTION__);

    configText = g_string_new("# Contactor configuration\n");

    gtk_window_get_position(GTK_WINDOW(ContactorWindow), &windowXpos, &windowYpos);
    g_string_append_printf(configText,"WindowPosition %d %d\n",windowXpos,windowYpos);

    updateConfigFile("IsolatorState",userPath,configText);

    g_string_free(configText,TRUE);

    gtk_widget_destroy(ContactorWindow);

}

// Repositon the window from config file
static int savedWindowPositionHandler(int nn)
{
    int windowXpos,windowYpos;
    windowXpos = atoi(getField(nn+1));
    windowYpos = atoi(getField(nn+2));
    gtk_window_move(GTK_WINDOW(ContactorWindow),windowXpos,windowYpos);
    return TRUE;
}

static Token savedStateTokens[] = {
    {"WindowPosition",0,savedWindowPositionHandler},
    {NULL,0,NULL}
};


void ContactorInit(GtkBuilder *builder,
		GString *sharedPath,
		__attribute__((unused))  GString *userPath)
{
    GString *fileName;
    int width,height;

    // Get widgets from the builder
    ContactorWindow = GTK_WIDGET(gtk_builder_get_object_checked (builder, "ContactorWindow"));
    ContactorDrawingArea = GTK_WIDGET(gtk_builder_get_object_checked(builder,"ContactorDrawingArea"));

    
    fileName = g_string_new(NULL);
    g_string_printf(fileName,"%sgraphics/contactor.xpm",sharedPath->str);
    
    //ContactorBackground_pixbuf =
//	gdk_pixbuf_new_from_file(fileName->str, NULL);

    //  printf("%s ContactorBackground_pixbuf = %p\n",fileName->str, ContactorBackground_pixbuf);

    
    // These dimensions used to come from a background image which is no longer used.
    width = 100;
    height = 120;

    gtk_window_set_default_size(GTK_WINDOW(ContactorWindow), width, height);
    
    g_string_free(fileName,TRUE);

    readConfigFile("IsolatorState",userPath,savedStateTokens);
    
    gtk_widget_show(ContactorWindow);
}


// Simplified version
static int
ContactorWindowEnterHandler(__attribute__((unused)) int s,
			  __attribute__((unused)) int e,
			  void *p)
{
    struct WindowEvent *wep = (struct WindowEvent *)p;
    //printf("%s called \n",__FUNCTION__);
    
    ConfigureLeftHandNew (50.0,50.0,SET_TARGETXY|SET_RESTINGXY|SET_FINGERXY,HAND_NOT_SHOWN);
    LeftHandInfo.Fsm->state = IDLE_HAND;
    LeftHandInfo.gotoRestingWhenIdle = FALSE;

    // Set Right hand to mouse entry point
    ConfigureRightHandNew(wep->eventX,wep->eventY,SET_TARGETXY|SET_RESTINGXY|SET_FINGERXY,HAND_ONE_FINGER);
    RightHandInfo.Fsm->state = TRACKING_HAND;
    RightHandInfo.gotoRestingWhenIdle = FALSE;


    SetActiveWindow(CONTACTORWINDOW);
    // Stop hand being released immediatly after entering the window.
    setEnterDelay(100);
    InContactorWindow = TRUE;

    savedCursor = gdk_window_get_cursor(wep->window);
    gdk_window_set_cursor(wep->window,blankCursor);

    gtk_widget_queue_draw(ContactorDrawingArea);
    return -1;
    
}



static
int
ContactorWindowLeaveHandler(__attribute__((unused)) int s,
			  __attribute__((unused)) int e,
			  void *p)
{
    struct WindowEvent *wep = (struct WindowEvent *)p;
    //printf("%s called \n",__FUNCTION__);
    //setHandsInWindow(NOWINDOW,NEITHER_HAND,NEITHER_HAND,0.0,0.0,0.0,0.0);
    SetActiveWindow(NOWINDOW);
    InContactorWindow = FALSE;

    gdk_window_set_cursor(wep->window,savedCursor);

    gtk_widget_queue_draw(ContactorDrawingArea);

    return -1;

}

static struct fsmtable ContactorWindowTable[] = {
    {OUTSIDE_WINDOW,       FSM_ENTER,       INSIDE_WINDOW,          ContactorWindowEnterHandler},
    {INSIDE_WINDOW,        FSM_LEAVE,       OUTSIDE_WINDOW,         ContactorWindowLeaveHandler},
    {-1,-1,-1,NULL}
}; 

static struct fsm ContactorWindowFSM = { "Contactor Window FSM",0, ContactorWindowTable ,
			 WindowFSMStateNames,WindowFSMEventNames,0,-1};



__attribute__((used))
gboolean
on_ContactorWindow_enter_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
				    __attribute__((unused)) GdkEventCrossing *event,
				    __attribute__((unused)) gpointer data)
{
    //struct WindowEvent we = {CONTACTORWINDOW,event->x,event->y,ContactorEnterHandler,event->window,NULL};
    struct WindowEvent we = {CONTACTORWINDOW,event->x,event->y,event->window,NULL};
    //printf("%s called %d \n",__FUNCTION__,GetActiveWindow());
    if(GetActiveWindow() == NOWINDOW)
    {
	setMouseAtXY(event->x,event->y);
    
	doFSM(&ContactorWindowFSM,FSM_ENTER,(void *) &we);
    }
    
    return TRUE;
}
__attribute__((used))
gboolean
on_ContactorWindow_leave_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
				    __attribute__((unused)) GdkEventCrossing *event,
				    __attribute__((unused)) gpointer data)
{
    //struct WindowEvent we = {CONTACTORWINDOW,event->x,event->y,ContactorLeaveHandler,event->window,NULL};
    struct WindowEvent we = {CONTACTORWINDOW,event->x,event->y,event->window,NULL};
    //printf("%s called \n",__FUNCTION__);
    if(GetActiveWindow() == CONTACTORWINDOW)
    {
	doFSM(&ContactorWindowFSM,FSM_LEAVE,(void *) &we);
    }
    return TRUE;
}



__attribute__((used))
gboolean
on_ContactorDrawingArea_button_press_event(__attribute__((unused)) GtkWidget *drawingArea,
					   GdkEventButton *event,
					   __attribute__((unused)) gpointer data)
{
    HandInfo *trackingHand;
   
    
    //printf("%s called\n",__FUNCTION__);

    if(event->button == 3)
    {
	dropHand();
	return TRUE;
    }

    trackingHand = updateHands(event->x,event->y,&FingerPressedAtX,&FingerPressedAtY);

    if(trackingHand == NULL) return TRUE;

    //ContactorState = !ContactorState;

    {
	double x,y,angle;


	x = event->x - 50.0;
	y = event->y - 60.0;
	
	angle = -atan2(x,y) - G_PI/2.0;
	//printf("diff = %f \n",angle-ContactorAngle);
	if(fabs(angle-ContactorAngle) < 0.3)
	{
	    HoldingContactor = TRUE;
	    ContactorOffsetAngle = angle-ContactorAngle;
	}
	else
	    HoldingContactor = FALSE;

    }
    return TRUE;
}

__attribute__((used)) 
gboolean
on_ContactorDrawingArea_button_release_event(__attribute__((unused)) GtkWidget *drawingArea,
					  __attribute__((unused)) GdkEventButton *event,
					  __attribute__((unused)) gpointer data)
{
    //printf("%s called\n",__FUNCTION__);
    HoldingContactor = FALSE;
    return TRUE;
}

__attribute__((used))
gboolean
on_ContactorDrawingArea_motion_notify_event(GtkWidget *drawingArea,
					    __attribute__((unused)) GdkEventMotion *event,
					    __attribute__((unused)) gpointer data)
{
    gdouble hx,hy;
    

    if(InContactorWindow)
    {
	updateHands(event->x,event->y,&hx,&hy);
	if(HoldingContactor)
	{
	    double x,y,oldAngle;
	

	    x = event->x - 50.0;
	    y = event->y - 60.0;

	    oldAngle = ContactorAngle;
	    
	    ContactorAngle = -atan2(x,y) - G_PI/2.0;
	    ContactorAngle -= ContactorOffsetAngle;
	    
	    //printf("Angle = %f\n",ContactorAngle);
	    if(ContactorAngle < -1.57)
	    {
		ContactorAngle = -1.57;
		HoldingContactor = FALSE;
	    }
	    if(ContactorAngle > 0.0)
	    {
		ContactorAngle = 0.0;
		HoldingContactor = FALSE;
	    }

	    if((oldAngle < -0.79) && (ContactorAngle > -0.79))
	    {
		ContactorAngle = 0.0;
		HoldingContactor = FALSE;
		//printf("POWER ON\n");
		wiring(MAINS_SUPPLY_ON,1);
		ContactorState = TRUE;
	    }
	    else if((oldAngle > -0.79) && (ContactorAngle < -0.79))
	    {
		ContactorAngle = -1.57;
		HoldingContactor = FALSE;
		//printf("POWER OFF\n");
		wiring(MAINS_SUPPLY_OFF,1);
		ContactorState = FALSE;
	    }

	    
	    
	}

	gtk_widget_queue_draw(drawingArea);
    }
     else
    {
	// Cursor motion but not  InWordGenWindow so entry has bee defered
	// by animation in another window.  Save cursor location for use
	// when the defered entry happens.
	deferedMotionX = event->x;
	deferedMotionY = event->y;
	deferedMotion = TRUE;
    }
    
    return TRUE;
}


// TO DO : This should draw the white square and render the text into a surface
// and preserve that between calls.

#define BACKGROUND 0.3
__attribute__((used))
gboolean
on_ContactorDrawingArea_draw(__attribute__((unused)) GtkWidget *drawingArea,
			  __attribute__((unused)) cairo_t *cr,
			  __attribute__((unused)) gpointer data)
{
    gint width, height,size,topEdge;
    static PangoLayout *layout = NULL;  // Retained between calls


    // Do some pango setup on the first call.
    if(layout == NULL)
    {
	PangoFontDescription *desc;
	layout = pango_cairo_create_layout (cr);
	desc = pango_font_description_from_string ("Sans 8");
	pango_layout_set_font_description (layout, desc);
	pango_font_description_free (desc);
    }

    
    // Get the size of the drawing area
    width = gtk_widget_get_allocated_width (drawingArea);
    height = gtk_widget_get_allocated_height (drawingArea);

    size = width - 20;
    topEdge = (height - size) / 2;

    // Fill whole area to black
    // Set fill colour to grey
    
    cairo_set_source_rgba (cr, BACKGROUND,BACKGROUND,BACKGROUND,1.0);
    //cairo_rectangle(cr,0,0,width,height);
    cairo_paint(cr);

    cairo_set_source_rgba (cr, 1.0,1.0,0.0,1.0);
    cairo_rectangle(cr,10,topEdge,size,size);
    cairo_fill(cr);

     cairo_set_line_width(cr, 6);  
     cairo_set_source_rgb(cr, 1.0, 0.0, 0);
  
     cairo_translate(cr, width/2, height/2);
     cairo_arc(cr, 0, 0, (size/2)-3, 0, 2 * M_PI);
     cairo_stroke(cr);
     cairo_fill(cr);

    // Draw Red circle

     cairo_set_source_rgba (cr, 1.0 , 1.0, 1.0, 1.0);
     cairo_move_to(cr,0.0,0.0);
     cairo_arc(cr, 0,0,(size/2)-3,  0.0 ,2.0 * G_PI);
     cairo_fill(cr);

//-----------
     cairo_set_source_rgb(cr, 0, 0, 0);

     pango_layout_set_text (layout, "OFF", -1);
    
     // Redo layout for new text
     pango_cairo_update_layout (cr, layout);
	
     // Retrieve the size of the rendered text
     pango_layout_get_size (layout, &width, &height);

     // Position the text at the end of the radial marks
     cairo_move_to(cr,  ((double)-width /PANGO_SCALE / 2.0),-38.0);
     // Render the text
     pango_cairo_show_layout (cr, layout);
     cairo_stroke(cr);

      pango_layout_set_text (layout, "O\nN", -1);
    
     // Redo layout for new text
     pango_cairo_update_layout (cr, layout);
	
     // Retrieve the size of the rendered text
     pango_layout_get_size (layout, &width, &height);

     // Position the text at the end of the radial marks
     cairo_move_to(cr, 28.0, ((double)-height /PANGO_SCALE / 2.0));
     // Render the text
     pango_cairo_show_layout (cr, layout);
     // cairo_stroke(cr);
// ----------

     cairo_rotate(cr,ContactorAngle);
     cairo_set_line_width(cr, 1);  
     cairo_set_source_rgba (cr, 0.8 , 0.0, 0.0, 1.0);
     cairo_move_to(cr,0.0,0.0);
     cairo_arc(cr, 0,0,(size/2)-3,  0.1* G_PI ,1.9 * G_PI);
     cairo_fill(cr);

    // Knob
    cairo_set_source_rgba (cr, 1.0 , 0.2, 0.0, 1.0);
    cairo_move_to(cr,0.0,10.0);
    cairo_line_to(cr,20.0,0.0);
    cairo_line_to(cr,0.0,-10.0);
    cairo_line_to(cr,-35.0,-7.0);
    cairo_line_to(cr,-35.0,7.0);
    // Draw and fill the trapezium
    cairo_close_path(cr);
    cairo_stroke_preserve(cr);
    cairo_fill(cr); 
     
    cairo_identity_matrix(cr);
     
    if(InContactorWindow)
	DrawHandsNew(cr);

    return TRUE;
}

__attribute__((used))
gboolean
on_ContactorWindow_delete_event(__attribute__((unused)) GtkWidget *widget,
			   __attribute__((unused)) gpointer data)
{
//    gtk_main_quit();

    if(ContactorState == FALSE) EmulatorShutdown();
    
    return TRUE;
}
