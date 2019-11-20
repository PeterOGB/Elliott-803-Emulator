#define G_LOG_USE_STRUCTURED

#include <gtk/gtk.h>
#include <math.h>

#include "Logging.h"
#include "Plotter.h"
#include "Wiring.h"
#include "Hands.h"
#include "Common.h"
#include "fsm.h"


extern int CPU_word_time_count;

static GtkWidget *PlotterWindow = NULL;
GtkWidget *PlotterDrawingArea = NULL;
static GdkPixbuf *background_pixbuf = NULL;

static GdkPixbuf *knobPixbufs[9];
static GdkPixbuf *carriage_pixbuf;
static GdkPixbuf *drum_pixbuf;
static GdkPixbuf *paper_pixbuf;
static GdkPixbuf *visible_pixbuf;
static GdkPixbuf *manualPixbufs[3];
static int knobCount,knobCount2;
static int manualKnobNumber;
static int powerKnobNumber;
static int stickyKnobNumber;
gboolean plotterMoved = FALSE;
static int drumFastMove = 0;
static int carriageFastMove = 0;
static int plotterBusyUntil = 0;
static gboolean PlotterPowerOn = FALSE;
static gboolean PowerSwitchOn = FALSE;
static gboolean PlotterManual = FALSE;
static gboolean V24On = FALSE;

static guint32 exitTimeStamp;
static gdouble LeftHandExitedAtX,LeftHandExitedAtY;
static gdouble RightHandExitedAtX,RightHandExitedAtY;
static gdouble FingerPressedAtX = 0,FingerPressedAtY = 0;
static gboolean warpToLeftHand = FALSE;
static gboolean warpToRightHand = FALSE;
static gboolean InPlotterWindow = FALSE;
static gboolean deferedMotion = FALSE;
static gdouble deferedMotionX,deferedMotionY;
static GList *pressedKeys = NULL;
static GdkSeat *seat = NULL;

static GdkRectangle OneFingerAreas[20];



static struct knobInfo
{
    int type;
    int state;
    void (*handler)(int state);
    int width,height;
    int pixIds[3];
    int xpos,ypos;
    gboolean changed;
    enum WiringEvent wire;

} knobs[20];     /* There are six knobs and one button on the Plotter */
                 /* Plus three paper areas */


static int PenX,PenY;
static gboolean PenDown = TRUE;
static cairo_t *visibleSurfaceCr = NULL;
static GdkRectangle PaperArea = {0,0,0,0};
static gboolean PaperLoaded = FALSE;
static gboolean PaperPositioning = FALSE;
static gboolean PaperMoving = FALSE;
static gdouble  PaperMovedByX = 0.0;
static gdouble  PaperMovedByY = 0.0;
static cairo_t *paperSurfaceCr = NULL;
static cairo_t *drumSurfaceCr = NULL;



__attribute__((used))
gboolean
on_PlotterDrawingArea_draw( __attribute__((unused)) GtkWidget *drawingArea,
			    __attribute__((unused)) cairo_t *cr,
			    __attribute__((unused)) gpointer data);

__attribute__((used))
gboolean
on_PlotterDrawingArea_enter_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
				    __attribute__((unused)) GdkEventCrossing *event,
				    __attribute__((unused)) gpointer data);

__attribute__((used))
gboolean
on_PlotterDrawingArea_leave_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
				    __attribute__((unused)) GdkEventCrossing *event,
				    __attribute__((unused)) gpointer data);

static void on_Hand_motion_event(HandInfo *MovingHand);

static 
void warpToFinger(GdkWindow *win,HandInfo *hand);


__attribute__((used)) 
gboolean
on_PlotterDrawingArea_motion_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
					  __attribute__((unused)) GdkEventMotion *event,
					  __attribute__((unused)) gpointer data);

__attribute__((used))
gboolean
on_PlotterDrawingArea_button_press_event(__attribute__((unused)) GtkWidget *drawingArea,
					 __attribute__((unused)) GdkEventButton *event,
					 __attribute__((unused)) gpointer data);
__attribute__((used))
gboolean
on_PlotterDrawingArea_button_release_event(__attribute__((unused)) GtkWidget *drawingArea,
					   __attribute__((unused)) GdkEventButton *event,
					   __attribute__((unused)) gpointer data);

static int
PlotterWindowEnterHandler(__attribute__((unused)) int s,
			  __attribute__((unused)) int e,
			  void *p)
{
    gdouble EnteredAtX,EnteredAtY;
    struct WindowEvent *wep  = (struct WindowEvent *) p;
    gint wide;
    GtkWidget *drawingArea;

    activeGdkWindow = wep->window;   // BUG FIX ?
    drawingArea = GTK_WIDGET(wep->data);
    
    EnteredAtX = wep->eventX;
    EnteredAtY = wep->eventY;

    // updateHands not called so need to do this
    setMouseAtXY(wep->eventX,wep->eventY);

    SetActiveWindow(PLOTTERWINDOW);

    wide = gtk_widget_get_allocated_width(drawingArea);

    if(EnteredAtX < wide/2)
    {
	ConfigureRightHandNew(400.0,250.0,SET_TARGETXY|SET_RESTINGXY|SET_FINGERXY,HAND_EMPTY);
	if(HandIsEmpty(LeftHand))
	{
	    ConfigureLeftHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_EMPTY);
	}
	else
	{
	    ConfigureLeftHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_HOLDING_REEL);
	}
	setLeftHandMode (TRACKING_HAND);
	setRightHandMode(IDLE_HAND);
	SetActiveWindow(PLOTTERWINDOW);
    }
    else
    {
	ConfigureLeftHandNew (100.0,250.0,SET_TARGETXY|SET_RESTINGXY|SET_FINGERXY,HAND_EMPTY);
	if(HandIsEmpty(RightHand))
	{
	    ConfigureRightHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_EMPTY);
	}
	else
	{
	    ConfigureRightHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_HOLDING_REEL);
	}
	setRightHandMode (TRACKING_HAND);
	setLeftHandMode(IDLE_HAND);
	SetActiveWindow(PLOTTERWINDOW);
    }


    
    // Stop hand being released immediatly after entering the window.
    setEnterDelay(100);

    LeftHandInfo.gotoRestingWhenIdle = FALSE;
    RightHandInfo.gotoRestingWhenIdle = FALSE;
    
    InPlotterWindow = TRUE;

    // if the entry was defered, check where the cursor has moved to since it entered the window
    if(deferedMotion)
    {
	gdouble hx,hy;
	updateHands(deferedMotionX,deferedMotionY,&hx,&hy);

	
	gtk_widget_queue_draw(PlotterDrawingArea);
	
	deferedMotion = FALSE;
    }

     // Remove cursor on entry
   
    savedCursor = gdk_window_get_cursor(wep->window);
    //gdk_window_set_cursor(wep->window,blankCursor);

    register_hand_motion_callback(on_Hand_motion_event);
    
    gtk_widget_queue_draw(PlotterDrawingArea);
#if 0
    if(seat == NULL)     // GRAB TEST
    {
	GdkDisplay *display;
    
	GdkWindow *window;


	printf("GRABBED\n");
	window = wep->window;

    
	display = gdk_display_get_default();
	seat = gdk_display_get_default_seat(display);

	//grabedAtX = event->x_root;
	//grabedAtY = event->y_root;
	//justGrabbed =  TRUE;

	//gdk_window_set_cursor(window,cursorOff);
	//cursorWindow = window;

	gdk_seat_grab (seat,
		       window,
		       GDK_SEAT_CAPABILITY_POINTER|GDK_SEAT_CAPABILITY_KEYBOARD, 
		       FALSE, //gboolean owner_events,
		       NULL, //GdkCursor *cursor,
		       NULL, //const GdkEvent *event,
		       NULL, //GdkSeatGrabPrepareFunc prepare_func,
		       NULL //gpointer prepare_func_data);
	    );
    }
#endif

    
    return -1;
}




static
int
PlotterWinowLeaveHandler(__attribute__((unused)) int s,
			  __attribute__((unused)) int e,
			  void *p)
{
    struct WindowEvent *wep  = (struct WindowEvent *) p;
    //printf("%s called \n",__FUNCTION__);
//    setHandsInWindow(NOWINDOW,USE_NEITHER_HAND,USE_NEITHER_HAND,0.0,0.0,0.0,0.0);


    g_info("Left\n");
    
    register_hand_motion_callback(NULL);
    exitTimeStamp = ((GdkEventCrossing *)wep->data)->time;
    

    getTrackingXY2(&LeftHandInfo,&LeftHandExitedAtX,&LeftHandExitedAtY);
    getTrackingXY2(&RightHandInfo,&RightHandExitedAtX,&RightHandExitedAtY);

    //printf("%s TIME: %"PRIu32" %f %f \n",__FUNCTION__,exitTimeStamp,wep->eventX,wep->eventY);
    
    SetActiveWindow(NOWINDOW);
    InPlotterWindow = FALSE;

    gdk_window_set_cursor(wep->window,savedCursor);
    
    gtk_widget_queue_draw(PlotterDrawingArea);

    // Reset list if any keys pressed when leaving
    g_list_free(pressedKeys);
    pressedKeys = NULL;

    if(seat != NULL) // GRAB TEST
    {
	gdk_seat_ungrab(seat);  
	seat = NULL;
    }
    
    return -1;
}

static int
PlotterWindowUnconstrainedHandler(__attribute__((unused)) int s,
				   __attribute__((unused)) int e,
				   void *p)
{
    struct WindowEvent *wep  = (struct WindowEvent *) p;

    warpToFinger(wep->window,(HandInfo *)wep->data);
    

    return(-1);
}

static struct fsmtable PlotterWindowTable[] = {
    {OUTSIDE_WINDOW,           FSM_ENTER,           INSIDE_WINDOW,            PlotterWindowEnterHandler},
    
    {INSIDE_WINDOW,            FSM_LEAVE,           OUTSIDE_WINDOW,           PlotterWinowLeaveHandler},
    {INSIDE_WINDOW,            FSM_CONSTRAINED,     CONSTRAINED_INSIDE,       NULL},
    
    {CONSTRAINED_INSIDE,       FSM_UNCONSTRAINED,   INSIDE_WINDOW,            PlotterWindowUnconstrainedHandler},

    // Leave this here incase the hand needs to be constrained 
/*
   
    
    {CONSTRAINED_INSIDE,       FSM_UNCONSTRAINED,   INSIDE_WINDOW,            KeyboardWindowUnconstrainedHandler},
    {CONSTRAINED_INSIDE,       FSM_LEAVE,           CONSTRAINED_OUTSIDE,      NULL},
    {CONSTRAINED_INSIDE,       FSM_CONSTRAINED,     BOTH_CONSTRAINED_INSIDE,  NULL}, 

    {BOTH_CONSTRAINED_INSIDE,  FSM_UNCONSTRAINED,   CONSTRAINED_INSIDE,       NULL},
    {BOTH_CONSTRAINED_INSIDE,  FSM_LEAVE,           BOTH_CONSTRAINED_OUTSIDE, NULL},
    
    {CONSTRAINED_OUTSIDE,      FSM_ENTER,           CONSTRAINED_INSIDE,       NULL},
    {CONSTRAINED_OUTSIDE,      FSM_UNCONSTRAINED,   WAITING_FOR_WARP_ENTRY,   KeyboardWindowUnconstrainedHandler},

    {WAITING_FOR_WARP_ENTRY,   FSM_ENTER,           INSIDE_WINDOW,            NULL},

    {BOTH_CONSTRAINED_OUTSIDE, FSM_ENTER,           BOTH_CONSTRAINED_INSIDE,  NULL},
    {BOTH_CONSTRAINED_OUTSIDE, FSM_UNCONSTRAINED,   WAITING_FOR_WARP_ENTRY2,  KeyboardWindowUnconstrainedHandler},
    
    {WAITING_FOR_WARP_ENTRY2,   FSM_ENTER,          CONSTRAINED_INSIDE,       NULL},
    
*/
    {-1,-1,-1,NULL}
}; 


static struct fsm PlotterWindowFSM = { "Plotter Window FSM",0, PlotterWindowTable ,
			 WindowFSMStateNames,WindowFSMEventNames,0,-1};


// The INSIDE flag is used to ignore duplicate entry /leave events caused as a side effect
// of the implicte grab that occures when a button is pressed and the mouse leaves the widget
// before the button is released.  It needs to be added to all windows.

static gboolean INSIDE = FALSE;
__attribute__((used))
gboolean
on_PlotterDrawingArea_enter_notify_event(GtkWidget *drawingArea,
				    __attribute__((unused)) GdkEventCrossing *event,
				    __attribute__((unused)) gpointer data)
{
    struct WindowEvent we = {PLOTTERWINDOW,event->x,event->y,event->window,(gpointer)drawingArea};

    //g_info("IS THIS CALLED ???\n");
    // THis doesn't help !
    //gdk_window_raise(gtk_widget_get_window(drawingArea));

    if(INSIDE)
    {
	//printf("%s DUPLICATE IGNORED\n",__FUNCTION__);
    }
    else
    {
	INSIDE = TRUE;
	doFSM(&PlotterWindowFSM,FSM_ENTER,(void *)&we);
    }
    return TRUE;
}

__attribute__((used))
gboolean
on_PlotterDrawingArea_leave_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
				    __attribute__((unused)) GdkEventCrossing *event,
				    __attribute__((unused)) gpointer data)
{
    struct WindowEvent we = {PLOTTERWINDOW,event->x,event->y,event->window,(gpointer) event};

    if(!INSIDE)
    {
	//printf("%s DUPLICATE IGNORED\n",__FUNCTION__);
    }
    else
    {
	INSIDE = FALSE;
	doFSM(&PlotterWindowFSM,FSM_LEAVE,(void *)&we);
    }
    return TRUE;
}


// These are all in pixels which is half resolution
#define LINES_VISIBLE 330
#define MIDDLE_LINE 165
#define PAPER_WIDE 400
#define PAPER_HIGH 400
#define PAPER_LEFT 10
#define PAPER_TOP 200
#define DRUM_WIDE 616
#define DRUM_HIGH 750
#define DRUM_LEFT 39.0
#define DRUM_TOP 18.0

static GdkRectangle squarePaperSize = { PAPER_LEFT,PAPER_TOP,PAPER_WIDE,PAPER_HIGH};

__attribute__((used))
gboolean
on_PlotterDrawingArea_draw( __attribute__((unused)) GtkWidget *drawingArea,
			    __attribute__((unused)) cairo_t *cr,
			    __attribute__((unused)) gpointer data)
{
    static gboolean firstCall = TRUE;
    static cairo_surface_t *backgroundSurface = NULL;
    static cairo_t *backgroundSurfaceCr = NULL;
    static cairo_surface_t *visibleSurface = NULL;
    static cairo_surface_t *carriageSurface = NULL;
    static cairo_surface_t *drumSurface = NULL;
     static cairo_t *carriageSurfaceCr = NULL;
    //static cairo_t *drumSurfaceCr = NULL;
    GtkAllocation  DrawingAreaAlloc;
    struct knobInfo *knob;
    int topLine;

    if(firstCall)
    {
	firstCall = FALSE;

	// Create surface for the carriage 
	carriageSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						     64,64);
	carriageSurfaceCr = cairo_create(carriageSurface);
	gdk_cairo_set_source_pixbuf (carriageSurfaceCr, carriage_pixbuf ,0.0,0.0);
	cairo_paint(carriageSurfaceCr);

	// Create a surface and an associated context for whole window
	gtk_widget_get_allocation(drawingArea, &DrawingAreaAlloc);
	
	backgroundSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			   DrawingAreaAlloc.width,DrawingAreaAlloc.height);
	backgroundSurfaceCr = cairo_create(backgroundSurface);

	gdk_cairo_set_source_pixbuf (backgroundSurfaceCr, background_pixbuf ,0.0,0.0);
	cairo_paint(backgroundSurfaceCr);


	// Create surface to be shown
	visibleSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						  DRUM_WIDE,DRUM_HIGH+LINES_VISIBLE);
	visibleSurfaceCr = cairo_create(visibleSurface);

	// Initialise it to the drum image.
	if(visible_pixbuf != NULL)
	    gdk_cairo_set_source_pixbuf (visibleSurfaceCr, visible_pixbuf ,0.0,0.0);
	
	cairo_paint(visibleSurfaceCr);


	// Create surface for things drawn on the drum !
	drumSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						  DRUM_WIDE,DRUM_HIGH+LINES_VISIBLE);
	drumSurfaceCr = cairo_create(drumSurface);

	// Initialise it to the drum image.
	gdk_cairo_set_source_pixbuf (drumSurfaceCr, drum_pixbuf ,0.0,0.0);
	cairo_paint(drumSurfaceCr);


	// Moved to plotterInit
#if 0	
	if(paper_pixbuf != NULL)
	{
	    cairo_surface_t *paperSurface = NULL;
	    paperSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						  2*PAPER_WIDE,2*PAPER_HIGH);

	    paperSurfaceCr = cairo_create(paperSurface);
	    gdk_cairo_set_source_pixbuf (paperSurfaceCr, paper_pixbuf ,0.0,0.0);
	
	    cairo_paint(paperSurfaceCr);

	    PaperLoaded = TRUE;
	    PaperArea = loadedPaperSize;
	}
#endif	    


#if 0
	// Add a piece of white paper
	cairo_set_source_rgba(visibleSurfaceCr,1.0,1.0,1.0,1.0);
	cairo_rectangle(visibleSurfaceCr, PAPER_LEFT,PAPER_TOP,PAPER_WIDE,PAPER_HIGH);
	cairo_stroke_preserve(visibleSurfaceCr);
	cairo_fill(visibleSurfaceCr);

	// Add duplicate at bottom of surface
	cairo_set_source_rgba(visibleSurfaceCr,1.0,1.0,1.0,1.0);
	cairo_rectangle(visibleSurfaceCr, PAPER_LEFT,PAPER_TOP+DRUM_HIGH,PAPER_WIDE,PAPER_HIGH);
	cairo_stroke_preserve(visibleSurfaceCr);
	cairo_fill(visibleSurfaceCr);
#endif	


#if 0
	cairo_set_line_width (visibleSurfaceCr, 2);
	cairo_set_source_rgba(visibleSurfaceCr,0.0,1.0,0.0,1.0);
	cairo_move_to(visibleSurfaceCr,0.0,0.0);
	cairo_line_to(visibleSurfaceCr,DRUM_WIDE,0.0);
	cairo_stroke(visibleSurfaceCr);

	cairo_set_source_rgba(visibleSurfaceCr,0.0,0.0,1.0,1.0);
	cairo_move_to(visibleSurfaceCr,0.0,DRUM_HIGH);
	cairo_line_to(visibleSurfaceCr,DRUM_WIDE,DRUM_HIGH);
	cairo_stroke(visibleSurfaceCr);
	
	cairo_set_source_rgba(visibleSurfaceCr,1.0,0.0,0.0,1.0);
	cairo_move_to(visibleSurfaceCr,0.0,LINES_VISIBLE);
	cairo_line_to(visibleSurfaceCr,DRUM_WIDE,LINES_VISIBLE);
	cairo_stroke(visibleSurfaceCr);
#endif
    }


    

    // Check for any knobs that had their "changed" flag set and red1aw them into the background
    // last knob is the manual switch
    for(int knobNumber = 0; knobNumber < knobCount-1; knobNumber += 1)
    {
	knob = &knobs[knobNumber];
	if(knob->changed)
	{
	    knob->changed = FALSE;
	    if(knob->pixIds[knob->state] != -1)
		gdk_cairo_set_source_pixbuf (backgroundSurfaceCr,
					     knobPixbufs[knob->pixIds[knob->state]],
					     knob->xpos,knob->ypos);
	    cairo_paint (backgroundSurfaceCr);

	}
    }
    // Draw manual button in the background surface
    {
	int state;
//	knob = &knobs[knobCount-1];
//	state = (knob->state != 0) ?  1:0; 
//	if((state == 1) && V24On) state = 2;  // Light it up 
	state = (PlotterManual) ? 1 : 0;
	if(PlotterManual && V24On) state = 2; // Light it up
	gdk_cairo_set_source_pixbuf (backgroundSurfaceCr,
				     manualPixbufs[state],
				     285.0,372.0);
	cairo_paint (backgroundSurfaceCr);
    }

    
    /*
    cairo_set_source_rgba(drumSurfaceCr,0.0,0.0,0.0,1.0);
    cairo_set_line_width (drumSurfaceCr, 1);
    cairo_set_line_cap  (drumSurfaceCr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to (drumSurfaceCr, counter, 50.0); cairo_line_to (drumSurfaceCr, counter, 50.0);
    cairo_stroke (drumSurfaceCr);
    counter += 1;
    */

    // Render the background surface
    cairo_set_source_surface (cr, backgroundSurface ,0.0,0.0);
    cairo_paint(cr);

    // Set top line of the image
    topLine = (PenY/2) - MIDDLE_LINE;

    // If close to the top, use the duplicate at the bottom.
    if((PenY/2)<MIDDLE_LINE)
    {
        topLine = DRUM_HIGH - (MIDDLE_LINE -(PenY/2));
    }
      
    // Draw the part of the drum surface visible in the window
    cairo_set_source_surface (cr, visibleSurface ,DRUM_LEFT,-topLine+DRUM_TOP);
    
    cairo_rectangle(cr, DRUM_LEFT, DRUM_TOP,DRUM_WIDE, LINES_VISIBLE);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);


    if(PaperPositioning)
    {
	int reducedHeight;
	int start;

	start = PaperMovedByY+PaperArea.y;
	reducedHeight = 0;

	if(start < DRUM_TOP)
	{
	    reducedHeight =  start - DRUM_TOP ;
	    start = DRUM_TOP;
	}
	
	cairo_set_source_rgba(cr,0.9,0.9,0.9,1.0);
	if(start+PaperArea.height >= LINES_VISIBLE+DRUM_TOP)
	{
	    reducedHeight += LINES_VISIBLE+DRUM_TOP - (PaperArea.y+PaperMovedByY);
	}
	else
	    reducedHeight += PaperArea.height;
	
	cairo_rectangle(cr,PaperMovedByX+PaperArea.x,start,PaperArea.width,reducedHeight);
	cairo_stroke_preserve(cr);
	cairo_fill(cr);


    }


    // Draw carraige support bars
    {
	gdouble grey;
	cairo_set_line_width (cr, 1);
	for(int bar = 0; bar < 4; bar ++)
	{
	    grey = bar / 3.0;
	    grey = 1.0 - grey;
	    cairo_set_source_rgba(cr,grey,grey,grey,1.0);
	    cairo_move_to (cr,DRUM_LEFT           +0.5,-25 + DRUM_TOP + MIDDLE_LINE + bar + 0.5);
	    cairo_line_to (cr,DRUM_LEFT+DRUM_WIDE +0.5,-25 + DRUM_TOP + MIDDLE_LINE + bar + 0.5);
	    cairo_move_to (cr,DRUM_LEFT           +0.5,+26 + DRUM_TOP + MIDDLE_LINE + bar + 0.5);
	    cairo_line_to (cr,DRUM_LEFT+DRUM_WIDE +0.5,+26 + DRUM_TOP + MIDDLE_LINE + bar + 0.5);

	    cairo_move_to (cr,DRUM_LEFT           +0.5,-26 + DRUM_TOP + MIDDLE_LINE - bar + 0.5);
	    cairo_line_to (cr,DRUM_LEFT+DRUM_WIDE +0.5,-26 + DRUM_TOP + MIDDLE_LINE - bar + 0.5);
	    cairo_move_to (cr,DRUM_LEFT           +0.5,+25 + DRUM_TOP + MIDDLE_LINE - bar + 0.5);
	    cairo_line_to (cr,DRUM_LEFT+DRUM_WIDE +0.5,+25 + DRUM_TOP + MIDDLE_LINE - bar + 0.5);
	    cairo_stroke(cr);
	}
	// Draw the pen wire
	grey = 0.8;
	cairo_set_source_rgba(cr,grey,grey,grey,1.0);
	cairo_move_to (cr,DRUM_LEFT           +0.5,DRUM_TOP + MIDDLE_LINE + 0.5);
	cairo_line_to (cr,DRUM_LEFT+DRUM_WIDE +0.5,DRUM_TOP + MIDDLE_LINE + 0.5);
	cairo_stroke(cr);
	grey = 0.5;
	cairo_set_source_rgba(cr,grey,grey,grey,1.0);
	cairo_move_to (cr,DRUM_LEFT           +0.5, 1+DRUM_TOP + MIDDLE_LINE + 0.5);
	cairo_line_to (cr,DRUM_LEFT+DRUM_WIDE +0.5, 1+DRUM_TOP + MIDDLE_LINE + 0.5);
	cairo_move_to (cr,DRUM_LEFT           +0.5,-1+DRUM_TOP + MIDDLE_LINE + 0.5);
	cairo_line_to (cr,DRUM_LEFT+DRUM_WIDE +0.5,-1+DRUM_TOP + MIDDLE_LINE + 0.5);
	cairo_stroke(cr);
    }
    

    // Draw the carriage
    cairo_set_source_surface(cr,carriageSurface,DRUM_LEFT+(PenX/2), DRUM_TOP+MIDDLE_LINE-32.0);
    cairo_rectangle(cr, DRUM_LEFT+(PenX/2),  DRUM_TOP+MIDDLE_LINE-32.0, 64.0, 64.0);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);
    
    if(InPlotterWindow)
	DrawHandsNew(cr); 
    return FALSE;
}


__attribute__((used)) 
gboolean
on_PlotterDrawingArea_motion_notify_event(GtkWidget *drawingArea,
					  __attribute__((unused)) GdkEventMotion *event,
					  __attribute__((unused)) gpointer data)
{
    //gdouble hx,hy;
    
    //printf("%s called\n",__FUNCTION__);
    if(warpToLeftHand)
    {
/*	
	int ox,oy;
	GdkWindow *win;
	win = gtk_widget_get_parent_window (drawingArea);

	gdk_window_get_origin (win,&ox,&oy);

	ox += ((gint) LeftHandInfo.FingerAtX);
	oy += ((gint) LeftHandInfo.FingerAtY);
	
	gdk_device_warp (event->device,
                         gdk_screen_get_default(),
                         ox,oy);

*/
	warpToFinger(gtk_widget_get_parent_window (drawingArea),&LeftHandInfo);
	warpToLeftHand = FALSE;
	return GDK_EVENT_STOP;
    }
    
    if(warpToRightHand)
    {
	int ox,oy;
	GdkWindow *win;
	win = gtk_widget_get_parent_window (drawingArea);

	gdk_window_get_origin (win,&ox,&oy);
	
	ox += ((gint) RightHandInfo.FingerAtX);
	oy += ((gint) RightHandInfo.FingerAtY);

	gdk_device_warp (event->device,
                         gdk_screen_get_default(),
                         ox,oy);

	warpToRightHand = FALSE;
	return GDK_EVENT_STOP;
    }

    if(InPlotterWindow)
    {
	updateHands(event->x,event->y,NULL,NULL);

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
	    
    return GDK_EVENT_STOP;
}



__attribute__((used))
gboolean
on_PlotterDrawingArea_button_press_event(__attribute__((unused)) GtkWidget *drawingArea,
					 __attribute__((unused)) GdkEventButton *event,
					 __attribute__((unused)) gpointer data)
{
    HandInfo *trackingHand;
    struct knobInfo *knob;
    int top,bottom,left,right;
    GdkRectangle *OneFingerArea;
    
    if(event->button == 3)
    {
	//dropHand();
	swapHands(drawingArea);
	return GDK_EVENT_STOP;
    }

    trackingHand = updateHands(event->x,event->y,&FingerPressedAtX,&FingerPressedAtY);
    if( (trackingHand != NULL) && (trackingHand->showingHand == HAND_ONE_FINGER)  )
    {
	trackingHand->FingersPressed |= trackingHand->IndexFingerBit;
	knob = knobs;
	for(int knobNumber= 0; knobNumber < knobCount; knobNumber += 1,knob++)
	{
	    left = knob->xpos;
	    right = left + knob->width;
	    top = knob->ypos;
	    bottom = top + knob->height;

	    if((FingerPressedAtX >= left) && (FingerPressedAtX <= right) &&
	       (FingerPressedAtY >= top) && (FingerPressedAtY <= bottom))
	    {

		switch(knob->type)
		{
		case 0:
		case 2:
		    if((FingerPressedAtX - left) >= (knob->width/2))
		    {
			if(knob->state < 2) knob->state += 1;
			//g_info("Right Hit button number %d %d ",knobNumber,knob->state);
		    }
		    else
		    {
			if(knob->state > 0) knob->state -= 1;
			//g_info("Left  Hit button number %d %d",knobNumber,knob->state);
		    }
		    knob->changed = TRUE;
		    break;
		case 1:
		    if((FingerPressedAtX - left) >= (knob->width/2))
		    {
			if(knob->state < 2) knob->state = 2;
			//g_info("Right Hit button number %d %d ",knobNumber,knob->state);
		    }
		    else
		    {
			if(knob->state > 0) knob->state = 0;
			//g_info("Left  Hit button number %d %d",knobNumber,knob->state);
		    }
		    knob->changed = TRUE;
		    break;
		case 3:
		    
		    switch(knob->state)
		    {
		    case 0:
			knob->state = 1;

			break;
		    case 1:
			knob->state = 2;
		    default:
			break;
		    }
		    break;

		default:
		    break;
		}
		if(knob->handler != NULL) (knob->handler)(knob->state);
		break;
	    }
 
	}
	for(int areaNumber= knobCount; areaNumber < knobCount2; areaNumber += 1)
	{
	    OneFingerArea = &OneFingerAreas[areaNumber];
	    left = OneFingerArea->x;
	    right = left + OneFingerArea->width;
	    top = OneFingerArea->y;
	    bottom = top + OneFingerArea->height;

	    if((FingerPressedAtX >= left) && (FingerPressedAtX <= right) &&
	       (FingerPressedAtY >= top) && (FingerPressedAtY <= bottom))
	    {
		if(knobs[areaNumber].handler != NULL) (knobs[areaNumber].handler)(knob->state);
	    }
	}


    }
    // Check to see if placing sticky tape on paper and drum
    else if( (trackingHand != NULL) && (trackingHand->showingHand == HAND_STICKY_TAPE)  )
    {
	g_debug("PressedAtX=%f PressedAtY=%f %f\n",FingerPressedAtX,FingerPressedAtY,
		(FingerPressedAtY - PaperArea.y));
	if( (fabs(FingerPressedAtY - PaperArea.y) < 7.0) &&
	    (FingerPressedAtX >= PaperArea.x) &&
	    (FingerPressedAtX <= PaperArea.x+PaperArea.width))
	{
	    cairo_surface_t *paperSurface = NULL;
	    int topLine;
	    // Put paper onto the drum
	    cairo_set_source_rgba(visibleSurfaceCr,0.5,1.0,1.0,1.0);
	    //cairo_rectangle(visibleSurfaceCr, 32+PAPER_LEFT,PAPER_TOP,PAPER_WIDE,PAPER_HIGH);

	    topLine = (PenY/2) - MIDDLE_LINE;



	    cairo_rectangle(visibleSurfaceCr,PaperArea.x-DRUM_LEFT,PaperArea.y+topLine-DRUM_TOP,PaperArea.width,PaperArea.height);
	    cairo_stroke_preserve(visibleSurfaceCr);
	    cairo_fill(visibleSurfaceCr);

	    cairo_rectangle(visibleSurfaceCr,PaperArea.x-DRUM_LEFT,DRUM_HIGH+PaperArea.y+topLine-DRUM_TOP,PaperArea.width,PaperArea.height);
	    cairo_stroke_preserve(visibleSurfaceCr);
	    cairo_fill(visibleSurfaceCr);

/*

	    // Add duplicate at bottom of surface
	    cairo_set_source_rgba(visibleSurfaceCr,1.0,1.0,1.0,1.0);
	    cairo_rectangle(visibleSurfaceCr, 32+PAPER_LEFT,PAPER_TOP+DRUM_HIGH,PAPER_WIDE,PAPER_HIGH);
	    cairo_stroke_preserve(visibleSurfaceCr);
	    cairo_fill(visibleSurfaceCr);
*/

	    // Create full resolution surface for the paper
	    paperSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						      2*PAPER_WIDE,2*PAPER_HIGH);

	    paperSurfaceCr = cairo_create(paperSurface);
	    // Clear to white
	    cairo_set_source_rgba(paperSurfaceCr,1.0,1.0,1.0,1.0);
	    cairo_paint(paperSurfaceCr);


	    
	    trackingHand->showingHand = HAND_EMPTY;
	    PaperPositioning = FALSE;
	    PaperMoving = FALSE;
	}

	
	
    }
    // Check to see if picking up piece of sticky tape
    else if( (trackingHand != NULL) && (trackingHand->showingHand == HAND_GRABBING)  )
    {
	trackingHand->showingHand = HAND_STICKY_TAPE;
    }
    if( (trackingHand != NULL)  && (trackingHand->showingHand == HAND_EMPTY) && PaperPositioning )
    {

	g_debug("PressedAt (%f,%f)  Paper (%d,%d) to (%d,%d)\n",
		FingerPressedAtX,FingerPressedAtY,
		PaperArea.x,PaperArea.y,
		PaperArea.x+PaperArea.width,PaperArea.y+PaperArea.height);

	if((FingerPressedAtX >= PaperArea.x) && (FingerPressedAtX <= PaperArea.x+PaperArea.width) &&
	   (FingerPressedAtY >= PaperArea.y) && (FingerPressedAtY <= PaperArea.y+PaperArea.height))
	{	
	    trackingHand->showingHand = HAND_ALL_FINGERS;

	
	    g_debug("PRESS %d \n",trackingHand->showingHand);
	    PaperMoving = TRUE;
	}
    }

    
    return GDK_EVENT_STOP;
}


__attribute__((used))
gboolean
on_PlotterDrawingArea_button_release_event(__attribute__((unused)) GtkWidget *drawingArea,
					   __attribute__((unused)) GdkEventButton *event,
					   __attribute__((unused)) gpointer data)
{
    HandInfo *trackingHand;
    struct knobInfo *knob;
    int top,bottom,left,right;
    
    if(event->button == 3)
    {
	return GDK_EVENT_STOP;
    }

    //g_info("Releaase\n");
    
    trackingHand = updateHands(event->x,event->y,&FingerPressedAtX,&FingerPressedAtY);
    
    if( (trackingHand != NULL) && (trackingHand->showingHand == HAND_ONE_FINGER)  )
    {
	knob = knobs;
	for(int knobNumber = 0; knobNumber < knobCount; knobNumber += 1,knob++)
	{
	    left = knob->xpos;
	    right = left + knob->width;
	    top = knob->ypos;
	    bottom = top + knob->height;

	    if((FingerPressedAtX >= left) && (FingerPressedAtX <= right) &&
	       (FingerPressedAtY >= top) && (FingerPressedAtY <= bottom))
	    {

		switch(knob->type)
		{
		case 2:
		    knob->state = 1;
		    knob->changed = TRUE;
		    break;
		case 3:
		    
		    switch(knob->state)
		    {
		    case 2:
			knob->state = 0;
			knob->changed = TRUE;
			break;
		    default:
			break;
		    }
		    break;
		default:
		    break;
		}
		if((knob->changed) && (knob->handler != NULL)) (knob->handler)(knob->state);
		
		break;
	    }
	}
    }



    // Not needed on theplotter
    // Always release the finger (even if not over a button).
    //if(trackingHand != NULL) trackingHand->FingersPressed &= ~trackingHand->IndexFingerBit;

    if(PaperMoving)
    {
	PaperMoving = FALSE;
	PaperArea.x += PaperMovedByX;
	PaperMovedByX = 0.0;
	PaperArea.y += PaperMovedByY;
	PaperMovedByY = 0.0;
	g_debug("Paper Top Corner Dropped at (%d,%d)\n",PaperArea.x,PaperArea.y);
	trackingHand->showingHand = HAND_EMPTY;
    }

    if((trackingHand != NULL) && (trackingHand->handConstrained != HAND_NOT_CONSTRAINED))
//	&& PaperMoving )
    {
	struct WindowEvent we = {PLOTTERWINDOW,0,0,event->window,(gpointer) trackingHand};
	trackingHand->handConstrained = HAND_NOT_CONSTRAINED;
	doFSM(&PlotterWindowFSM,FSM_UNCONSTRAINED,(void *)&we);
    }


    
    return GDK_EVENT_STOP;
}






// Used to repositoin the mouse cursor at the tracking hand when the hand becomes
// unconstrained after trying to move with fingers pressed
static 
void warpToFinger(GdkWindow *win,HandInfo *hand)
{
    int ox,oy;

    //win = gtk_widget_get_parent_window (drawingArea);

    gdk_window_get_origin (win,&ox,&oy);

    ox += ((gint) hand->FingerAtX);
    oy += ((gint) hand->FingerAtY);
	
    gdk_device_warp (gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_display_get_default())),
		     gdk_screen_get_default(),
		     ox,oy);
}






// Called from the timerTick in Hands.c when a hand (not the mouse cursor) moves.
static void on_Hand_motion_event(HandInfo *movingHand)
{

    int n,ix,iy;
    enum  handimages showing;
    gdouble hx,hy;
    gboolean overKnob = FALSE;
    gboolean overSticky = FALSE;
    
    //printf("%s called\n",__FUNCTION__);

    //movingHand = updateHands(event->x,event->y,&hx,&hy);
    getTrackingXY2(movingHand,&hx,&hy);
    ix = (int) hx;
    iy = (int) hy;

    //showing = HAND_EMPTY;
    showing = movingHand->showingHand;


    for(n=0;n<knobCount2;n++)
    {
	if( (ix >= OneFingerAreas[n].x) &&
	    (ix <= (OneFingerAreas[n].x+OneFingerAreas[n].width)) &&
	    (iy >= OneFingerAreas[n].y) &&
	    (iy <= (OneFingerAreas[n].y+OneFingerAreas[n].height)) )
	{
	    overKnob = TRUE;
	    break;
	}
    }

    if( (ix >= OneFingerAreas[stickyKnobNumber].x) &&
	(ix <= (OneFingerAreas[stickyKnobNumber].x+OneFingerAreas[stickyKnobNumber].width)) &&
	(iy >= OneFingerAreas[stickyKnobNumber].y) &&
	(iy <= (OneFingerAreas[stickyKnobNumber].y+OneFingerAreas[stickyKnobNumber].height)) )
    {
	overSticky = TRUE;
    }

    
      
    if(showing == HAND_EMPTY)
    {
	if(overKnob)
	{
	    showing = HAND_ONE_FINGER;
	}
	if(overSticky)
	{
	    showing = HAND_GRABBING;
	}
    }
    else
    {
	if( !(overKnob || overSticky) && ( (showing == HAND_ONE_FINGER) || (showing == HAND_GRABBING)))
	{
	    showing = HAND_EMPTY;
	}
	

    }


    
  
    if(PaperMoving)
    {
	g_debug("MOved (%f,%f)\n",hx-FingerPressedAtX,hy-FingerPressedAtY);
	PaperMovedByX = hx-FingerPressedAtX;
	PaperMovedByY = hy-FingerPressedAtY;
    }


    

    if((movingHand->handConstrained == HAND_NOT_CONSTRAINED) && PaperMoving)
    {
	if((PaperArea.x + PaperMovedByX - 32) < DRUM_LEFT)
	{

	    movingHand->handConstrained = HAND_CONSTRAINED_BY_PRESS;
	    printf("*************** CONSTRAINED *********************\n");
	    doFSM(&PlotterWindowFSM,FSM_CONSTRAINED,NULL);

	}

    }

/*
	if(movingHand->FingersPressed != 0)
	{
	    if(!overVolumeControl && !wasOverVolumeControl)
	    {
		movingHand->handConstrained = HAND_CONSTRAINED_BY_PRESS;
		//printf("*************** CONSTRAINED *********************\n");
		doFSM(&KeyboardWindowFSM,FSM_CONSTRAINED,NULL); //(void *)&we);
	    }
	    else
	    {
		if(!overVolumeControl && wasOverVolumeControl)
		{
		    movingHand->handConstrained = HAND_CONSTRAINED_BY_VOLUME;
		    //printf("*************** CONSTRAINED BY VOLUME CONTROL *********************\n");
		    doFSM(&KeyboardWindowFSM,FSM_CONSTRAINED,NULL); //(void *)&we);
		    showing = HAND_ONE_FINGER;
		}
		if(overVolumeControl)
		{
		    gboolean under,over;
		    under = (volumeY + deltaVolume) < 0;
		    over  = (volumeY + deltaVolume) > 40;
		    //printf("volumeY=%d deltaVolulme=%d\n",volumeY,deltaVolume);
		    if(under || over )
		    {
			if(under) volumeY = -deltaVolume;
			if(over) volumeY = 40 - deltaVolume;
			if(deltaVolume != 0)
			{
			movingHand->handConstrained = HAND_CONSTRAINED_BY_VOLUME;
			//printf("*************** CONSTRAINED BY VOLUME CONTROL *********************\n");
			doFSM(&KeyboardWindowFSM,FSM_CONSTRAINED,NULL); //(void *)&we);
			showing = HAND_ONE_FINGER;
			}
		    }
		    wiring(VOLUME_CONTROL,(unsigned int) (volumeY+deltaVolume));
		}
	    }
	}
    }
    else if(movingHand->handConstrained == HAND_CONSTRAINED_BY_VOLUME)
    {
	showing = HAND_ONE_FINGER;
    }

    // Save this to detect if the hand has moved off the volume control
    wasOverVolumeControl = overVolumeControl;
*/


    if(movingHand == &LeftHandInfo)
    {
	
	if(LeftHandInfo.showingHand != showing)
	{
	    g_debug("Left hand changed\n");
	    ConfigureLeftHandNew (0.0,0.0,0,showing);
	}
    }

    if(movingHand == &RightHandInfo)
    {
	if(RightHandInfo.showingHand != showing)
	    ConfigureRightHandNew (0.0,0.0,0,showing);
    }
}


// CPU interface functions

static unsigned int CLines = 0;
static gboolean PLOTTERF72 = FALSE;


static void V24changed(unsigned int value)
{
    V24On = (value != 0);
    plotterMoved = TRUE;  // Force a redraw
    PlotterPowerOn = PowerSwitchOn && V24On;
    if(!PlotterPowerOn)
	PenDown = TRUE;
    g_info("24 Volts %s\n",V24On ? "ON" : "OFF");
}


static void F72changed(unsigned int value)
{
    if(value == 1)
    {
	if((CLines & 7168) == 7168)
	{
	    PLOTTERF72 = TRUE;
	    if( (CPU_word_time_count >= plotterBusyUntil) && !PlotterManual && PlotterPowerOn)
		wiring(READY,1);
	}
    }
    else
    {
	PLOTTERF72 = FALSE;
    }
}
static void ClinesChanged(unsigned int value)
{
    CLines = value;
}

static void wrapY(void)
{

    if(PenY <0)
    {
	PenY += (2*DRUM_HIGH);
	g_info("Wrap 1 %d\n",PenY);
    }

    if(PenY > (2*DRUM_HIGH))
    {
	PenY -= (2*DRUM_HIGH);
	g_info("Wrap 2 %d\n",PenY);
    }
}

static void limitX(void)
{
	if(PenX > 1100)
	{
	    PenX = 1100;
	} else 	if(PenX < 0)
	{
	    PenX = 0;
	}
}

static void plotPoint(void)
{
    gdouble halfX,halfY,halfXPlus;
    halfX = PenX/2;
    halfXPlus = halfX + 32.5;
    halfY = PenY/2;
	
    cairo_set_source_rgba(visibleSurfaceCr,0.0,0.0,0.0,1.0);
    cairo_set_line_width (visibleSurfaceCr, 1);
    cairo_set_line_cap  (visibleSurfaceCr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to (visibleSurfaceCr, halfXPlus,0.5+(halfY));
    cairo_line_to (visibleSurfaceCr, halfXPlus,0.5+(halfY));
    cairo_stroke (visibleSurfaceCr);

    // Duplicate at bottom
    if((halfY)<=LINES_VISIBLE)
    {
	cairo_set_source_rgba(visibleSurfaceCr,0.0,0.0,0.0,1.0);
	cairo_move_to (visibleSurfaceCr, halfXPlus,0.5+(halfY)+DRUM_HIGH);
	cairo_line_to (visibleSurfaceCr, halfXPlus,0.5+(halfY)+DRUM_HIGH);
	cairo_stroke (visibleSurfaceCr);
    }



    if((halfX >= PaperArea.x) && (halfX <= (PaperArea.x+PaperArea.width)) &&
       (halfY >= PaperArea.y) && (halfY <= (PaperArea.y+PaperArea.height)))
    {
	
	//printf("ON  Paper %d %f\n",PenX,(2.0*PaperArea.x));
	// Draw full resolution version if paper loaded
	if(paperSurfaceCr != NULL)
	{
	    cairo_set_source_rgba(paperSurfaceCr,0.0,0.0,0.0,1.0);
	    cairo_set_line_width (paperSurfaceCr, 1);
	    cairo_set_line_cap  (paperSurfaceCr, CAIRO_LINE_CAP_ROUND);
	    cairo_move_to (paperSurfaceCr, 0.5+PenX-(2*PaperArea.x),0.5+PenY-(2*PaperArea.y));
	    cairo_line_to (paperSurfaceCr, 0.5+PenX-(2*PaperArea.x),0.5+PenY-(2*PaperArea.y));
	    cairo_stroke (paperSurfaceCr);
	}
    }
    else
    {
	cairo_set_source_rgba(drumSurfaceCr,0.0,0.0,0.0,1.0);
	cairo_set_line_width (drumSurfaceCr, 1);
	cairo_set_line_cap  (drumSurfaceCr, CAIRO_LINE_CAP_ROUND);
	cairo_move_to (drumSurfaceCr, halfXPlus,0.5+(halfY));
	cairo_line_to (drumSurfaceCr, halfXPlus,0.5+(halfY));
	cairo_stroke (drumSurfaceCr);

	// Duplicate at bottom
	if((halfY)<=LINES_VISIBLE)
	{
	    cairo_set_source_rgba(drumSurfaceCr,0.0,0.0,0.0,1.0);
	    cairo_move_to (drumSurfaceCr, halfXPlus,0.5+(halfY)+DRUM_HIGH);
	    cairo_line_to (drumSurfaceCr, halfXPlus,0.5+(halfY)+DRUM_HIGH);
	    cairo_stroke (drumSurfaceCr);
	}

	//printf("OFF Paper\n");
    }
}

static void ACTchanged(unsigned int value)
{
    if(value == 1)
    {
	if(PLOTTERF72)
	{
	    if(CLines & 1)
	    {
		PenX += 1;
		limitX();
	    }
	    if(CLines & 2)
	    {
		PenX -= 1;
		limitX();
	    }
	    if(CLines & 4)
	    {
		PenY -= 1;
		wrapY();
	    }
	    if(CLines & 8)
	    {
		PenY += 1;
		wrapY();
	    }
	    if(CLines & 16)
	    {
		PenDown = FALSE;
	    }
	    if(CLines & 32)
	    {
		PenDown = TRUE;
	    }

	    if(PenDown)
	    {
		plotPoint();
	    }
	    plotterMoved = TRUE;

	    if(CLines & 060)
		plotterBusyUntil = 350;
	    else
		plotterBusyUntil = 11;
	    
	    wiring(READY,0);
	}
    }
    else
    {
	if(PLOTTERF72)
	{
	    plotterBusyUntil += CPU_word_time_count;
	}

    }
}



// Called from TIMER100HZ wiring
static void fastMovePen( __attribute__((unused)) unsigned int value)
{
    if(PlotterPowerOn)
    {
	if(drumFastMove != 0)
	{
	    PenY +=  drumFastMove;
	    wrapY();
	}

	if(carriageFastMove != 0)
	{
	    PenX +=  carriageFastMove;
	    limitX();
	}

	if((drumFastMove != 0) || (carriageFastMove != 0))
	{
	    if(PenDown)
	    {
		plotPoint();
	    }
	    plotterMoved = TRUE;
	}
    }
}



// Handlers for knobs

static void squarePaperHandler(__attribute__((unused)) int state)
{
    
    static cairo_surface_t *paperSurface = NULL;
    static GdkRectangle Paper = {150,50,200,200};

    g_info("squarePaperHandler\n");

    PaperPositioning = TRUE;
    PaperArea = Paper;

    
#if 0
    if(PaperLoaded == FALSE)
    {
	// Put paper onto the drum
	cairo_set_source_rgba(visibleSurfaceCr,1.0,1.0,1.0,1.0);
	cairo_rectangle(visibleSurfaceCr, 32+PAPER_LEFT,PAPER_TOP,PAPER_WIDE,PAPER_HIGH);
	cairo_stroke_preserve(visibleSurfaceCr);
	cairo_fill(visibleSurfaceCr);

	// Add duplicate at bottom of surface
	cairo_set_source_rgba(visibleSurfaceCr,1.0,1.0,1.0,1.0);
	cairo_rectangle(visibleSurfaceCr, 32+PAPER_LEFT,PAPER_TOP+DRUM_HIGH,PAPER_WIDE,PAPER_HIGH);
	cairo_stroke_preserve(visibleSurfaceCr);
	cairo_fill(visibleSurfaceCr);


	// Create full resolution surface for the paper
	paperSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						  2*PAPER_WIDE,2*PAPER_HIGH);

	paperSurfaceCr = cairo_create(paperSurface);
	// Clear to white
	cairo_set_source_rgba(paperSurfaceCr,1.0,1.0,1.0,1.0);
	cairo_paint(paperSurfaceCr);
	
	PaperLoaded = TRUE;
	PaperArea = squarePaperSize;
    }
    else
    {
	// Take paper off the drum
	// free any previous paper context and surface;
	if(paperSurface != NULL)
	{
	    cairo_surface_write_to_png (paperSurface,"/tmp/plotterPaper.png");
	    cairo_surface_destroy(paperSurface);

	}
	if(paperSurfaceCr != NULL)
	{
	    cairo_destroy(paperSurfaceCr);	    
	}

	cairo_set_source_surface(visibleSurfaceCr, cairo_get_target(drumSurfaceCr), 0.0,0.0);
	cairo_rectangle(visibleSurfaceCr, 0.0,0.0, DRUM_WIDE, DRUM_HIGH+LINES_VISIBLE);
	cairo_fill(visibleSurfaceCr);

	PaperLoaded = FALSE;

    }
#endif
}

static void stickyTapeHandler(__attribute__((unused)) int state)
{
    //holdingStikyTape = TRUE;
    ConfigureLeftHandNew(0.0,0.0,0,HAND_STICKY_TAPE);

}


static void powerKnobHandler(int state)
{
    g_info("state = %d\n",state);
    PowerSwitchOn = (state == 0);
    // Use V24On as that is powered from the same 
    PlotterPowerOn = PowerSwitchOn && V24On;
    if(!PlotterPowerOn)
	PenDown = TRUE;
    plotterMoved = TRUE;  // Force a redraw
}

static void manualHandler(int state)
{
    g_info("state = %d\n",state);
    PlotterManual = !(state == 0) ;
}
static void carriageSingleKnobHandler(int state)
{
    if(PlotterPowerOn)
    {
	PenX += state -1;
	limitX();
	if(PenDown)
	{
	    plotPoint();
	}
	plotterMoved = TRUE;
    }
}
static void CarraigeFastKnobHandler(int state)
{
    carriageFastMove = state -1;
}
static void drumSingleKnobHandler(int state)
{
    if(PlotterPowerOn)
    { 
	PenY += state -1;
	wrapY();
	if(PenDown)
	{
	    plotPoint();
	}
	plotterMoved = TRUE;
    }
}

static void DrumFastKnobHandler(int state)
{
    drumFastMove = state -1;
}
static void penUpDownHandler(int state)
{
    if(PlotterPowerOn)
    {
	if(state == 2) PenDown = FALSE;
	else if(state == 0) PenDown = TRUE;
    }
}


static const char *knobPngFileNames[] =
{
    "CalcompDownLeft.png","CalcompLeft.png","CalcompUpLeft.png",
    "CalcompUp.png",
    "CalcompUpRight.png","CalcompDownRight.png",
    "CalcompManualInOff.png","CalcompManualInOn.png","CalcompManualOut.png"
};

static const char *manualPngFileNames[] =
{
    "CalcompManualOut.png","CalcompManualInOff.png","CalcompManualInOn.png"
};


void PlotterTidy(GString *userPath)
{
    GString *configText;
    int windowXpos,windowYpos;
    GString *drumFileName;
    
    drumFileName = g_string_new(userPath->str);

    g_string_printf(drumFileName,"%s%s",userPath->str,"Drum.png");
    
    cairo_surface_write_to_png (cairo_get_target(drumSurfaceCr),drumFileName->str);

    if(paperSurfaceCr != NULL)
    {
	// Eventually will need to save the paper position and size in a config file.
	g_string_printf(drumFileName,"%s%s",userPath->str,"Paper.png");


	cairo_surface_write_to_png (cairo_get_target(paperSurfaceCr),drumFileName->str);
    }
   

 
    g_string_printf(drumFileName,"%s%s",userPath->str,"Visible.png");
    
    cairo_surface_write_to_png (cairo_get_target(visibleSurfaceCr),drumFileName->str);

    g_string_free(drumFileName,TRUE);

    configText = g_string_new("# Plotter  configuration\n");

    gtk_window_get_position(GTK_WINDOW(PlotterWindow), &windowXpos, &windowYpos);
    g_string_append_printf(configText,"WindowPosition %d %d\n",windowXpos,windowYpos);
    if(PaperLoaded)
    {
	g_string_append_printf(configText,"PaperInfo %d %d %d %d\n",
			       PaperArea.x,PaperArea.y,PaperArea.width,PaperArea.height);
    }
    
    // Should probably save them all, but power and manual are the important ones 
    g_string_append_printf(configText,"ManualButton %d\n",PlotterManual ? 1 : 0);
    g_string_append_printf(configText,"PowerKnob %d\n",PowerSwitchOn ? 1 : 0);

    updateConfigFile("PlotterState",userPath,configText);

    g_string_free(configText,TRUE);


    gtk_widget_destroy(PlotterWindow);



}



static int savedWindowPositionHandler(int nn)
{
    int windowXpos,windowYpos;
    windowXpos = atoi(getField(nn+1));
    windowYpos = atoi(getField(nn+2));
    gtk_window_move(GTK_WINDOW(PlotterWindow),windowXpos,windowYpos);
    return TRUE;
}

static int savedPaperInfoHandler(int nn)
{
    PaperArea.x = atoi(getField(nn+1));
    PaperArea.y = atoi(getField(nn+2));
    PaperArea.width = atoi(getField(nn+3));
    PaperArea.height = atoi(getField(nn+4));
    PaperLoaded = TRUE;
    return TRUE;
}



static int savedManualButtonHandler(int nn)
{
    int n;
    n = atoi(getField(nn+1));
    g_info("n=%d\n",n);
    PlotterManual = (n == 1) ? TRUE : FALSE;
    knobs[manualKnobNumber].state  = (n == 0) ? 0 : 1;
    return TRUE;
}

static int savedPowerKnobHandler(int nn)
{
    int n;
    n = atoi(getField(nn+1));
    g_info("n=%d\n",n);
    PowerSwitchOn = (n == 1) ? TRUE : FALSE;
    knobs[powerKnobNumber].state  = (n == 1) ? 0 : 2;
    knobs[powerKnobNumber].changed = TRUE;
    return TRUE;
}


static Token savedStateTokens[] = {
    {"WindowPosition",0,savedWindowPositionHandler},
    {"PaperInfo",0,savedPaperInfoHandler},
    {"ManualButton",0,savedManualButtonHandler},
    {"PowerKnob",0,savedPowerKnobHandler},
    {NULL,0,NULL}
};




#define KNOB_WIDE 32
#define KNOB_HIGH 32

void PlotterInit( __attribute__((unused)) GtkBuilder *builder,
	      __attribute__((unused))  GString *sharedPath,
	      __attribute__((unused))  GString *userPath)
{
    GString *fileName = NULL;
    int width,height;
    int n,knobNumber;
    GError *error = NULL;
    struct knobInfo *knob;
    GdkRectangle *switchArea;
    
    PlotterWindow = GTK_WIDGET(gtk_builder_get_object_checked (builder, "PlotterWindow"));
    PlotterDrawingArea = GTK_WIDGET(gtk_builder_get_object_checked(builder,"PlotterDrawingArea"));


    fileName = g_string_new(NULL);
    g_string_printf(fileName,"%sgraphics/calcomp.png",sharedPath->str);
    
    background_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
    
    width = gdk_pixbuf_get_width(background_pixbuf);
    height = gdk_pixbuf_get_height(background_pixbuf) + 200;

    gtk_window_set_default_size(GTK_WINDOW(PlotterWindow), width, height);

    g_string_printf(fileName,"%sgraphics/carriage.png",sharedPath->str);
    
    carriage_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);

    // Try to restore plotter images from users directory
    // Use images from 803-Resources if not found.
    g_string_printf(fileName,"%sDrum.png",userPath->str);
    drum_pixbuf =
	gdk_pixbuf_new_from_file(fileName->str,NULL);
    
    if(drum_pixbuf == NULL)
    {
	g_string_printf(fileName,"%sgraphics/Drum.png",sharedPath->str);
	drum_pixbuf =
	    my_gdk_pixbuf_new_from_file(fileName->str);
    }



	
 

    
    
    g_string_printf(fileName,"%sVisible.png",userPath->str);
    visible_pixbuf =
	gdk_pixbuf_new_from_file(fileName->str,NULL);
    if(visible_pixbuf == NULL)
    {
	g_string_printf(fileName,"%sgraphics/Drum.png",sharedPath->str);
	visible_pixbuf =
	    my_gdk_pixbuf_new_from_file(fileName->str);
    }

    // Paper is loaded after the config file has been read.
    

    for(n=0;n<9;n++)
    {
	g_string_printf(fileName,"%sgraphics/%s",sharedPath->str,knobPngFileNames[n]);
	knobPixbufs[n] = gdk_pixbuf_new_from_file(fileName->str,&error);

	if(error != NULL)
	    g_error("Failed to read image file %s due to %s\n",fileName->str,error->message);
    }

    for(n=0;n<3;n++)
    {
	g_string_printf(fileName,"%sgraphics/%s",sharedPath->str,manualPngFileNames[n]);
	manualPixbufs[n] = gdk_pixbuf_new_from_file(fileName->str,&error);

	if(error != NULL)
	    g_error("Failed to read image file %s due to %s\n",fileName->str,error->message);
    }

    

    knobNumber = 0;

/* POWER knob*/
    knob = &knobs[knobNumber];
    powerKnobNumber = knobNumber;

    knob->type = 1;
    knob->xpos = 0;
    knob->ypos = 253;
    knob->width = KNOB_WIDE;
    knob->height = KNOB_HIGH;
    knob->state = 0;
    knob->pixIds[0] = 4;  knob->pixIds[1] = -1;  knob->pixIds[2] = 5;
    knob->changed = FALSE;
    knob->wire = 0;
    knob->handler = powerKnobHandler;

    switchArea = &OneFingerAreas[knobNumber];
    switchArea->x = knob->xpos;
    switchArea->y = knob->ypos;	
    switchArea->width = knob->width;
    switchArea->height = knob->height;

    knobNumber += 1;

/* Carriage Fast knob */

    knob = &knobs[knobNumber];
    
    knob->type = 0;
    knob->xpos = 0;
    knob->ypos = 155;
    knob->width = KNOB_WIDE;
    knob->height = KNOB_HIGH;
    knob->state = 1;
    knob->pixIds[0] = 2;  knob->pixIds[1] = 3;  knob->pixIds[2] = 4;
    knob->changed = FALSE;
    knob->wire = 0;

    switchArea = &OneFingerAreas[knobNumber];
    switchArea->x = knob->xpos;
    switchArea->y = knob->ypos;	
    switchArea->width = knob->width;
    switchArea->height = knob->height;
    knob->handler = CarraigeFastKnobHandler;

    knobNumber += 1;

/* Carriage Single Step knob */

    knob = &knobs[knobNumber];
    
    knob->type = 2;
    knob->xpos = 0;
    knob->ypos = 60;
    knob->width = KNOB_WIDE;
    knob->height = KNOB_HIGH;
    knob->state = 1;
    knob->pixIds[0] = 2;  knob->pixIds[1] = 3;  knob->pixIds[2] = 4;
    knob->changed = FALSE;
    knob->wire = 0;

    switchArea = &OneFingerAreas[knobNumber];
    switchArea->x = knob->xpos;
    switchArea->y = knob->ypos;	
    switchArea->width = knob->width;
    switchArea->height = knob->height;
    knob->handler = carriageSingleKnobHandler;

    knobNumber += 1;

/* Drum Single Step knob */

    knob = &knobs[knobNumber];
    
    knob->type = 2;
    knob->xpos = 661;
    knob->ypos = 60;
    knob->width = KNOB_WIDE;
    knob->height = KNOB_HIGH;
    knob->state = 1;
    knob->pixIds[0] = 0;  knob->pixIds[1] = 1;  knob->pixIds[2] = 2;
    knob->changed = FALSE;
    knob->wire = 0;

    switchArea = &OneFingerAreas[knobNumber];
    switchArea->x = knob->xpos;
    switchArea->y = knob->ypos;	
    switchArea->width = knob->width;
    switchArea->height = knob->height;
    knob->handler = drumSingleKnobHandler;

    knobNumber += 1;


/* Drum Fast knob */

    knob = &knobs[knobNumber];
    
    knob->type = 0;
    knob->xpos = 661;
    knob->ypos = 155;
    knob->width = KNOB_WIDE;
    knob->height = KNOB_HIGH;
    knob->state = 1;
    knob->pixIds[0] = 0;  knob->pixIds[1] = 1;  knob->pixIds[2] = 2;
    knob->changed = FALSE;
    knob->wire = 0;

    switchArea = &OneFingerAreas[knobNumber];
    switchArea->x = knob->xpos;
    switchArea->y = knob->ypos;	
    switchArea->width = knob->width;
    switchArea->height = knob->height;
    knob->handler = DrumFastKnobHandler;
    knobNumber += 1;


/* Pen Up & Down */

    knob = &knobs[knobNumber];
    
    knob->type = 2;
    knob->xpos = 661;
    knob->ypos = 253;
    knob->width = KNOB_WIDE;
    knob->height = KNOB_HIGH;
    knob->state = 1;
    knob->pixIds[0] = 0;  knob->pixIds[1] = 1;  knob->pixIds[2] = 2;
    knob->changed = FALSE;
    knob->wire = 0;

    switchArea = &OneFingerAreas[knobNumber];
    switchArea->x = knob->xpos;
    switchArea->y = knob->ypos;	
    switchArea->width = knob->width;
    switchArea->height = knob->height;
    knob->handler = penUpDownHandler;
    
    knobNumber += 1;

    /* Manual */

    knob = &knobs[knobNumber];
    manualKnobNumber = knobNumber;
    
    knob->type = 3;    // Press-press Toggle switch
    knob->xpos = 285;
    knob->ypos = 372;
    knob->width = 48;
    knob->height = 35;
    knob->state = 0;
    knob->pixIds[0] = 0;  knob->pixIds[1] = 1;  knob->pixIds[2] = 2;
    knob->changed = FALSE;
    knob->wire = 0;

    switchArea = &OneFingerAreas[knobNumber];
    switchArea->x = knob->xpos;
    switchArea->y = knob->ypos;	
    switchArea->width = knob->width;
    switchArea->height = knob->height;
    knob->handler = manualHandler;
    
    knobNumber += 1;

    knobCount = knobNumber;

    /* Square Paper Area */

    knob = &knobs[knobNumber];

    switchArea = &OneFingerAreas[knobNumber];
    switchArea->x = 584;
    switchArea->y = 378;	
    switchArea->width = 90;
    switchArea->height = 90;
    knob->handler = squarePaperHandler;

    knobNumber += 1;

    knobCount2 = knobNumber;
     
    /* Sticky tape area  */
        
    knob = &knobs[knobNumber];
    stickyKnobNumber = knobNumber;
    
    switchArea = &OneFingerAreas[knobNumber];
    switchArea->x = 477;
    switchArea->y = 424;	
    switchArea->width = 38;
    switchArea->height = 29;
    knob->handler = stickyTapeHandler;

    knobNumber += 1;

    
   
    

    PenX = 100;
    PenY = (PAPER_TOP+PAPER_HIGH);    // 1660 puts pen at 830
    PenY = 0;
    PenDown = TRUE;


    
    
    connectWires(F72, F72changed);
    connectWires(ACT, ACTchanged);
    connectWires(CLINES,ClinesChanged);
    connectWires(TIMER100HZ,fastMovePen);
    connectWires(PTS24VOLTSON,V24changed);


    readConfigFile("PlotterState",userPath,savedStateTokens);

    // If paper size/position had been configured load the high res image.
    // THe low res version of the paper is already drawn in the visible surface.
    if(PaperLoaded)
    {
	cairo_surface_t *paperSurface = NULL;
	int w,h;
	
	g_string_printf(fileName,"%sPaper.png",userPath->str);
	paper_pixbuf =
	    gdk_pixbuf_new_from_file(fileName->str,NULL);


	// Check pixbuf is the right size.
	h = gdk_pixbuf_get_height(paper_pixbuf);
	w = gdk_pixbuf_get_width(paper_pixbuf); 

	if((h != 2*PaperArea.height) || (w != 2*PaperArea.width))
	{
	    g_error("Loaded paper pixbuf size is wrong. (%d,%d) should be (%d,%d)\n",
		    w,h,2*PaperArea.width,2*PaperArea.height);
	}

	// THis is the full resolution version so double the dimenstions
	paperSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						  2*PaperArea.width,2*PaperArea.height);

	paperSurfaceCr = cairo_create(paperSurface);
	gdk_cairo_set_source_pixbuf (paperSurfaceCr, paper_pixbuf ,0.0,0.0);
	
	cairo_paint(paperSurfaceCr);
    }
    

    g_string_free(fileName,TRUE);
    fileName = NULL;
    
    gtk_widget_show(PlotterWindow);





}

enum PlotterStates {PLOTTER_NO_PAPER,PLOTTER_HOLDING_BELOW,PLOTTER_HOLDING_ABOVE,PLOTTER_DROPPED,
		    PLOTTER_POSITIONING,PLOTTER_STICKY1,PLOTTER_BOTTOM_FIXED,PLOTTER_STICKY2,
		    PLOTTER_BOTH_FIXED,PLOTTER_TOP_FIXED,PLOTTER_STICKY3,PLOTTER_TOP_FREE,
		    PLOTTER_BOTTOM_FREE,PLOTTER_BOTH_FREE,PLOTTER_POSITIONING2,PLOTTER_PARER_REMOVED};

const char *PlotterStateNames[] =
    {"PLOTTER_NO_PAPER","PLOTTER_HOLDING_BELOW","PLOTTER_HOLDING_ABOVE","PLOTTER_DROPPED",
     "PLOTTER_POSITIONING","PLOTTER_STICKY1","PLOTTER_BOTTOM_FIXED","PLOTTER_STICKY2",
     "PLOTTER_BOTH_FIXED","PLOTTER_TOP_FIXED","PLOTTER_STICKY3","PLOTTER_TOP_FREE",
     "PLOTTER_BOTTOM_FREE","PLOTTER_BOTH_FREE","PLOTTER_POSITIONING2","PLOTTER_PARER_REMOVED"};


enum PlotterEvents {PLOTTER_PRESS_NEW_PAPER,PLOTTER_ABOVE_CARRIAGE,PLOTTER_BELOW_CARRIAGE,
		    PLOTTER_DROP_PAPER,PLOTTER_PAPER_PRESS,
		    PLOTTER_PAPER_RELEASE,PLOTTER_PRESS_STICKY,PLOTTER_PRESS_BOTTOM_EDGE,PLOTTER_PRESS_TOP_EDGE,
		    PLOTTER_PRESS_CORNER};

const char *PlotterEvetNames[] =
{"PLOTTER_PRESS_NEW_PAPER","PLOTTER_ABOVE_CARRIAGE","PLOTTER_BELOW_CARRIAGE",
 "PLOTTER_DROP_PAPER","PLOTTER_PAPER_PRESS",
 "PLOTTER_PAPER_RELEASE","PLOTTER_PRESS_STICKY","PLOTTER_PRESS_BOTTOM_EDGE","PLOTTER_PRESS_TOP_EDGE",
 "PLOTTER_PRESS_CORNER"};



struct fsmtable PlotterPaperTable[] = {

    {PLOTTER_NO_PAPER,         PLOTTER_PRESS_NEW_PAPER,    PLOTTER_HOLDING_BELOW,     NULL},

    {PLOTTER_HOLDING_BELOW,    PLOTTER_ABOVE_CARRIAGE,     PLOTTER_HOLDING_ABOVE,     NULL},

    {PLOTTER_HOLDING_ABOVE,    PLOTTER_BELOW_CARRIAGE,     PLOTTER_HOLDING_BELOW,     NULL},
    {PLOTTER_HOLDING_ABOVE,    PLOTTER_DROP_PAPER,         PLOTTER_DROPPED,           NULL},
    
    {PLOTTER_DROPPED,          PLOTTER_PAPER_PRESS,        PLOTTER_POSITIONING,       NULL},
    {PLOTTER_DROPPED,          PLOTTER_PRESS_STICKY,       PLOTTER_STICKY1,           NULL},
    
    {PLOTTER_POSITIONING,      PLOTTER_PAPER_RELEASE,      PLOTTER_DROPPED,           NULL},

    {PLOTTER_STICKY1,          PLOTTER_PRESS_BOTTOM_EDGE,  PLOTTER_BOTTOM_FIXED,      NULL},
    {PLOTTER_STICKY1,          PLOTTER_PRESS_TOP_EDGE},    PLOTTER_TOP_FIXED,         NULL},

    {PLOTTER_BOTTOM_FIXED,     PLOTTER_PRESS_STICKY,       PLOTTER_STICKY2,           NULL},

    {PLOTTER_STICKY2,          PLOTTER_PRESS_TOP_EDGE,     PLOTTER_BOTH_FIXED,        NULL},

    {PLOTTER_BOTH_FIXED,       PLOTTER_PRESS_BOTTOM_EDGE,  PLOTTER_BOTTOM_FREE,       NULL},
    {PLOTTER_BOTH_FIXED,       PLOTTER_PRESS_TOP_EDGE,     PLOTTER_TOP_FREE,          NULL},
    
    {PLOTTER_TOP_FIXED,        PLOTTER_PRESS_STICKY,       PLOTTER_STICKY3,           NULL},

    {PLOTTER_STICKY3,          PLOTTER_PRESS_BOTTOM_EDGE,  PLOTTER_BOTH_FIXED,        NULL},

    {PLOTTER_TOP_FREE,         PLOTTER_PRESS_BOTTOM_EDGE,  PLOTTER_BOTH_FREE,         NULL},

    {PLOTTER_BOTTOM_FREE,      PLOTTER_PRESS_TOP_EDGE,     PLOTTER_BOTH_FREE,         NULL},

    {PLOTTER_BOTH_FREE,        PLOTTER_PAPER_PRESS,        PLOTTER_POSITIONING2,      NULL},
    {PLOTTER_BOTH_FREE,        PLOTTER_PRESS_CORNER,       PLOTTER_PARER_REMOVED,     NULL,

    {PLOTTER_POSITIONING2,     PLOTTER_DROP_PAPER,         PLOTTER_BOTH_FREE,         NULL},

    {PLOTTER_PARER_REMOVED,    PLOTTER_PRESS_NEW_PAPER,    PLOTTER_NO_PAPER,          NULL},

    {-1,-1,-1,NULL}
};

    

     
