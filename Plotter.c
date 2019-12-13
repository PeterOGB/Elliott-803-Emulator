#define G_LOG_USE_STRUCTURED

#include <gtk/gtk.h>
#include <math.h>

#include "Logging.h"
#include "Plotter.h"
#include "Wiring.h"
#include "Hands.h"
#include "Common.h"
#include "fsm.h"



typedef struct _myRectangle {
    gdouble x,y,width,height;
    gdouble topLine;
} Rectangle;


typedef struct _visibleArea {
    gdouble left,top,right,bottom;
    gboolean wrapped;
} VisibleArea;

// Ensure that a is in the range 0.0 <= a < b
// Typically used as wrapRange(topLine,DRUM_HIGH);

#define wrapRange(a,b) {while(a<0.0)a+=b;while(a>=b)a-=b;}

extern int CPU_word_time_count;

static GtkWidget *PlotterWindow = NULL;
GtkWidget *PlotterDrawingArea = NULL;
static GdkPixbuf *background_pixbuf = NULL;

static GdkPixbuf *knobPixbufs[9];
static GdkPixbuf *carriage_pixbuf;
static GdkPixbuf *drum_pixbuf;
static GdkPixbuf *paper_pixbuf;
static GdkPixbuf *sticky_pixbuf;
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
//static gdouble LeftHandExitedAtX,LeftHandExitedAtY;
//static gdouble RightHandExitedAtX,RightHandExitedAtY;
static gdouble FingerPressedAtX = 0,FingerPressedAtY = 0;
static gboolean warpToLeftHand = FALSE;
static gboolean warpToRightHand = FALSE;
static gboolean InPlotterWindow = FALSE;
static gboolean deferedMotion = FALSE;
static gdouble deferedMotionX,deferedMotionY;
static GList *pressedKeys = NULL;
static GdkSeat *seat = NULL;

static Rectangle OneFingerAreas[20];
// Not static so that DrawHandsNew can read it.
HandInfo *handHoldingPaper = NULL;
//static int wrappingState  = 1;
static gboolean topSticky = FALSE;
static Rectangle topStickyArea = {0.0,0.0,0.0,0.0,0.0};
static VisibleArea topStickyVisibleArea = {0.0,0.0,0.0,0.0,FALSE};

static gboolean bottomSticky = FALSE;
static Rectangle bottomStickyArea = {0.0,0.0,0.0,0.0,0.0}; 
static VisibleArea bottomStickyVisibleArea = {0.0,0.0,0.0,0.0,FALSE};

//static gdouble WrappedPaper = 0.0;
//static gdouble wrappedPaper = 0.0;
static gdouble unwrappedPaper = 0.0;
static gdouble attachingLine = 0.0;
static GdkPixbuf *paperSmall_pixbuf;

static int paperSize = 0;
static gdouble initialDrop = 0.0;
static gdouble paperBottom,paperTop;
static gdouble stickyBottom,stickyTop;

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

// penx and peny are full resolution but half sized so steps will be
// +/- 0.5 
static gdouble penx,peny;    // For half resolution image
//static gdouble PenX,PenY;    // For full resolution image (not needed anymore) 
static gboolean PenDown = TRUE;
static cairo_t *visibleSurfaceCr = NULL;
static Rectangle PaperArea = {0.0,0.0,0.0,0.0,0.0};
static VisibleArea PaperVisibleArea = {0.0,0.0,0.0,0.0,FALSE};
static gboolean PaperLoaded = FALSE;
//static gboolean PaperPositioning = FALSE;
//static gboolean PaperMoving = FALSE;
static gdouble  PaperMovedByX = 0.0;
static gdouble  PaperMovedByY = 0.0;
static cairo_t *paperSurfaceCr = NULL;
static cairo_t *drumSurfaceCr = NULL;
static int wrappingFsmState = 0;
static int showing = 0;


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

static
void on_Hand_motion_event(HandInfo *MovingHand);

static 
void warpToFinger(GdkWindow *win,HandInfo *hand);

static
void wrappingPaper(gdouble direction);

static
void showArea2(cairo_t *cr, GdkPixbuf *pixbuf,
	       gdouble atX, gdouble atY,
	       gdouble visibleLeft,gdouble visibleTop,
	       gdouble n);


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





enum PlotterStates {PLOTTER_NO_PAPER,PLOTTER_HOLDING_PAPER,PLOTTER_PLACED_PAPER,
		    PLOTTER_BOTTOM_FIXED,
		    PLOTTER_BOTH_FIXED,PLOTTER_TOP_FIXED,PLOTTER_STICKY3,PLOTTER_TOP_FREE,
		    PLOTTER_BOTTOM_FREE,PLOTTER_BOTH_FREE,PLOTTER_POSITIONING2,PLOTTER_PAPER_REMOVED};

const char *PlotterStateNames[] =
    {"PLOTTER_NO_PAPER","PLOTTER_HOLDING_PAPER","PLOTTER_PLACED_PAPER",
     "PLOTTER_BOTTOM_FIXED",
     "PLOTTER_BOTH_FIXED","PLOTTER_TOP_FIXED","PLOTTER_STICKY3","PLOTTER_TOP_FREE",
     "PLOTTER_BOTTOM_FREE","PLOTTER_BOTH_FREE","PLOTTER_POSITIONING2","PLOTTER_PARER_REMOVED"};


enum PlotterEvents {PLOTTER_PRESS_NEW_PAPER,PLOTTER_PLACE_PAPER,PLOTTER_PAPER_PRESS,
		    PLOTTER_FIX_BOTTOM_EDGE,PLOTTER_FIX_TOP_EDGE,
		    PLOTTER_PRESS_BOTTOM_STICKY,PLOTTER_PRESS_TOP_STICKY,
		    PLOTTER_PICKUP_PAPER,PLOTTER_DROP_PAPER};

const char *PlotterEventNames[] =
{"PLOTTER_PRESS_NEW_PAPER", "PLOTTER_PLACE_PAPER","PLOTTER_PAPER_PRESS",
 "PLOTTER_FIX_BOTTOM_EDGE","PLOTTER_FIX_TOP_EDGE",
 "PLOTTER_PRESS_BOTTOM_EDGE","PLOTTER_PRESS_TOP_EDGE",
 "PLOTTER_PICKUP_PAPER","PLOTTER_DROP_PAPER"};



struct fsmtable PlotterPaperTable[] = {

    {PLOTTER_NO_PAPER,         PLOTTER_PRESS_NEW_PAPER,    PLOTTER_HOLDING_PAPER,     NULL},

    {PLOTTER_HOLDING_PAPER,    PLOTTER_PLACE_PAPER,        PLOTTER_PLACED_PAPER,      NULL},
    {PLOTTER_HOLDING_PAPER,    PLOTTER_DROP_PAPER,         PLOTTER_NO_PAPER,          NULL},

    {PLOTTER_PLACED_PAPER,     PLOTTER_PICKUP_PAPER,       PLOTTER_HOLDING_PAPER,     NULL},
    {PLOTTER_PLACED_PAPER,     PLOTTER_FIX_BOTTOM_EDGE,    PLOTTER_BOTTOM_FIXED,      NULL},

    {PLOTTER_BOTTOM_FIXED,     PLOTTER_FIX_TOP_EDGE,       PLOTTER_BOTH_FIXED,        NULL},

    {PLOTTER_BOTH_FIXED,       PLOTTER_PRESS_BOTTOM_STICKY,PLOTTER_BOTTOM_FREE,       NULL},
    {PLOTTER_BOTH_FIXED,       PLOTTER_PRESS_TOP_STICKY,   PLOTTER_TOP_FREE,          NULL},

    {PLOTTER_BOTTOM_FREE,      PLOTTER_PRESS_TOP_STICKY,   PLOTTER_PLACED_PAPER,      NULL},
    {PLOTTER_TOP_FREE,         PLOTTER_PRESS_BOTTOM_STICKY,PLOTTER_PLACED_PAPER,      NULL},
    
    
    {-1,-1,-1,NULL}
};



struct fsm PlotterPaperFSM = { "Plotter FSM",0, PlotterPaperTable ,
			 PlotterStateNames,PlotterEventNames,1,-1};

     





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
	ConfigureRightHandNew(400.0,250.0,SET_TARGETXY|SET_RESTINGXY|SET_FINGERXY,HAND_NO_CHANGE);
	ConfigureLeftHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_NO_CHANGE);
	/*
	if(HandIsEmpty(LeftHand))
	{
	    ConfigureLeftHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_EMPTY);
	}
	else
	{
	    ConfigureLeftHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_HOLDING_REEL);
	}
	*/
	setLeftHandMode (TRACKING_HAND);
	setRightHandMode(IDLE_HAND);
	
    }
    else
    {
	ConfigureLeftHandNew (100.0,250.0,SET_TARGETXY|SET_RESTINGXY|SET_FINGERXY,HAND_NO_CHANGE);
	ConfigureRightHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_NO_CHANGE);
	/*
	if(HandIsEmpty(RightHand))
	{
	    ConfigureRightHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_EMPTY);
	}
	else
	{
	    ConfigureRightHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_HOLDING_REEL);
	}
	*/
	setRightHandMode (TRACKING_HAND);
	setLeftHandMode(IDLE_HAND);
	
    }
    SetActiveWindow(PLOTTERWINDOW);

    
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
    

    //getTrackingXY2(&LeftHandInfo,&LeftHandExitedAtX,&LeftHandExitedAtY);
    //getTrackingXY2(&RightHandInfo,&RightHandExitedAtX,&RightHandExitedAtY);

    //printf("%s TIME: %"PRIu32" %.1f %.1f \n",__FUNCTION__,exitTimeStamp,wep->eventX,wep->eventY);
    
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

#if 0
static int
PlotterWindowUnconstrainedHandler(__attribute__((unused)) int s,
				   __attribute__((unused)) int e,
				   void *p)
{
    struct WindowEvent *wep  = (struct WindowEvent *) p;

    warpToFinger(wep->window,(HandInfo *)wep->data);
    

    return(-1);
}
#endif

static struct fsmtable PlotterWindowTable[] = {
    {OUTSIDE_WINDOW,           FSM_ENTER,           INSIDE_WINDOW,            PlotterWindowEnterHandler},
    
    {INSIDE_WINDOW,            FSM_LEAVE,           OUTSIDE_WINDOW,           PlotterWinowLeaveHandler},
//    {INSIDE_WINDOW,            FSM_CONSTRAINED,     CONSTRAINED_INSIDE,       NULL},
    
//    {CONSTRAINED_INSIDE,       FSM_UNCONSTRAINED,   INSIDE_WINDOW,            PlotterWindowUnconstrainedHandler},

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



void showArea2(cairo_t *cr, GdkPixbuf *pixbuf,
	      gdouble atX, gdouble atY,
	      gdouble visibleLeft,gdouble visibleTop,
	      
	      gdouble n)
{
    gdouble top,bot;
    cairo_surface_t *surface = cairo_get_target(cr);
    gdouble visibleHeight = cairo_image_surface_get_height(surface);
    gdouble itemWidth = gdk_pixbuf_get_width(pixbuf);
    gdouble itemHeight = gdk_pixbuf_get_height(pixbuf);

    atY -= visibleTop;
    atX -= visibleLeft;
    
    top = atY + n ;
    bot = itemHeight + top;

    wrapRange(bot,visibleHeight);
    wrapRange(top,visibleHeight);


    g_debug("top=%.1f bot=%.1f\n",top,bot);

    if(bot < top)
    {
	//gdk_cairo_set_source_pixbuf (cr, pixbuf , atX,top);
	cairo_set_source_rgba(cr,1.0,0.0,0.0,1.0);
	cairo_rectangle(cr, atX,top,itemWidth,visibleHeight-top);
	cairo_fill(cr);
	    
	//gdk_cairo_set_source_pixbuf (cr, pixbuf , atX,top-visibleHeight);
	cairo_set_source_rgba(cr,0.0,1.0,0.0,1.0);
	cairo_rectangle(cr, atX,0.0,itemWidth,bot);
	cairo_fill(cr);
    }
    else
    {
//	gdk_cairo_set_source_pixbuf (cr, pixbuf , atX,top);
	cairo_set_source_rgba(cr,0.0,0.0,1.0,1.0);
	cairo_rectangle(cr, atX,top,itemWidth,itemHeight);
	cairo_fill(cr);
    }

}



// These are all in pixels which is half resolution
#define LINES_VISIBLE 330.0
#define MIDDLE_LINE 165.0
#define DRAWABLE_DRUM_WIDE 550.0
#define DRUM_WIDE 616.0
#define DRUM_HIGH 940.0
#define DRAWABLE_DRUM_LEFT 72.0
#define DRUM_LEFT 39.0
#define DRUM_TOP 21.0


#define STICKY_HIGH 20.0
//#define INITIAL_WRAPPED 25.0


//static Rectangle squarePaperSize = { PAPER_LEFT,PAPER_TOP,PAPER_WIDE,PAPER_HIGH};
static cairo_surface_t *drumSurface = NULL;
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
    //static cairo_surface_t *drumSurface = NULL;
     static cairo_t *carriageSurfaceCr = NULL;
    //static cairo_t *drumSurfaceCr = NULL;
    GtkAllocation  DrawingAreaAlloc;
    struct knobInfo *knob;
    gdouble topLine;
    //gdouble wrappedWrappedPaper = wrappedPaper;
    //gdouble wrappedUnwrappedPaper = unwrappedPaper;

    if(firstCall)
    {
	firstCall = FALSE;

	
	//cairo_set_operator (cr,CAIRO_OPERATOR_SOURCE);
	
	// Create surface for the carriage 
	carriageSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						     64,64);
	carriageSurfaceCr = cairo_create(carriageSurface);
	cairo_set_operator (carriageSurfaceCr,CAIRO_OPERATOR_SOURCE);
	gdk_cairo_set_source_pixbuf (carriageSurfaceCr, carriage_pixbuf ,0.0,0.0);
	cairo_paint(carriageSurfaceCr);

	// Create a surface and an associated context for whole window
	gtk_widget_get_allocation(drawingArea, &DrawingAreaAlloc);
	
	backgroundSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			   DrawingAreaAlloc.width,DrawingAreaAlloc.height);
	backgroundSurfaceCr = cairo_create(backgroundSurface);
	//cairo_set_operator (backgroundSurfaceCr,CAIRO_OPERATOR_SOURCE);

	gdk_cairo_set_source_pixbuf (backgroundSurfaceCr, background_pixbuf ,0.0,0.0);
	cairo_paint(backgroundSurfaceCr);


	// Create surface to be shown
	// THis is the size of the visible image loaded from .png file
	visibleSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						    (int) DRAWABLE_DRUM_WIDE,
						    (int) DRUM_HIGH); //560,940);
	//DRAWABLE_DRUM_WIDE,DRUM_HIGH);
	visibleSurfaceCr = cairo_create(visibleSurface);
	// Set operator and antialias to that exact drawing to pixels works
	cairo_set_operator (visibleSurfaceCr,CAIRO_OPERATOR_SOURCE);
	//cairo_set_antialias (visibleSurfaceCr,CAIRO_ANTIALIAS_NONE);

	// Initialise it to the drum image.
	if(visible_pixbuf != NULL)
	    gdk_cairo_set_source_pixbuf (visibleSurfaceCr, visible_pixbuf ,0.0,0.0);
	
	cairo_paint(visibleSurfaceCr);


	// Create surface for things drawn on the drum !
	drumSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						 (int) DRAWABLE_DRUM_WIDE,
						 (int) DRUM_HIGH);
	drumSurfaceCr = cairo_create(drumSurface);

	// Initialise it to the drum image.
	gdk_cairo_set_source_pixbuf (drumSurfaceCr, drum_pixbuf ,0.0,0.0);
	cairo_paint(drumSurfaceCr);



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
	state = (PlotterManual) ? 1 : 0;
	if(PlotterManual && V24On) state = 2; // Light it up
	gdk_cairo_set_source_pixbuf (backgroundSurfaceCr,
				     manualPixbufs[state],
				     285.0,372.0);
	cairo_paint (backgroundSurfaceCr);
    }

    
    // Render the background surface
    cairo_set_source_surface (cr, backgroundSurface ,0.0,0.0);
    cairo_paint(cr);

    
    topLine = peny - MIDDLE_LINE;
    if(topLine < 0) topLine = DRUM_HIGH + topLine;


    // Draw the part of the drum surface visible in the window
   
    cairo_set_source_surface (cr, visibleSurface ,DRAWABLE_DRUM_LEFT,DRUM_TOP-topLine);
    {
	// THis makes the surface wrap so no need to keep duplicate area
	// at the bottom anymore :-)
	cairo_pattern_t *pattern;
	pattern = cairo_get_source (cr);

	cairo_pattern_set_extend (pattern,CAIRO_EXTEND_REPEAT);
    }
	
    cairo_rectangle(cr, DRAWABLE_DRUM_LEFT,DRUM_TOP,DRAWABLE_DRUM_WIDE, LINES_VISIBLE);
    //cairo_stroke_preserve(cr);
    cairo_fill(cr);


    // Draw paper resting on the drum before it is fixed
    if(PlotterPaperFSM.state == PLOTTER_PLACED_PAPER)
    {
	gdouble y;
	//y = -PaperArea.height + PaperArea.y;
	y = PaperArea.y;
	gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,y);
	cairo_rectangle(cr,PaperArea.x,y,PaperArea.width,PaperArea.height);
	cairo_fill(cr);

    }


    //wrapRange(wrappedWrappedPaper,DRUM_HIGH);
    //wrapRange(wrappedUnwrappedPaper,DRUM_HIGH);


    // Draw paper controlled by new state based wrapping algorithm

    if(PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED)
    {   // Clipping is set up so that bottom sticky is drawn correctly at the top and bottom
	cairo_save(cr);

	cairo_rectangle(cr,PaperArea.x,0.0,PaperArea.width,LINES_VISIBLE+DRUM_TOP);
	cairo_clip(cr);
	
	switch(showing)
	{

	case 1:
	    // Draw from top of paper to bottom of paper.
	    gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,paperTop+DRUM_TOP);
	    cairo_rectangle(cr,PaperArea.x,paperTop+DRUM_TOP,PaperArea.width,PaperArea.height);
	    cairo_fill(cr);
	    break;


	case 2:
	    //  Draw from the top of the paperdown to the bottom edge of the drum
	    gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,paperTop+DRUM_TOP);
	    cairo_rectangle(cr,PaperArea.x,paperTop+DRUM_TOP,PaperArea.width,
			    LINES_VISIBLE-paperTop);
	    cairo_fill(cr);
	    break;

	case 3:
	    // No Paper to draw
	    
	    break;
	case 4:
	    //g_debug("4: paperBottom-DRUM_TOP=%.1f\n",paperBottom-DRUM_TOP);
	    // Draw from the top of drum down to the bottom edge of the paper
	    gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,-PaperArea.height+(paperBottom+DRUM_TOP));
	    cairo_rectangle(cr,PaperArea.x,DRUM_TOP,PaperArea.width,paperBottom);
	    cairo_fill(cr);
	    break;
	case 5:
	    // Draw from top of paper (between P1 and P2) to bottom of paper.
	    gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,DRUM_TOP-(DRUM_HIGH-paperTop));
	    cairo_rectangle(cr,PaperArea.x,DRUM_TOP-(DRUM_HIGH-paperTop),PaperArea.width,PaperArea.height);
	    cairo_fill(cr);
	    break;
	case 6:
	    // Draw from the top of window down to the bottom edge of the paper
	    gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,-PaperArea.height+paperBottom+DRUM_TOP);
	    cairo_rectangle(cr,PaperArea.x,0.0,PaperArea.width,paperBottom+DRUM_TOP);
	    cairo_fill(cr);
	    break;
	case 7:
	    //  Draw from the top of the paper (Between P1 and P2) down to the bottom edge of the drum
	    // There seems to ba fence post error here when paperTop is  0.0 it needs tp be DRUM_HIGH
	    g_debug("%.1f %.1f %.1f\n",DRUM_TOP-(DRUM_HIGH-paperTop),LINES_VISIBLE+(DRUM_HIGH-paperTop),paperTop);
	    {
		gdouble pt;
		pt = paperTop <= 0.0 ? DRUM_HIGH:paperTop;
		gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,DRUM_TOP-(DRUM_HIGH-pt));
		cairo_rectangle(cr,PaperArea.x,DRUM_TOP-(DRUM_HIGH-pt),PaperArea.width,
				LINES_VISIBLE+(DRUM_HIGH-pt));
	    }
	    
	    //gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,DRUM_TOP-(DRUM_HIGH-paperTop));
	    //cairo_rectangle(cr,PaperArea.x,DRUM_TOP-(DRUM_HIGH-paperTop),PaperArea.width,
	    //		    LINES_VISIBLE+(DRUM_HIGH-paperTop));
	    cairo_fill(cr);
	    break;
	case 8:
	    //  Draw from the top of drum down to the bottom of the drum
	    gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,-PaperArea.height+(paperBottom+DRUM_TOP));
	    cairo_rectangle(cr,PaperArea.x,DRUM_TOP,PaperArea.width,LINES_VISIBLE);
	    cairo_fill(cr);
	    break;
	case 9:
	    //  Draw from the top of window down to the bottom of the drum
	    gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,-PaperArea.height+paperBottom+DRUM_TOP);
	    cairo_rectangle(cr,PaperArea.x,0.0,PaperArea.width,LINES_VISIBLE+DRUM_TOP);
	    cairo_fill(cr);

	    break;
	case 10:
	    //  Draw from the top of the paperdown to the bottom edge of the drum
	    gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,paperTop+DRUM_TOP);
	    cairo_rectangle(cr,PaperArea.x,paperTop+DRUM_TOP,PaperArea.width,
			    LINES_VISIBLE-paperTop);
	    cairo_fill(cr);

	    // Draw from the top of drum down to the bottom edge of the paper
	    gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,-PaperArea.height+(paperBottom+DRUM_TOP));
	    cairo_rectangle(cr,PaperArea.x,DRUM_TOP,PaperArea.width,paperBottom);
	    cairo_fill(cr);

	    break;


	default:
	    g_debug("Unknown showing=%d\n",showing);
	    break;
	    

	}

	// Now draw the bottom sticky (or parts of it as needed
	
	if( (stickyTop <= LINES_VISIBLE ) && (stickyBottom <= LINES_VISIBLE))
	{
	    //g_debug("SHOWING WHOLE STICKY\n");
	    // Show complete bottom sticky
	    gdk_cairo_set_source_pixbuf(cr,sticky_pixbuf,bottomStickyArea.x,stickyTop+DRUM_TOP);
	    cairo_paint(cr);


	}
	else if(stickyBottom <= STICKY_HIGH)
	{
	    //g_debug("SHOWING PART OF STICKY AT TOP\n");
	     // Draw from the top of drum down to the bottom edge of the sticky
	    gdk_cairo_set_source_pixbuf(cr,sticky_pixbuf,bottomStickyArea.x,-STICKY_HIGH+(stickyBottom+DRUM_TOP));
	    cairo_rectangle(cr,bottomStickyArea.x,DRUM_TOP,bottomStickyArea.width,stickyBottom);
	    cairo_fill(cr);

	}
	else if(stickyTop <= LINES_VISIBLE)
	{
	    //g_debug("SHOWING PART OF STICKY AT BOTTOM\n");
	    //  Draw from the top of the sticky down to the bottom edge of the drum
	    gdk_cairo_set_source_pixbuf(cr,sticky_pixbuf,bottomStickyArea.x,stickyTop+DRUM_TOP);
	    cairo_rectangle(cr,bottomStickyArea.x,stickyTop+DRUM_TOP,bottomStickyArea.width,
			    LINES_VISIBLE-stickyTop);
	    cairo_fill(cr);

	}    
	cairo_restore(cr);
    }






    
    

    // Not too sure about all the 0.5s  in here !
    // Draw carraige support bars
    {
	gdouble grey;
	cairo_set_line_width (cr, 1);
	for(int bar = 0; bar < 4; bar ++)
	{
	    grey = bar / 3.0;
	    grey = 1.0 - grey;
	    cairo_set_source_rgba(cr,grey,grey,grey,1.0);
	    cairo_move_to (cr,DRUM_LEFT+          +0.5,-25 + DRUM_TOP + MIDDLE_LINE + bar + 0.5);
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
    

    // Draw the carriage   (carriage is 64x64)
    cairo_set_source_surface(cr,carriageSurface,DRUM_LEFT+penx, DRUM_TOP+MIDDLE_LINE-32.0);
    cairo_rectangle(cr, DRUM_LEFT+penx,  DRUM_TOP+MIDDLE_LINE-32.0, 64.0, 64.0);
    cairo_fill(cr);



    // Draw paper in hand over the top of anythign else (except the hands).
    if(PlotterPaperFSM.state == PLOTTER_HOLDING_PAPER) 
    {
	gdouble x,y;
	// Draw the paper being held by the hand.
	getTrackingXY2(handHoldingPaper,&x,&y);
	
	if(handHoldingPaper == &LeftHandInfo)
	{
	   gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,x,y-PaperArea.height);
	    cairo_rectangle(cr,x,y,PaperArea.width,-PaperArea.height);
	}
	else
	{
	    gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,x-PaperArea.width,y-PaperArea.height);
	    cairo_rectangle(cr,x,y,-PaperArea.width,-PaperArea.height);
	}
	cairo_fill(cr);
    }

    if(InPlotterWindow)
	DrawHandsNew(cr); 
    return FALSE;
}

/*
A : PA.h  < ID
B : ID < PA.h < ID+DRUM_TOP
C : ID+DRUM_TOP < PA.h < VISILE_LINES
D: VISIBLE_LINES < PA.h < VISIBLE_LINES + DRUM_TOP
E: VISIBLE_LINES + DRUM_TOP < PA.h <  DRUM_HIGH-VISIBLE_LINES
F:  DRUM_HIGH-VISIBLE_LINES < PA.h
 */

// ID = Initial Drop    TP = Top Passing     BP = Bottom Passing 
enum WrappinEvents {ID,TP1,TP2,TP3,BP1,BP2,BP3};
static const char *WrappingEventNames[] =
{ "Initial Drop",
  "Top Passing 1","Top Passing 2","Top Passing 3",
  "Bottom Passing 1","Bottom Passing 2","Bottom Passing 3" };
  
    


struct wrapping {
    int state;
    int showing;
    int eventWrapping;
    int nextState;
    int eventUnwrapping;
    int prevState;
};


struct wrapping wrappingTableA[] =
{
    { 0,    0,    ID,     1,    0,    0,   },
    { 1,    1,    BP3,    2,    0,    0,   },
    { 2,    2,    TP3,    3,    BP3,  1,   },
    { 3,    3,    BP2,    4,    TP3,  2,   },
    { 4,    4,    TP2,    1,    BP2,  3,   },
    {-1,    0,    0,      0,    0,    0,   }
};

struct wrapping wrappingTableB[] =
{
    { 0,    0,    ID,     1,    0,    0,   },
    { 1,    5,    TP2,    2,    0,    0,   },
    { 2,    1,    BP3,    3,    TP2,  1,   },
    { 3,    2,    TP3,    4,    BP3,  2,   },
    { 4,    3,    BP2,    5,    TP3,  3,   },
    { 5,    4,    TP2,    2,    BP2,  4,   },
    {-1,    0,    0,      0,    0,    0,   }
};

struct wrapping wrappingTableC[] =
{
    { 0,    0,    ID,     1,    0,    0,   },
    { 1,    6,    TP1,    2,    0,    0,   },
    { 2,    5,    TP2,    3,    TP1,  1,   },
    { 3,    1,    BP3,    4,    TP2,  2,   },
    { 4,    2,    TP3,    5,    BP3,  3,   },
    { 5,    3,    BP2,    6,    TP3,  4,   },
    { 6,    4,    TP2,    3,    BP2,  5,   },
    {-1,    0,    0,      0,    0,    0,   }
};

struct wrapping wrappingTableD[] =
{
    { 0,    0,    ID,     1,    0,    0,   },
    { 1,    6,    TP1,    2,    0,    0,   },
    { 2,    5,    BP3,    3,    TP1,  1,   },
    { 3,    7,    TP2,    4,    BP3,  2,   },
    { 4,    2,    TP3,    5,    TP2,  3,   },
    { 5,    3,    BP2,    6,    TP3,  4,   },
    { 6,    4,    BP3,    7,    BP2,  5,   },
    { 7,    8,    TP2,    4,    BP3,  6,   },
    {-1,    0,    0,      0,    0,    0,   }
};


struct wrapping wrappingTableE[] =
{
    { 0,    0,    ID,     1,    0,    0,   },
    { 1,    6,    BP3,    2,    0,    0,   },
    { 2,    9,    TP1,    3,    BP3,  1,   },
    { 3,    7,    TP2,    4,    TP1,  2,   },
    { 4,    2,    TP3,    5,    TP2,  3,   },
    { 5,    3,    BP2,    6,    TP3,  4,   },
    { 6,    4,    BP3,    7,    BP2,  5,   },
    { 7,    8,    TP2,    4,    BP3,  6,   },
    {-1,    0,    0,      0,    0,    0,   }
};


struct wrapping wrappingTableF[] =
{
    { 0,    0,    ID,     1,    0,    0,   },
    { 1,    6,    BP3,    2,    0,    0,   },
    { 2,    9,    TP1,    3,    BP3,  1,   },
    { 3,    7,    TP2,    4,    TP1,  2,   },
    { 4,    2,    BP2,    5,    TP2,  3,   },
    { 5,   10,    TP3,    6,    BP2,  4,   },
    { 6,    4,    BP3,    7,    TP3,  5,   },
    { 7,    8,    TP2,    4,    BP3,  6,   },
    {-1,    0,    0,      0,    0,    0,   }
};
static struct wrapping *wrappingFsm = NULL;

static 
void doWrappingFsm(int event,gboolean wrappingDirection )
{
    struct wrapping *fsm;
    gboolean found = FALSE;

    g_debug("event = %s direction = %s\n",WrappingEventNames[event],wrappingDirection?"true":"false");

    fsm = wrappingFsm;
    while(fsm->state != -1)
    {
	if(fsm->state == wrappingFsmState)
	{
	    if(wrappingDirection)
	    {
		if(fsm->eventWrapping == event)
		{

		    wrappingFsmState = fsm->nextState;
		    found = TRUE;
		    break;
		}
	    }
	    else
	    {
		if(fsm->eventUnwrapping == event)
		{
		    wrappingFsmState = fsm->prevState;
		    found = TRUE;
		    break;
		}
	    }
	}
	fsm++;
    }

    if(found)
    {
	fsm = wrappingFsm;
	while(fsm->state != -1)
	{
	    if(fsm->state == wrappingFsmState)
	    {
		showing = fsm->showing;
		break;
	    }
	    fsm++;
	}
    }
    g_debug("state = %d showing = %d\n",wrappingFsmState,showing);
}




// direction is now +/- 0.5    -ve when wrapping
static void wrappingPaperNew(gdouble direction)
{
    gboolean dirFlag;
    dirFlag = (direction < 0.0);
    //wrappedPaper -= direction;
    paperBottom -= direction;
    paperTop -= direction;
    wrapRange(paperTop,DRUM_HIGH);
    wrapRange(paperBottom,DRUM_HIGH);

    stickyBottom -= direction; 
    stickyTop  -= direction;
    wrapRange(stickyBottom,DRUM_HIGH);
    wrapRange(stickyTop,DRUM_HIGH);

    

    if(paperTop == (DRUM_HIGH-DRUM_TOP))
    {
	g_debug("Top Passing 1\n");
	doWrappingFsm(TP1,dirFlag);
	
    }
    else if(paperTop == 0.0)
    {
	g_debug("Top Passing 2 %.1f %.1f\n",paperTop,paperBottom);
	doWrappingFsm(TP2,dirFlag);
    }
    else if(paperTop == LINES_VISIBLE)
    {
	g_debug("Top Passing 3\n");
	doWrappingFsm(TP3,dirFlag);
    }
    if(paperBottom == (DRUM_HIGH-DRUM_TOP))
    {
	g_debug("Bottom Passing 1\n");
	doWrappingFsm(BP1,dirFlag);
    }
    else if(paperBottom == 0.0)
    {
	g_debug("Bottom Passing 2\n");
	doWrappingFsm(BP2,dirFlag);
    }
    else if(paperBottom == LINES_VISIBLE)
    {
	g_debug("Bottom Passing 3\n");
	doWrappingFsm(BP3,dirFlag);
    }


}




#if 1
// direction is now +/- 0.5
static void wrappingPaper(gdouble direction)
{
    //gdouble topLine;
    //gdouble prevWrappedPaper;
    //gdouble wrappedWrappedPaper,prevWrappedWrappedPaper,prevWrappedUnwrappedPaper;
    //gdouble wrappedUnwrappedPaper;


    wrappingPaperNew(direction);






#if 0    
    prevWrappedPaper = wrappedPaper;
    prevWrappedUnwrappedPaper = unwrappedPaper;
    
    wrappedPaper -= direction;
    unwrappedPaper = PaperArea.height - wrappedPaper;
    wrappedUnwrappedPaper = unwrappedPaper;

    wrappedWrappedPaper = wrappedPaper;
    prevWrappedWrappedPaper = prevWrappedPaper;
	
    wrapRange(wrappedWrappedPaper,940.0);
    wrapRange(prevWrappedWrappedPaper,940.0);
    wrapRange(prevWrappedUnwrappedPaper,940.0);
    wrapRange(wrappedUnwrappedPaper,940.0);

    topLine = peny - MIDDLE_LINE;
    wrapRange(topLine,DRUM_HIGH);
#endif	
    //g_debug("wrappedPaper=%.1f  unwrappedPaper=%.1f %.1f\n",wrappedPaper,unwrappedPaper,direction);





    
}
#endif



__attribute__((used)) 
gboolean
on_PlotterDrawingArea_motion_notify_event(GtkWidget *drawingArea,
					  __attribute__((unused)) GdkEventMotion *event,
					  __attribute__((unused)) gpointer data)
{
    if(warpToLeftHand)
    {
	warpToFinger(gtk_widget_get_parent_window (drawingArea),&LeftHandInfo);
	warpToLeftHand = FALSE;
	return GDK_EVENT_STOP;
    }
    
    if(warpToRightHand)
    {
	warpToFinger(gtk_widget_get_parent_window (drawingArea),&RightHandInfo);
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

static
void updateVisibleSurface(gboolean showPaper,gboolean setPaperPosition,gdouble paperPosition,
			  gboolean showTopSticky,
			  gboolean showBottomSticky)
{
#if 1    
    static gdouble paperTopLine = 0.0;

    if(setPaperPosition)
	paperTopLine = paperPosition;

    
    //Reset the visible surface to the drum image before transfering
    // Paper and stickies into the visible surface.
    gdk_cairo_set_source_pixbuf (visibleSurfaceCr, visible_pixbuf ,0.0,0.0);
    cairo_paint(visibleSurfaceCr);


#if 0
    if(showPaper)
    {
	gdk_cairo_set_source_pixbuf(visibleSurfaceCr,paperSmall_pixbuf,
				    PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
				    paperTopLine);

	cairo_rectangle(visibleSurfaceCr,
			PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
			paperTopLine,
			PaperArea.width,
			PaperArea.height);
	cairo_fill(visibleSurfaceCr);

	// Check if the paper spans the top/bottom boundary
	// and draw the part at the top of the visibleSurface
		
	//if( (PaperArea.y + topLine - DRUM_TOP) > DRUM_HIGH)
	{
	    gdk_cairo_set_source_pixbuf(visibleSurfaceCr,paperSmall_pixbuf,
					PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
					paperTopLine-DRUM_HIGH);

	    cairo_rectangle(visibleSurfaceCr,
			    PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
			    paperTopLine-DRUM_HIGH,
			    PaperArea.width+0.0,
			    PaperArea.height); 
	    cairo_fill(visibleSurfaceCr);	    
	}
    }
#endif
    cairo_set_operator (visibleSurfaceCr,CAIRO_OPERATOR_OVER);
    if(showPaper)
    {
	// Add the top sticky into the visibleSurface
	showArea2(visibleSurfaceCr,paperSmall_pixbuf,
		  PaperArea.x,PaperArea.y,
		  DRAWABLE_DRUM_LEFT, DRUM_TOP,
		  PaperArea.topLine);	
    }

    if(showTopSticky)
    {
	// Add the top sticky into the visibleSurface
	showArea2(visibleSurfaceCr,sticky_pixbuf,
		  topStickyArea.x,topStickyArea.y,
		  DRAWABLE_DRUM_LEFT, DRUM_TOP,
		  topStickyArea.topLine);
    }

    if(showBottomSticky)
    {
	// Add the bottom sticky into the visibleSurface
	showArea2(visibleSurfaceCr,sticky_pixbuf,
		  bottomStickyArea.x,bottomStickyArea.y,
		  DRAWABLE_DRUM_LEFT, DRUM_TOP,
		  bottomStickyArea.topLine);
    }	    
#endif	    
}    


__attribute__((used))
gboolean
on_PlotterDrawingArea_button_press_event(__attribute__((unused)) GtkWidget *drawingArea,
					 __attribute__((unused)) GdkEventButton *event,
					 __attribute__((unused)) gpointer data)
{
    HandInfo *trackingHand;
    struct knobInfo *knob;
    gdouble top,bottom,left,right;
    Rectangle *OneFingerArea;
    
    if(event->button == 3)
    {
	swapHands(drawingArea);
	return GDK_EVENT_STOP;
    }

    // Check for all the things that can be pointed at with one finger 
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
	for(int areaNumber = knobCount; areaNumber < knobCount2; areaNumber += 1)
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
    // Check to see if picking up piece of sticky tape
    else if( (trackingHand != NULL) && (trackingHand->showingHand == HAND_GRABBING)  )
    {
	//doFSM(&PlotterPaperFSM,PLOTTER_PRESS_STICKY,NULL);
	trackingHand->showingHand = HAND_STICKY_TAPE;
    }

 
    
    
   
    else if(PlotterPaperFSM.state == PLOTTER_HOLDING_PAPER)
    {
	 // Look for paper being dropped in top half.
	if(FingerPressedAtY < 150.0)
	    
	{
	    gdouble topLine;
	    topLine = peny - MIDDLE_LINE;
	    wrapRange(topLine,DRUM_HIGH);
	    PaperArea.topLine = topLine;
	    
	    doFSM(&PlotterPaperFSM,PLOTTER_PLACE_PAPER,NULL);
	    ConfigureHand(trackingHand,0.0,0.0,0,HAND_EMPTY);

	    if(handHoldingPaper == &LeftHandInfo)
		modf(FingerPressedAtX,&PaperArea.x);
	    else
		modf(FingerPressedAtX - PaperArea.width,&PaperArea.x);
	    
	    modf(FingerPressedAtY - PaperArea.height,&PaperArea.y);
	   
	    //PaperArea.y = FingerPressedAtY;
	    PaperMovedByX = PaperMovedByY = 0.0;
	}
	else
	    // Or for folder/bin presses
	{
	    for(int areaNumber= knobCount2; areaNumber < stickyKnobNumber; areaNumber += 1)
	    {
		OneFingerArea = &OneFingerAreas[areaNumber];
		left = OneFingerArea->x;
		right = left + OneFingerArea->width;
		top = OneFingerArea->y;
		bottom = top + OneFingerArea->height;

		if((FingerPressedAtX >= left) && (FingerPressedAtX <= right) &&
		   (FingerPressedAtY >= top) && (FingerPressedAtY <= bottom))
		{
		    if(knobs[areaNumber].handler != NULL) (knobs[areaNumber].handler)(0);
		}
	    }
	}

	
	
    } else if( (trackingHand != NULL) &&
	     (trackingHand->showingHand == HAND_STICKY_TAPE) &&
	     (PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED)
	)
    {
	
	g_debug("FingerPressedAtY-DRUM_TOP=%.1f  paperTop.y=%.1f \n",
		FingerPressedAtY-DRUM_TOP, paperTop);

	if( (fabs(FingerPressedAtY-DRUM_TOP - paperTop) < 7.0) &&
	    (FingerPressedAtX >= PaperArea.x) &&
	    (FingerPressedAtX <= PaperArea.x+PaperArea.width))
	{	
	    gdouble topLine;

	    topLine = peny - MIDDLE_LINE;
	    wrapRange(topLine,DRUM_HIGH);
	    
	    g_debug("STICKY WOULD BE  FIXED %.1f %.1f %.1f\n",paperTop,paperBottom,topLine);

	     // Add the top sticky
	    topSticky = TRUE;
	    topStickyArea.x = FingerPressedAtX-28.0;
	    topStickyArea.y = FingerPressedAtY-(STICKY_HIGH/2.0);
	    topStickyArea.width = 50;
	    topStickyArea.height = STICKY_HIGH;
	    topStickyArea.topLine = topLine;

	    
	    updateVisibleSurface(TRUE,FALSE,0.0,TRUE,TRUE);

	    doFSM(&PlotterPaperFSM,PLOTTER_FIX_TOP_EDGE,NULL);
	    trackingHand->showingHand = HAND_EMPTY;	    
	}
#if 0
	
	if( (fabs(FingerPressedAtY + unwrappedPaper - attachingLine) < 7.0) &&
	    (FingerPressedAtX >= PaperArea.x) &&
	    (FingerPressedAtX <= PaperArea.x+PaperArea.width))
	{
	    gdouble topLine;
	    gboolean wrapped;
	    
	    g_debug("STICKY FIXED\n");
	    doFSM(&PlotterPaperFSM,PLOTTER_FIX_TOP_EDGE,NULL);
	    trackingHand->showingHand = HAND_EMPTY;

	    //Reset the visible surface to the drum image before transfering
	    // Paper and stickies into the visible surface.

	    topLine = peny - MIDDLE_LINE;
	    if(topLine < 0) topLine = DRUM_HIGH + topLine;


#if 1
	    topLine = peny - MIDDLE_LINE;
	    wrapRange(topLine,DRUM_HIGH);

	    wrapped = FALSE;

	    top = topLine-unwrappedPaper+attachingLine-DRUM_TOP;
	    wrapRange(top,DRUM_HIGH);
	    bottom = top + PaperArea.height;
	    wrapRange(bottom,DRUM_HIGH);

	    g_debug("bottom-top=%.1f-%.1f=%.1f != %.1f\n",bottom,top,bottom-top,PaperArea.height);
	    
	    if(top > bottom)
	    {
		gdouble t;
		t = top;
		top = bottom;
		bottom = t;
		wrapped = TRUE;
	    }

	    VisibleArea pva = {PaperArea.x-DRAWABLE_DRUM_LEFT,
				  top,
				  PaperArea.x-DRAWABLE_DRUM_LEFT+PaperArea.width,
				  bottom,
				  wrapped};
	    PaperVisibleArea = pva;
#endif




	    
	     // Add the top sticky
	    topSticky = TRUE;
	    topStickyArea.x = FingerPressedAtX-28.0;
	    topStickyArea.y = FingerPressedAtY-(STICKY_HIGH/2.0);
	    topStickyArea.width = 50;
	    topStickyArea.height = STICKY_HIGH;
	    topStickyArea.topLine = topLine;

	    {
		wrapped = FALSE;
		
		bottom = topStickyArea.y+topLine-1.0;
		wrapRange(bottom,DRUM_HIGH);
		top = bottom - topStickyArea.height;
		wrapRange(top,DRUM_HIGH);

		if(top > bottom)
		{
		    gdouble t;
		    t = top;
		    top = bottom;
		    bottom = t;
		    wrapped = TRUE;
		}
		    

		
		VisibleArea sticky = {topStickyArea.x-DRAWABLE_DRUM_LEFT,
				    top,
				    topStickyArea.x-DRAWABLE_DRUM_LEFT+topStickyArea.width,
				    bottom,
				    wrapped};
		topStickyVisibleArea = sticky;
	    }

	    updateVisibleSurface(TRUE,TRUE,topLine-unwrappedPaper+attachingLine-DRUM_TOP,TRUE,TRUE);
	}
#endif	

    }  else if( (trackingHand != NULL) &&
	     (trackingHand->showingHand == HAND_EMPTY) &&
	     (PlotterPaperFSM.state == PLOTTER_PLACED_PAPER)
	)
    {

	g_debug("FingerPressedAtY=%.1f  PaperArea.y=%.1f\n",FingerPressedAtY,PaperArea.y);

	if( (fabs(FingerPressedAtY - (PaperArea.y+PaperArea.height)) < 7.0) &&
	    (
		( (trackingHand == &LeftHandInfo) &&
		  (FingerPressedAtX >= PaperArea.x) &&
		  (FingerPressedAtX <= PaperArea.x+10.0) ) ||
		( (trackingHand == &RightHandInfo) &&
		  (FingerPressedAtX >= PaperArea.x + PaperArea.width -10.0) &&
		  (FingerPressedAtX <= PaperArea.x + PaperArea.width ) )
		)
	    )
	{
	    g_debug("PICK UP \n");
	    trackingHand = getTrackingXY(&FingerPressedAtX,&FingerPressedAtY);
	    handHoldingPaper = trackingHand;
	    ConfigureHand(trackingHand,0.0,0.0,0,HAND_HOLDING_PAPER);


	    
	    doFSM(&PlotterPaperFSM,PLOTTER_PICKUP_PAPER,NULL);
	    //trackingHand->showingHand = HAND_HOLDING_PAPER;	    

	}
    } else if( (trackingHand != NULL) &&
	     (trackingHand->showingHand == HAND_STICKY_TAPE) &&
	     (PlotterPaperFSM.state == PLOTTER_PLACED_PAPER)
	)
    {
	// Check if placing sticky tape on bottom edge of paper
	g_debug("FingerPressedAtY=%.1f  PaperArea.y=%.1f\n",FingerPressedAtY,PaperArea.y);


	if( (fabs(FingerPressedAtY - (PaperArea.y+PaperArea.height)) < 7.0) &&
	    (FingerPressedAtX >= PaperArea.x) &&
	    (FingerPressedAtX <= PaperArea.x+PaperArea.width))
	{
	    gdouble topLine;
	    topLine = peny - MIDDLE_LINE;
	    wrapRange(topLine,DRUM_HIGH);

	    //wrappedPaper = FingerPressedAtY - DRUM_TOP;
	    //unwrappedPaper = PaperArea.height - wrappedPaper;
	    
	    doFSM(&PlotterPaperFSM,PLOTTER_FIX_BOTTOM_EDGE,NULL);
	    trackingHand->showingHand = HAND_EMPTY;

	    //g_debug("wrappedPaper = %.1f\n",wrappedPaper);

	


	    {
		wrappingFsm = NULL;
		wrappingFsmState = 0;
		showing = 0;
		
		// initialDrop is set from the position that the paper was dropped at. 
		initialDrop = (PaperArea.y+PaperArea.height) - DRUM_TOP;
		printf("A: %.1f < %.1f\n",PaperArea.height,initialDrop);
		printf("B: %.1f < %.1f < %.1f\n",initialDrop,PaperArea.height,initialDrop+DRUM_TOP);
		printf("C: %.1f < %.1f < %.1f\n",initialDrop+DRUM_TOP,PaperArea.height,LINES_VISIBLE);
		printf("D: %.1f < %.1f < %.1f\n",LINES_VISIBLE,PaperArea.height,LINES_VISIBLE+DRUM_TOP);
		printf("E: %.1f < %.1f < %.1f\n",LINES_VISIBLE+DRUM_TOP,PaperArea.height,DRUM_HIGH-LINES_VISIBLE);
		printf("F: %.1f < %.1f\n", DRUM_HIGH-LINES_VISIBLE,PaperArea.height);

		// TO DO  Check these comparisons (some should be <= not <
		if(PaperArea.height < initialDrop)
		{
		    paperSize = 1;
		    wrappingFsm = wrappingTableA;
		}
		else if( (initialDrop<PaperArea.height) && (PaperArea.height<initialDrop+DRUM_TOP) )
		{
		    paperSize = 2;
		    wrappingFsm = wrappingTableB;
		}
		else if( (initialDrop+DRUM_TOP<PaperArea.height) && (PaperArea.height<LINES_VISIBLE) )
		{
		    paperSize = 3;
		    wrappingFsm = wrappingTableC;
		}
		else if( (LINES_VISIBLE<PaperArea.height) && (PaperArea.height<LINES_VISIBLE+DRUM_TOP) )
		{
		    paperSize = 4;
		    wrappingFsm = wrappingTableD;
		    
		}
		else if( (LINES_VISIBLE+DRUM_TOP<PaperArea.height) && (PaperArea.height<DRUM_HIGH-LINES_VISIBLE) )
		{
		    paperSize = 5;
		    wrappingFsm = wrappingTableE;
		}
		else if(DRUM_HIGH-LINES_VISIBLE<PaperArea.height)
		{
		    paperSize = 6;
		     wrappingFsm = wrappingTableF;
		}
		else
		{
		    paperSize = 0;		    

		}
		g_debug("Papersize = %d %c\n",paperSize,0x40+paperSize);

		if(wrappingFsm != NULL)
		{

		    doWrappingFsm(ID,TRUE);
		    g_debug("Starting in state %d showing %d\n",wrappingFsmState,showing);

		}

		
		paperBottom = initialDrop;
		paperTop = paperBottom - PaperArea.height;
		wrapRange(paperTop,DRUM_HIGH);

		// Sticky position is set by the press positoin not the paper position
		stickyBottom = (FingerPressedAtY - DRUM_TOP) +(STICKY_HIGH/2.0); 
		stickyTop    = (FingerPressedAtY - DRUM_TOP) -(STICKY_HIGH/2.0); 
		wrapRange(stickyBottom,DRUM_HIGH);
		wrapRange(stickyTop,DRUM_HIGH);
		

	    }




	    

	     // Add the bottom sticky
	    bottomSticky = TRUE;
	    bottomStickyArea.x = FingerPressedAtX-28;
	    bottomStickyArea.y = FingerPressedAtY-(STICKY_HIGH/2.0);
	    bottomStickyArea.width = 50;
	    bottomStickyArea.height = STICKY_HIGH;
	    bottomStickyArea.topLine = topLine;


#if 0	        
	    doFSM(&PlotterPaperFSM,PLOTTER_FIX_BOTTOM_EDGE,NULL);
	    trackingHand->showingHand = HAND_EMPTY;
	    wrappedPaper = INITIAL_WRAPPED;
	    calculateWrapping();
	    

	    
	    topLine = peny - MIDDLE_LINE;
	    if(topLine < 0) topLine = DRUM_HIGH + topLine;
	    g_debug("PaperArea.y=%.1f DRUM_TOP=%.1f topLine=%.1f \n",PaperArea.y,DRUM_TOP,topLine);


	    // Attaching line

	    attachingLine = PaperArea.y - INITIAL_WRAPPED;
	    g_debug("attachingLine = %.1f (%.1f)\n",attachingLine, PaperArea.y + topLine - DRUM_TOP - DRUM_HIGH);


	    if(PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED)
	    {
		gdk_cairo_set_source_pixbuf(visibleSurfaceCr,paperSmall_pixbuf,
					    PaperArea.x-DRAWABLE_DRUM_LEFT,
					    PaperArea.y + topLine - DRUM_TOP-PaperArea.height);
		
		cairo_rectangle(visibleSurfaceCr,
				PaperArea.x-DRAWABLE_DRUM_LEFT,
				PaperArea.y + topLine - DRUM_TOP+0.0,
				PaperArea.width,
				-INITIAL_WRAPPED);
	
		cairo_fill(visibleSurfaceCr);
		
		// Check if the bottom few lines of the paper spans the top/bottom boundary
		// and draw duplicate to give part at the top of the visibleSurface
		
		if( (PaperArea.y + topLine - DRUM_TOP) > DRUM_HIGH)
		{
		    gdk_cairo_set_source_pixbuf(visibleSurfaceCr,paperSmall_pixbuf,
						PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
						PaperArea.y + topLine - DRUM_TOP-PaperArea.height -DRUM_HIGH);
		    cairo_rectangle(visibleSurfaceCr,
				    PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
				    PaperArea.y + topLine - DRUM_TOP - DRUM_HIGH+0.0,
				    PaperArea.width+0.0,
				    -INITIAL_WRAPPED); 
		    cairo_fill(visibleSurfaceCr);	    
		}
		
		
	    }



	     // Add the bottom sticky
	    bottomSticky = TRUE;
	    bottomStickyArea.x = FingerPressedAtX-28;
	    bottomStickyArea.y = FingerPressedAtY-(STICKY_HIGH/2.0);
	    bottomStickyArea.width = 50;
	    bottomStickyArea.height = STICKY_HIGH;
	    bottomStickyArea.topLine = topLine;
	    cairo_set_operator (visibleSurfaceCr,CAIRO_OPERATOR_OVER);

	    g_debug("peny=%.1f FingerPressedAtY=%.1f\n",peny,FingerPressedAtY);
	    // Add the bottom sticky into the visibleSurface
	    showArea2(visibleSurfaceCr,sticky_pixbuf,
		      bottomStickyArea.x,bottomStickyArea.y,
		      DRAWABLE_DRUM_LEFT, DRUM_TOP,
		      bottomStickyArea.topLine);
	    /* showArea2(visibleSurfaceCr,sticky_pixbuf, */
	    /* 	      FingerPressedAtX-28,FingerPressedAtY-(STICKY_HIGH/2.0), */
	    /* 	      DRAWABLE_DRUM_LEFT, DRUM_TOP, */
	    /* 	      topLine); */

	    cairo_set_operator (visibleSurfaceCr,CAIRO_OPERATOR_SOURCE);
	  
	    {
		gboolean wrapped = FALSE;
		
		bottom = bottomStickyArea.y+topLine-1.0;
		while(bottom >= 940.0) bottom -= 940.0;
		top = bottom - bottomStickyArea.height;

		while(top < 0.0) top += 940.0;
		

		if(top > bottom)
		{
		    gdouble t;
		    t = top;
		    top = bottom;
		    bottom = t;
		    wrapped = TRUE;
		}
		    

		
		VisibleArea sticky = {bottomStickyArea.x-DRAWABLE_DRUM_LEFT,
				    top,
				    bottomStickyArea.x-DRAWABLE_DRUM_LEFT+bottomStickyArea.width,
				    bottom,
				    wrapped};
		bottomStickyVisibleArea = sticky;
	    }
#endif
	    
	}

    }  else if( (trackingHand != NULL) &&
		( (PlotterPaperFSM.state == PLOTTER_BOTH_FIXED) ||
		  (PlotterPaperFSM.state == PLOTTER_BOTTOM_FREE) ||
		  (PlotterPaperFSM.state == PLOTTER_TOP_FREE) )
	)
    {
	gdouble fingerX,fingerY,topLine;
	topLine = peny - MIDDLE_LINE;
	
	
	fingerX = FingerPressedAtX - DRAWABLE_DRUM_LEFT;
	fingerY = FingerPressedAtY - DRUM_TOP + topLine;
	
	wrapRange(fingerY,DRUM_HIGH);
	
	g_debug("Press when bots fixed (%.1f,%.1f) (%.1f,%.1f) (%.1f,%.1f) %s\n",
		fingerX,fingerY,
		bottomStickyVisibleArea.left,bottomStickyVisibleArea.top,
		bottomStickyVisibleArea.right,bottomStickyVisibleArea.bottom,
		bottomStickyVisibleArea.wrapped ? "Wrapped":"Not Wrapped");
	
	if( (fingerX >= bottomStickyVisibleArea.left) &&
	    (fingerX <= (bottomStickyVisibleArea.right)))
	{
	    if(bottomStickyVisibleArea.wrapped != (	(fingerY > bottomStickyVisibleArea.top) &&
							(fingerY <= (bottomStickyVisibleArea.bottom)) ) )
	    {
		g_debug("ON BOTTOM STICKY\n");
		doFSM(&PlotterPaperFSM,PLOTTER_PRESS_BOTTOM_STICKY,NULL);
		updateVisibleSurface(TRUE,FALSE,0.0,TRUE,FALSE);
	
	    }
	}


	g_debug("Press when bots fixed (%.1f,%.1f) (%.1f,%.1f) (%.1f,%.1f) %s\n",
		fingerX,fingerY,
		topStickyVisibleArea.left,topStickyVisibleArea.top,
		topStickyVisibleArea.right,topStickyVisibleArea.bottom,
		topStickyVisibleArea.wrapped ? "Wrapped":"Not Wrapped");
	if( (fingerX >= topStickyVisibleArea.left) &&
	    (fingerX <= (topStickyVisibleArea.right)))
	{
	    if(topStickyVisibleArea.wrapped != (	(fingerY > topStickyVisibleArea.top) &&
							(fingerY <= (topStickyVisibleArea.bottom)) ) )
	    {
		g_debug("ON TOP STICKY\n");
		doFSM(&PlotterPaperFSM,PLOTTER_PRESS_TOP_STICKY,NULL);
		updateVisibleSurface(TRUE,FALSE,0.0,FALSE,TRUE);
	    }
	}


	if(PlotterPaperFSM.state == PLOTTER_PLACED_PAPER)
	{
	    updateVisibleSurface(FALSE,FALSE,0.0,FALSE,FALSE);
	}
	
    }


#if 0
    {
	// This works but should be using the visible area not the rectangle
	gdouble topLine,fingerY;
	topLine = peny - MIDDLE_LINE;
	if(topLine < 0) topLine = DRUM_HIGH + topLine;

	fingerY = topLine + FingerPressedAtY;
	while(fingerY >= 940.) fingerY -= 940.0;

	
	OneFingerArea = &bottomStickyArea;
	left = OneFingerArea->x;
	right = left + OneFingerArea->width;
	top = OneFingerArea->y + OneFingerArea->topLine;
	bottom = top + OneFingerArea->height;
	while(top >= 940.) top -= 940.0;
	while(bottom >= 940.) bottom -= 940.0;

	g_debug("Press when both fixed (%d,%d) (%d,%d) (%.1f,%.1f)\n",
		left,top,right,bottom,FingerPressedAtX,fingerY);
	
	if((FingerPressedAtX >= left) && (FingerPressedAtX <= right) &&
	   (fingerY >= top) && (fingerY <= bottom))
	{

	    g_debug("Press on bitton sticky\n");
	    //if(knobs[areaNumber].handler != NULL) (knobs[areaNumber].handler)(knob->state);
	}
    }
 #endif
    
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



    // Always release the finger (even if not over a button).
    if(trackingHand != NULL) trackingHand->FingersPressed &= ~trackingHand->IndexFingerBit;

    
    return GDK_EVENT_STOP;
}






// Used to reposition the mouse cursor at the tracking hand when the hand becomes
// unconstrained after trying to move with fingers pressed
static 
void warpToFinger(GdkWindow *win,HandInfo *hand)
{
    int ox,oy;

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




    if(movingHand == &LeftHandInfo)
    {
	
	if(LeftHandInfo.showingHand != showing)
	{
	    //g_debug("Left hand changed\n");
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


static void yStep(gdouble dy)
{

    peny += dy;
    wrapRange(peny,DRUM_HIGH);
}

static void xStep(gdouble dx)
{
    penx += dx;
    if(penx < 0.0) penx = 0.0;
    else if(penx>550.0) penx = 550.0;
    //wrapRange(penx,DRAWABLE_DRUM_WIDE);
}



static void plotPoint(void)
{
    gboolean draw = TRUE;
    gboolean overPaper = FALSE;

    /* g_debug("(%.1f,%.1f) (%.1f,%.1f) (%.1f,%.1f) %s\n",penx,peny, */
    /* 	    topStickyVisibleArea.left,topStickyVisibleArea.top, */
    /* 	    topStickyVisibleArea.right,topStickyVisibleArea.bottom, */
    /* 	    topStickyVisibleArea.wrapped ? "Wrapped":"Not Wrapped"); */
    if( (penx >= topStickyVisibleArea.left) &&
	(penx <= (topStickyVisibleArea.right)))
    {
	if(topStickyVisibleArea.wrapped != (	(peny > topStickyVisibleArea.top) &&
						(peny <= (topStickyVisibleArea.bottom)) ) )
	{
	    g_debug("ON BOTTOM STICKY\n");
	    draw = FALSE;
	}
    }


    
    /* g_debug("(%.1f,%.1f) (%.1f,%.1f) (%.1f,%.1f) %s\n",penx,peny, */
    /* 	    bottomStickyVisibleArea.left,bottomStickyVisibleArea.top, */
    /* 	    bottomStickyVisibleArea.right,bottomStickyVisibleArea.bottom, */
    /* 	    bottomStickyVisibleArea.wrapped ? "Wrapped":"Not Wrapped"); */
    if( (penx >= bottomStickyVisibleArea.left) &&
	(penx <= (bottomStickyVisibleArea.right)))
    {
	if(bottomStickyVisibleArea.wrapped != (	(peny > bottomStickyVisibleArea.top) &&
						(peny <= (bottomStickyVisibleArea.bottom)) ) )
	{
	    g_debug("ON BOTTOM STICKY\n");
	    draw = FALSE;
	}
    }
    


    if(draw)
    {

	cairo_set_line_width (visibleSurfaceCr, 1);
	cairo_set_line_cap  (visibleSurfaceCr, CAIRO_LINE_CAP_ROUND);
	cairo_move_to (visibleSurfaceCr,penx,peny);
	cairo_line_to (visibleSurfaceCr,penx,peny);
	cairo_stroke (visibleSurfaceCr);
	
	g_debug("(%.1f,%.1f) (%.1f,%.1f) (%.1f,%.1f) %s\n",penx,peny,
		PaperVisibleArea.left,PaperVisibleArea.top,
		PaperVisibleArea.right,PaperVisibleArea.bottom,
		PaperVisibleArea.wrapped ? "Wrapped":"Not Wrapped");
	cairo_set_source_rgba(visibleSurfaceCr,0.0,0.0,0.0,1.0);
	if( (penx >= PaperVisibleArea.left) &&
	    (penx <= (PaperVisibleArea.right)))
	{
	    if(PaperVisibleArea.wrapped != (	(peny > PaperVisibleArea.top) &&
							(peny <= (PaperVisibleArea.bottom)) ) )
	    {
		overPaper = TRUE;
		//printf("OVER PAPER\n");
	    }
	}
	

       
       if(overPaper)
       {
	
	//printf("ON  Paper %d %.1f\n",PenX,(2.0*PaperArea.x));
	// Draw full resolution version if paper loaded
	   //if(paperSurfaceCr != NULL)

	   g_debug("2*PaperVisibleArea.top=%.1f 2*PaperVisibleArea.bottom=%.1f\n",
		   2.0*PaperVisibleArea.top,2.0*PaperVisibleArea.bottom);
	   
	   if(!PaperVisibleArea.wrapped)
	   {
	       gdouble x,y;

	       x =  penx-(2.0*PaperVisibleArea.left);
	       y =  peny-(2.0*PaperVisibleArea.top);

	       g_debug("(x,y) = (%.1f,%.1f)\n",x,y);
	   }
	   else
	   {
	       gdouble x,y;

	       x =  penx-(2.0*PaperVisibleArea.left);
	       y =  peny-(2.0*PaperVisibleArea.bottom);
	       wrapRange(y,2.0*DRUM_HIGH);

	       g_debug("(x,y) = (%.1f,%.1f)\n",x,y);
	   }
       }
    }
}



static void ACTchanged(unsigned int value)
{
    //gdouble oldpeny;
    
    if(value == 1)
    {
	if(PLOTTERF72)
	{
	    if(CLines & 1)
	    {
		//PenX += 1;
		//limitX();
		xStep(0.5);
	    }
	    if(CLines & 2)
	    {
		//PenX -= 1;
		//limitX();
		xStep(-0.5);
	    }
	    if(CLines & 4)
	    {
		
		//PenY -= 1;
		//oldpeny = peny;
		//wrapY();
		yStep(-0.5);
		if(PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED) 
		    wrappingPaper(-0.5);
		
	    }
	    if(CLines & 8)
	    {
		//PenY += 1;
		//oldpeny = peny;
		//wrapY();
		yStep(0.5);
		if(PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED) 
		    wrappingPaper(0.5);
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
	    yStep(0.5*drumFastMove);
	    if(PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED) 
		wrappingPaper(0.5*drumFastMove);
	}

	if(carriageFastMove != 0)
	{
	    xStep(0.5*carriageFastMove);
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
    //static Rectangle Paper = {0.0,0.0,400.0,80.0,0.0};
    //static Rectangle Paper = {0.0,0.0,400.0,95.0,0.0};
    static Rectangle Paper = {0.0,0.0,400.0,120.0,0.0};
    //static Rectangle Paper = {0.0,0.0,400.0,200.0,0.0};
    //static Rectangle Paper = {0.0,0.0,400.0,340.0,0.0};
    //static Rectangle Paper = {0.0,0.0,400.0,621.0,0.0};
    HandInfo *trackingHand;
    gdouble topLine,fx,fy; //,top,bottom;
//    gboolean wrapped = FALSE;


    g_info("squarePaperHandler\n");

    if(PlotterPaperFSM.state == PLOTTER_NO_PAPER)
    {
#if 0
	
	trackingHand = getTrackingXY(&fx,&fy);
	topLine = peny - MIDDLE_LINE;
	wrapRange(topLine,DRUM_HIGH);

	PaperArea.x = fx - 30.0;
	PaperArea.y = fy -(STICKY_HIGH/2.0);
	PaperArea.topLine = topLine;
	
	wrapped = FALSE;
		
	bottom = PaperArea.y+topLine-1.0;
	while(bottom >= 940.0) bottom -= 940.0;
	top = bottom - PaperArea.height;

	while(top < 0.0) top += 940.0;
		

	if(top > bottom)
	{
	    gdouble t;
	    t = top;
	    top = bottom;
	    bottom = t;
	    wrapped = TRUE;
	}

	VisibleArea sticky = {PaperArea.x-DRAWABLE_DRUM_LEFT,
			      top,
			      PaperArea.x-DRAWABLE_DRUM_LEFT+PaperArea.width,
			      bottom,
			      wrapped};
	PaperVisibleArea = sticky;
#endif
	trackingHand = getTrackingXY(&fx,&fy);
	topLine = peny - MIDDLE_LINE;
	wrapRange(topLine,DRUM_HIGH);

	PaperArea = Paper;
	
	handHoldingPaper = trackingHand;
	ConfigureHand(trackingHand,0.0,0.0,0,HAND_HOLDING_PAPER);
    
	doFSM(&PlotterPaperFSM,PLOTTER_PRESS_NEW_PAPER,NULL);
    }
}

static void PaperBinHandler(__attribute__((unused)) int state)
{
    HandInfo *trackingHand;

    g_info("PaperBinHandler\n");
    trackingHand = getTrackingXY(NULL,NULL);

    if( (PlotterPaperFSM.state == PLOTTER_HOLDING_PAPER) &&
	(trackingHand->showingHand == HAND_HOLDING_PAPER))
    {
	handHoldingPaper = NULL;
	ConfigureHand(trackingHand,0.0,0.0,0,HAND_EMPTY);
    
	doFSM(&PlotterPaperFSM,PLOTTER_DROP_PAPER,NULL);

//	PaperArea = NULL;
    }
}




#if 0
static void stickyTapeHandler(__attribute__((unused)) int sstate)
{
    //holdingStikyTape = TRUE;
    g_debug("Happened\n");
    doFSM(&PlotterPaperFSM,PLOTTER_PRESS_STICKY,NULL);
    //ConfigureLeftHandNew(0.0,0.0,0,HAND_STICKY_TAPE);

}
#endif

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
	xStep(0.5 * (state -1));
	
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

    if( (PlotterPowerOn) && (state != 1) )
    {
	yStep(0.5*(state -1));

	if(PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED)  wrappingPaper(0.5*(state - 1));
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
	g_debug("Writing paper image to %s\n",drumFileName->str);

	cairo_surface_write_to_png (cairo_get_target(paperSurfaceCr),drumFileName->str);
    }
    else
	g_debug("Paper image NOT SAVED\n");
   

 
    g_string_printf(drumFileName,"%s%s",userPath->str,"Visible.png");
    
    cairo_surface_write_to_png (cairo_get_target(visibleSurfaceCr),drumFileName->str);

    g_string_free(drumFileName,TRUE);

    configText = g_string_new("# Plotter  configuration\n");

    gtk_window_get_position(GTK_WINDOW(PlotterWindow), &windowXpos, &windowYpos);
    g_string_append_printf(configText,"WindowPosition %d %d\n",windowXpos,windowYpos);
    if(PaperLoaded)
    {
	g_string_append_printf(configText,"PaperInfo %.1f %.1f %.1f %.1f\n",
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
    Rectangle *switchArea;
    
    PlotterWindow = GTK_WIDGET(gtk_builder_get_object_checked (builder, "PlotterWindow"));
    PlotterDrawingArea = GTK_WIDGET(gtk_builder_get_object_checked(builder,"PlotterDrawingArea"));


    fileName = g_string_new(NULL);
    g_string_printf(fileName,"%sgraphics/calcomp.png",sharedPath->str);
    
    background_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
    
    width = gdk_pixbuf_get_width(background_pixbuf);
    height = gdk_pixbuf_get_height(background_pixbuf) ;

    gtk_window_set_default_size(GTK_WINDOW(PlotterWindow), width, height);

    g_string_printf(fileName,"%sgraphics/carriage.png",sharedPath->str);
    
    carriage_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);

    // Try to restore plotter images from users directory
    // Use images from 803-Resources if not found.
    g_string_printf(fileName,"%sDrum2.png",userPath->str);
    drum_pixbuf =
	gdk_pixbuf_new_from_file(fileName->str,NULL);
    
    if(drum_pixbuf == NULL)
    {
	g_string_printf(fileName,"%sgraphics/Drum2.png",sharedPath->str);
	drum_pixbuf =
	    my_gdk_pixbuf_new_from_file(fileName->str);
    }


    

    g_string_printf(fileName,"%sPaperSmall3.png",userPath->str);
    paperSmall_pixbuf =
	gdk_pixbuf_new_from_file(fileName->str,NULL);	

    g_debug("paperSmall_pixbuf=%p\n",paperSmall_pixbuf);

    
    
    g_string_printf(fileName,"%sVisible2.png",userPath->str);
    visible_pixbuf =
	gdk_pixbuf_new_from_file(fileName->str,NULL);
    if(visible_pixbuf == NULL)
    {
	g_string_printf(fileName,"%sgraphics/Drum2.png",sharedPath->str);
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
    switchArea->topLine =  0.0;
    

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
    switchArea->topLine =  0.0;
    
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
    switchArea->topLine =  0.0;
    
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
    switchArea->topLine =  0.0;
    
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
    switchArea->topLine =  0.0;
    
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
    switchArea->topLine =  0.0;
    
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
    switchArea->topLine =  0.0;
    
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
    

    /* Paper Bin */
    
    knob = &knobs[knobNumber];

    switchArea = &OneFingerAreas[knobNumber];
    switchArea->x = 371;
    switchArea->y = 422;	
    switchArea->width = 50;
    switchArea->height = 56;
    knob->handler = PaperBinHandler;

    knobNumber += 1;
#if 0
     /* Paper Folder */
    
    knob = &knobs[knobNumber];

    switchArea = &OneFingerAreas[knobNumber];
    switchArea->x = 306;
    switchArea->y = 422;	
    switchArea->width = 60;
    switchArea->height = 56;
    knob->handler = PaperFolderHandler;

    knobNumber += 1   
#endif

    
    
     
    /* Sticky tape area  */
        
    knob = &knobs[knobNumber];
    stickyKnobNumber = knobNumber;
    
    switchArea = &OneFingerAreas[knobNumber];
    switchArea->x = 477;
    switchArea->y = 424;	
    switchArea->width = 38;
    switchArea->height = 29;
    switchArea->topLine =  0.0;
    
    //knob->handler = stickyTapeHandler;   // Not called

    knobNumber += 1;

    
   
    

    penx = 0.0;
    //PenY = (PAPER_TOP+PAPER_HIGH);    // 1660 puts pen at 830
    peny =  0.0;
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
	    g_error("Loaded paper pixbuf size is wrong. (%d,%d) should be (%.1f,%.1f)\n",
		    w,h,2*PaperArea.width,2*PaperArea.height);
	}

	// THis is the full resolution version so double the dimenstions
	paperSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,w,h);
	// 2*PaperArea.width,2*PaperArea.height);

	paperSurfaceCr = cairo_create(paperSurface);
	gdk_cairo_set_source_pixbuf (paperSurfaceCr, paper_pixbuf ,0.0,0.0);
	
	cairo_paint(paperSurfaceCr);
    }
    

    g_string_printf(fileName,"%sgraphics/Sticky.png",sharedPath->str);
    sticky_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
    
    g_string_free(fileName,TRUE);
    fileName = NULL;
    
    gtk_widget_show(PlotterWindow);





}
