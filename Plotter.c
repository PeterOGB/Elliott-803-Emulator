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

static gboolean topSticky = FALSE;
static Rectangle topStickyArea = {0.0,0.0,0.0,0.0,0.0};
static VisibleArea topStickyVisibleArea = {0.0,0.0,0.0,0.0,0.0};

static gboolean bottomSticky = FALSE;
static Rectangle bottomStickyArea = {0.0,0.0,0.0,0.0,0.0}; 
static VisibleArea bottomStickyVisibleArea = {0.0,0.0,0.0,0.0,0.0};

static gdouble WrappedPaper = 0.0;
static gdouble wrappedPaper = 0.0;
static gdouble unwrappedPaper = 0.0;
static gdouble attachingLine = 0.0;
static GdkPixbuf *paperSmall_pixbuf;


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


static gdouble penx,peny;    // For half resolution image
static gdouble PenX,PenY;    // For full resolution image
static gboolean PenDown = TRUE;
static cairo_t *visibleSurfaceCr = NULL;
static Rectangle PaperArea = {0.0,0.0,0.0,0.0,0.0};
static Rectangle PaperVisibleArea = {0.0,0.0,0.0,0.0,0.0};
static gboolean PaperLoaded = FALSE;
//static gboolean PaperPositioning = FALSE;
//static gboolean PaperMoving = FALSE;
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

static
void on_Hand_motion_event(HandInfo *MovingHand);

static 
void warpToFinger(GdkWindow *win,HandInfo *hand);

static
void wrappingPaper(int direction);

static
void calculateWrapping(void);

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





enum PlotterStates {PLOTTER_NO_PAPER,PLOTTER_HOLDING_PAPER,PLOTTER_DROPPED_PAPER,
		    PLOTTER_BOTTOM_FIXED,
		    PLOTTER_BOTH_FIXED,PLOTTER_TOP_FIXED,PLOTTER_STICKY3,PLOTTER_TOP_FREE,
		    PLOTTER_BOTTOM_FREE,PLOTTER_BOTH_FREE,PLOTTER_POSITIONING2,PLOTTER_PAPER_REMOVED};

const char *PlotterStateNames[] =
    {"PLOTTER_NO_PAPER","PLOTTER_HOLDING_PAPER","PLOTTER_DROPPED_PAPER",
     "PLOTTER_BOTTOM_FIXED",
     "PLOTTER_BOTH_FIXED","PLOTTER_TOP_FIXED","PLOTTER_STICKY3","PLOTTER_TOP_FREE",
     "PLOTTER_BOTTOM_FREE","PLOTTER_BOTH_FREE","PLOTTER_POSITIONING2","PLOTTER_PARER_REMOVED"};


enum PlotterEvents {PLOTTER_PRESS_NEW_PAPER,PLOTTER_DROP_PAPER,PLOTTER_PAPER_PRESS,
		    PLOTTER_FIX_BOTTOM_EDGE,PLOTTER_FIX_TOP_EDGE,
		    PLOTTER_PRESS_BOTTOM_STICKY,PLOTTER_PRESS_TOP_STICKY,
		    PLOTTER_PICKUP_PAPER};

const char *PlotterEventNames[] =
{"PLOTTER_PRESS_NEW_PAPER", "PLOTTER_DROP_PAPER","PLOTTER_PAPER_PRESS",
 "PLOTTER_FIX_BOTTOM_EDGE","PLOTTER_FIX_TOP_EDGE",
 "PLOTTER_PRESS_BOTTOM_EDGE","PLOTTER_PRESS_TOP_EDGE",
 "PLOTTER_PICKUP_PAPER"};



struct fsmtable PlotterPaperTable[] = {

    {PLOTTER_NO_PAPER,         PLOTTER_PRESS_NEW_PAPER,    PLOTTER_HOLDING_PAPER,     NULL},

    {PLOTTER_HOLDING_PAPER,    PLOTTER_DROP_PAPER,         PLOTTER_DROPPED_PAPER,     NULL},

    {PLOTTER_DROPPED_PAPER,    PLOTTER_PICKUP_PAPER,       PLOTTER_HOLDING_PAPER,     NULL},
    {PLOTTER_DROPPED_PAPER,    PLOTTER_FIX_BOTTOM_EDGE,    PLOTTER_BOTTOM_FIXED,      NULL},

    {PLOTTER_BOTTOM_FIXED,     PLOTTER_FIX_TOP_EDGE,       PLOTTER_BOTH_FIXED,        NULL},

    {PLOTTER_BOTH_FIXED,       PLOTTER_PRESS_BOTTOM_STICKY,PLOTTER_BOTTOM_FREE,       NULL},
    {PLOTTER_BOTH_FIXED,       PLOTTER_PRESS_TOP_STICKY,   PLOTTER_TOP_FREE,          NULL},

    {PLOTTER_BOTTOM_FREE,      PLOTTER_PRESS_TOP_STICKY,   PLOTTER_DROPPED_PAPER,     NULL},
    {PLOTTER_TOP_FREE,         PLOTTER_PRESS_BOTTOM_STICKY,PLOTTER_DROPPED_PAPER,     NULL},
    
    
    {-1,-1,-1,NULL}
};

#if 0
struct fsmtable PlotterPaperTableOld[] = {

    {PLOTTER_NO_PAPER,         PLOTTER_PRESS_NEW_PAPER,    PLOTTER_HOLDING_BELOW,     NULL},

    {PLOTTER_HOLDING_BELOW,    PLOTTER_ABOVE_CARRIAGE,     PLOTTER_HOLDING_ABOVE,     NULL},

    {PLOTTER_HOLDING_ABOVE,    PLOTTER_BELOW_CARRIAGE,     PLOTTER_HOLDING_BELOW,     NULL},
    {PLOTTER_HOLDING_ABOVE,    PLOTTER_DROP_PAPER,         PLOTTER_DROPPED,           NULL},
    
    {PLOTTER_DROPPED,          PLOTTER_PAPER_PRESS,        PLOTTER_POSITIONING,       NULL},
    {PLOTTER_DROPPED,          PLOTTER_PRESS_STICKY,       PLOTTER_STICKY1,           NULL},
    
    {PLOTTER_POSITIONING,      PLOTTER_PAPER_RELEASE,      PLOTTER_DROPPED,           NULL},

    {PLOTTER_STICKY1,          PLOTTER_PRESS_BOTTOM_EDGE,  PLOTTER_BOTTOM_FIXED,      NULL},
    {PLOTTER_STICKY1,          PLOTTER_PRESS_TOP_EDGE,     PLOTTER_TOP_FIXED,         NULL},

    {PLOTTER_BOTTOM_FIXED,     PLOTTER_PRESS_STICKY,       PLOTTER_STICKY2,           NULL},

    {PLOTTER_STICKY2,          PLOTTER_PRESS_TOP_EDGE,     PLOTTER_BOTH_FIXED,        NULL},

    {PLOTTER_BOTH_FIXED,       PLOTTER_PRESS_BOTTOM_EDGE,  PLOTTER_BOTTOM_FREE,       NULL},
    {PLOTTER_BOTH_FIXED,       PLOTTER_PRESS_TOP_EDGE,     PLOTTER_TOP_FREE,          NULL},
    
    {PLOTTER_TOP_FIXED,        PLOTTER_PRESS_STICKY,       PLOTTER_STICKY3,           NULL},

    {PLOTTER_STICKY3,          PLOTTER_PRESS_BOTTOM_EDGE,  PLOTTER_BOTH_FIXED,        NULL},

    {PLOTTER_TOP_FREE,         PLOTTER_PRESS_BOTTOM_EDGE,  PLOTTER_BOTH_FREE,         NULL},

    {PLOTTER_BOTTOM_FREE,      PLOTTER_PRESS_TOP_EDGE,     PLOTTER_BOTH_FREE,         NULL},

    {PLOTTER_BOTH_FREE,        PLOTTER_PAPER_PRESS,        PLOTTER_POSITIONING2,      NULL},
    {PLOTTER_BOTH_FREE,        PLOTTER_PRESS_CORNER,       PLOTTER_PAPER_REMOVED,     NULL},

    {PLOTTER_POSITIONING2,     PLOTTER_DROP_PAPER,         PLOTTER_BOTH_FREE,         NULL},

    {PLOTTER_PAPER_REMOVED,    PLOTTER_PRESS_NEW_PAPER,    PLOTTER_NO_PAPER,          NULL},

    {-1,-1,-1,NULL}
};

#endif

struct fsm PlotterPaperFSM = { "Plotter FSM",0, PlotterPaperTable ,
			 PlotterStateNames,PlotterEventNames,1,-1};

     





void calculateWrapping(void)
{
    modf(WrappedPaper,&wrappedPaper);
    unwrappedPaper = PaperArea.height - wrappedPaper;
}



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

    while(bot < 0.0) bot += visibleHeight;
    if(bot >= visibleHeight) bot -= visibleHeight;
    
    while(top < 0.0) top += visibleHeight;
    if(top >= visibleHeight) top -= visibleHeight;

    if(bot < top)
    {
	gdk_cairo_set_source_pixbuf (cr, pixbuf , atX,top);
	cairo_rectangle(cr, atX,top,itemWidth,visibleHeight-top);
	cairo_fill(cr);
	    
	gdk_cairo_set_source_pixbuf (cr, pixbuf , atX,top-visibleHeight);
	cairo_rectangle(cr, atX,0.0,itemWidth,bot);
	cairo_fill(cr);
    }
    else
    {
	gdk_cairo_set_source_pixbuf (cr, pixbuf , atX,top);
	cairo_rectangle(cr, atX,top,itemWidth,itemHeight);
	cairo_fill(cr);
    }

}



// These are all in pixels which is half resolution
#define LINES_VISIBLE 330
#define MIDDLE_LINE 165
#define DRAWABLE_DRUM_WIDE 552
#define DRUM_WIDE 616
#define DRUM_HIGH 940.0
#define DRAWABLE_DRUM_LEFT 72.0
#define DRUM_LEFT 39.0
#define DRUM_TOP 21.0


#define STICKY_HIGH 20
#define INITIAL_WRAPPED 25


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
	visibleSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,560,940);
	//DRAWABLE_DRUM_WIDE,DRUM_HIGH);
	visibleSurfaceCr = cairo_create(visibleSurface);
	// Set operator and antialias to that exact drawing to pixels works
	cairo_set_operator (visibleSurfaceCr,CAIRO_OPERATOR_SOURCE);
	cairo_set_antialias (visibleSurfaceCr,CAIRO_ANTIALIAS_NONE);

	// Initialise it to the drum image.
	if(visible_pixbuf != NULL)
	    gdk_cairo_set_source_pixbuf (visibleSurfaceCr, visible_pixbuf ,0.0,0.0);
	
	cairo_paint(visibleSurfaceCr);


	// Create surface for things drawn on the drum !
	drumSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						  DRAWABLE_DRUM_WIDE,DRUM_HIGH);
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
    if(PlotterPaperFSM.state == PLOTTER_DROPPED_PAPER)
    {
	gdouble y;
	//cairo_set_source_rgba(cr,1.0,1.0,1.0,1.0);
	//g_debug("PaperArea.y=%f\n",PaperArea.y);
	//gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x,-456);
	//cairo_rectangle(cr,PaperArea.x,PaperArea.y,PaperArea.width,-PaperArea.height);
	y = -PaperArea.height + PaperArea.y - 0.0;
	gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,PaperArea.x+1.0,y);
	cairo_rectangle(cr,PaperArea.x+1.0,y,PaperArea.width,PaperArea.height);
	//cairo_stroke_preserve(cr);
	cairo_fill(cr);

    }

/*   THIS ONE WORKS */
    if( (PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED) && (unwrappedPaper > 0.0) ) 
    {
	if(wrappedPaper >= INITIAL_WRAPPED)
	{
	    //cairo_set_source_rgba(cr,1.0,1.0,1.0,1.0);
	    gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,
					PaperArea.x-1.0,
				   -unwrappedPaper+attachingLine );
	    cairo_rectangle(cr,PaperArea.x-1.0,
			    attachingLine+0.0,
			    PaperArea.width+1.0,
			    -unwrappedPaper);
	    //cairo_stroke_preserve(cr);
	    cairo_fill(cr);
	}
	else
	{
	    
	    //cairo_set_source_rgba(cr,1.0,1.0,1.0,1.0);

	    gdk_cairo_set_source_pixbuf(cr,paperSmall_pixbuf,
					PaperArea.x-1.0,
					-unwrappedPaper+attachingLine );
	    

	    cairo_rectangle(cr,PaperArea.x-1.0,
			    attachingLine+(wrappedPaper-INITIAL_WRAPPED)+1.0,
			    PaperArea.width+1.0,
			    -unwrappedPaper);
	    //cairo_stroke_preserve(cr);
	    cairo_fill(cr);

	}

    }
  
    
/*
    if(PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED)
    {
	if( (unwrappedPaper >= 0) && (wrappedPaper >= 0))
	{
 	    cairo_set_source_rgba(cr,1.0,0.0,0.0,1.0);
	    cairo_rectangle(cr,PaperArea.x,PaperArea.y-20,PaperArea.width,20-unwrappedPaper/2.0);
	    cairo_stroke_preserve(cr);
	    cairo_fill(cr);
	}

	if( (unwrappedPaper >= 0) && (wrappedPaper >= 0))
	{
	    cairo_set_source_rgba(cr,1.0,0.0,0.0,1.0);
	    cairo_rectangle(cr,PaperArea.x,PaperArea.y-20,PaperArea.width,20-unwrappedPaper/2.0);
	    cairo_stroke_preserve(cr);
	    cairo_fill(cr);
	}

	if(wrappedPaper <= 0)
	{
	    cairo_set_source_rgba(cr,0.0,1.0,0.0,1.0);
	    cairo_rectangle(cr,PaperArea.x,PaperArea.y+(wrappedPaper/2.0)-20,PaperArea.width,20-PaperArea.height);
	    cairo_stroke_preserve(cr);
	    cairo_fill(cr);
	}

    }
*/
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
    

    // Draw the carriage
    cairo_set_source_surface(cr,carriageSurface,DRUM_LEFT+penx, DRUM_TOP+MIDDLE_LINE-32.0);
    cairo_rectangle(cr, DRUM_LEFT+penx,  DRUM_TOP+MIDDLE_LINE-32.0, 64.0, 64.0);
    //cairo_stroke_preserve(cr);
    cairo_fill(cr);



    // Draw paper in hand over the top of anythign else (except the hands).
    if(PlotterPaperFSM.state == PLOTTER_HOLDING_PAPER) 
    {
	gdouble x,y;
	// Draw the paper being held by the hand.
	getTrackingXY2(handHoldingPaper,&x,&y);
	
	//cairo_set_source_rgba(cr,1.0,1.0,1.0,1.0);


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
	//cairo_stroke_preserve(cr);
	cairo_fill(cr);

    }


/*
    if(bottomSticky)
    {
	cairo_set_source_rgba(cr,0.3,0.3,0.3,0.5);
	cairo_rectangle(cr,bottomStickyArea.x,bottomStickyArea.y - drumMovesSinceFixed,
			bottomStickyArea.width,bottomStickyArea.height);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgba(cr,0.8,0.8,0.8,0.8);
	cairo_fill(cr);
    }
*/
    
    if(InPlotterWindow)
	DrawHandsNew(cr); 
    return FALSE;
}




static void wrappingPaper(int direction)
{
    gdouble topLine;

    WrappedPaper -= direction;
    calculateWrapping();
    topLine = peny - MIDDLE_LINE;
    if(topLine < 0) topLine = DRUM_HIGH + topLine;
	
    g_debug("wrappedPaper=%f  unwrappedPaper=%f %d\n",wrappedPaper,unwrappedPaper,direction);

    
#if 1   
    if( (direction < 0) && (unwrappedPaper > 0.0)  && (wrappedPaper >= INITIAL_WRAPPED))
    {
	gdouble yline;
    	//cairo_set_source_rgba(visibleSurfaceCr,1.0,1.0,1.0,1.0);
	//cairo_set_line_width (visibleSurfaceCr, 1);
	//cairo_set_line_cap  (visibleSurfaceCr, CAIRO_LINE_CAP_BUTT);

	yline = PaperArea.y + topLine - DRUM_TOP - INITIAL_WRAPPED + 1.0;
	if(yline > DRUM_HIGH) yline -= DRUM_HIGH;
	g_debug("yline = %f\n",yline);

	gdk_cairo_set_source_pixbuf(visibleSurfaceCr,paperSmall_pixbuf,
				    PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
				    yline-PaperArea.height+wrappedPaper-1.0);
	cairo_rectangle(visibleSurfaceCr,PaperArea.x-1.0- DRAWABLE_DRUM_LEFT,yline-1.0,PaperArea.width+0.0, +1.0);
	cairo_fill(visibleSurfaceCr);

	/*
	cairo_move_to (visibleSurfaceCr,
		       PaperArea.x-1.0-DRAWABLE_DRUM_LEFT,
		       yline);
	cairo_line_to (visibleSurfaceCr,
		       PaperArea.x+PaperArea.width+0.0-DRAWABLE_DRUM_LEFT,
		       yline); 
	cairo_stroke (visibleSurfaceCr);
	*/
    }

    // TO DO !!
    
    if( (direction > 0) && (wrappedPaper >= INITIAL_WRAPPED)  && (unwrappedPaper > 0.0))
    {
	gdouble yline;
    	cairo_set_source_rgba(visibleSurfaceCr,0.5,0.5,0.5,1.0);
	cairo_set_line_width (visibleSurfaceCr, 1);
	cairo_set_line_cap  (visibleSurfaceCr, CAIRO_LINE_CAP_BUTT);
	yline = PaperArea.y + topLine - DRUM_TOP - INITIAL_WRAPPED + 0.0;
	if(yline > DRUM_HIGH) yline -= DRUM_HIGH;

	//cairo_set_source_surface (visibleSurfaceCr, drumSurface ,0.0,0.0); //DRAWABLE_DRUM_LEFT,yline);
	//cairo_rectangle(visibleSurfaceCr,PaperArea.x-1.0- DRAWABLE_DRUM_LEFT,yline-1,PaperArea.width+1, 1.0);
	//cairo_fill(visibleSurfaceCr);
	
	cairo_move_to (visibleSurfaceCr,
		       PaperArea.x-1.0-DRAWABLE_DRUM_LEFT,
		       yline);
	cairo_line_to (visibleSurfaceCr,
		       PaperArea.x+PaperArea.width+0.0-DRAWABLE_DRUM_LEFT,
		       yline);
	cairo_stroke (visibleSurfaceCr);
	
    }
#endif    
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
/*	int ox,oy;
	GdkWindow *win;
	win = gtk_widget_get_parent_window (drawingArea);

	gdk_window_get_origin (win,&ox,&oy);
	
	ox += ((gint) RightHandInfo.FingerAtX);
	oy += ((gint) RightHandInfo.FingerAtY);

	gdk_device_warp (event->device,
                         gdk_screen_get_default(),
                         ox,oy);*/
	
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
    // Check to see if picking up piece of sticky tape
    else if( (trackingHand != NULL) && (trackingHand->showingHand == HAND_GRABBING)  )
    {
	//doFSM(&PlotterPaperFSM,PLOTTER_PRESS_STICKY,NULL);
	trackingHand->showingHand = HAND_STICKY_TAPE;
    }

    
    // Look for paper being dropped in top half.
    else if( (PlotterPaperFSM.state == PLOTTER_HOLDING_PAPER) &&
	(FingerPressedAtY < 150.0) )
    
    {
	doFSM(&PlotterPaperFSM,PLOTTER_DROP_PAPER,NULL);
	ConfigureHand(trackingHand,0.0,0.0,0,HAND_EMPTY);

	if(handHoldingPaper == &LeftHandInfo)
	    modf(FingerPressedAtX,&PaperArea.x);
	else
	    modf(FingerPressedAtX - PaperArea.width,&PaperArea.x);

	modf(FingerPressedAtY,&PaperArea.y);
	//PaperArea.y = FingerPressedAtY;
	PaperMovedByX = PaperMovedByY = 0.0;
	
    } else if( (trackingHand != NULL) &&
	     (trackingHand->showingHand == HAND_STICKY_TAPE) &&
	     (PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED)
	)
    {
	
	g_debug("FingerPressedAtY=%f  PaperArea.y=%f %f\n",
		FingerPressedAtY,PaperArea.y, -unwrappedPaper+attachingLine);
	if( (fabs(FingerPressedAtY + unwrappedPaper - attachingLine) < 7.0) &&
	    (FingerPressedAtX >= PaperArea.x) &&
	    (FingerPressedAtX <= PaperArea.x+PaperArea.width))
	{
	    gdouble topLine;
	    g_debug("STICKY FIXED\n");
	    doFSM(&PlotterPaperFSM,PLOTTER_FIX_TOP_EDGE,NULL);
	    trackingHand->showingHand = HAND_EMPTY;

	    //Reset the visible surface to the drum image before transfering
	    // Paper and stickies into the visible surface.
	    gdk_cairo_set_source_pixbuf (visibleSurfaceCr, visible_pixbuf ,0.0,0.0);
	    cairo_paint(visibleSurfaceCr);

	    topLine = peny - MIDDLE_LINE;
	    if(topLine < 0) topLine = DRUM_HIGH + topLine;

	    gdk_cairo_set_source_pixbuf(visibleSurfaceCr,paperSmall_pixbuf,
					PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
					topLine-unwrappedPaper+attachingLine-DRUM_TOP);
	 

	    cairo_rectangle(visibleSurfaceCr,
			    PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
			    topLine -unwrappedPaper+attachingLine -DRUM_TOP + 0.0,
			    PaperArea.width,
			    PaperArea.height);
	
	    cairo_fill(visibleSurfaceCr);


	    // Check if the paper spans the top/bottom boundary
	    // and draw the part at the top of the visibleSurface
		
	    //if( (PaperArea.y + topLine - DRUM_TOP) > DRUM_HIGH)
	    {
		gdk_cairo_set_source_pixbuf(visibleSurfaceCr,paperSmall_pixbuf,
					    PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
					    topLine-unwrappedPaper+attachingLine-DRUM_TOP-DRUM_HIGH);

		cairo_rectangle(visibleSurfaceCr,
				PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
				topLine -unwrappedPaper+attachingLine -DRUM_TOP + 0.0-DRUM_HIGH,
				PaperArea.width+0.0,
				PaperArea.height); 

		cairo_fill(visibleSurfaceCr);	    
	    }

	     // Add the top sticky
	    topSticky = TRUE;
	    topStickyArea.x = FingerPressedAtX-28;
	    topStickyArea.y = FingerPressedAtY-(STICKY_HIGH/2.0);
	    topStickyArea.width = 50;
	    topStickyArea.height = STICKY_HIGH;
	    topStickyArea.topLine = topLine;

	    {
		gboolean wrapped = FALSE;
		
		bottom = topStickyArea.y+topLine-1.0;
		while(bottom >= 940.0) bottom -= 940.0;
		top = bottom - topStickyArea.height;

		while(top < 0.0) top += 940.0;
		

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
	    
	    // Add the top sticky into the visibleSurface
	    cairo_set_operator (visibleSurfaceCr,CAIRO_OPERATOR_OVER);
	    showArea2(visibleSurfaceCr,sticky_pixbuf,
		      FingerPressedAtX-28,FingerPressedAtY-(STICKY_HIGH/2.0),
		      DRAWABLE_DRUM_LEFT, DRUM_TOP,
		      topLine);

	    // Add the bottom sticky into the visibleSurface
	    showArea2(visibleSurfaceCr,sticky_pixbuf,
		      bottomStickyArea.x,bottomStickyArea.y,
		      DRAWABLE_DRUM_LEFT, DRUM_TOP,
		      bottomStickyArea.topLine);

	    
	    
	}
	

    } else if( (trackingHand != NULL) &&
	     (trackingHand->showingHand == HAND_STICKY_TAPE) &&
	     (PlotterPaperFSM.state == PLOTTER_DROPPED_PAPER)
	)
    {

	g_debug("FingerPressedAtY=%f  PaperArea.y=%f\n",FingerPressedAtY,PaperArea.y);


	
	if( (fabs(FingerPressedAtY - PaperArea.y) < 7.0) &&
	    (FingerPressedAtX >= PaperArea.x) &&
	    (FingerPressedAtX <= PaperArea.x+PaperArea.width))
	{
	    gdouble topLine;
	    topLine = peny - MIDDLE_LINE;

	    g_debug("topLine=%f peny=%f\n",topLine,peny);
	    if(topLine < 0)
	    {
		topLine = DRUM_HIGH + topLine;
	    }
	    g_debug("topLine=%f peny=%f\n",topLine,peny);
	    
	    doFSM(&PlotterPaperFSM,PLOTTER_FIX_BOTTOM_EDGE,NULL);
	    trackingHand->showingHand = HAND_EMPTY;
	    WrappedPaper = INITIAL_WRAPPED;
	    calculateWrapping();
	    

	    
	    topLine = peny - MIDDLE_LINE;
	    if(topLine < 0) topLine = DRUM_HIGH + topLine;
	    g_debug("PaperArea.y=%f DRUM_TOP=%f topLine=%f \n",PaperArea.y,DRUM_TOP,topLine);


	    // Attaching line

	    attachingLine = PaperArea.y - INITIAL_WRAPPED;
	    g_debug("attachingLine = %f (%f)\n",attachingLine, PaperArea.y + topLine - DRUM_TOP - DRUM_HIGH);


	    if(PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED)
	    {
		gdk_cairo_set_source_pixbuf(visibleSurfaceCr,paperSmall_pixbuf,
					    PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
					    PaperArea.y + topLine - DRUM_TOP-PaperArea.height);
		
		cairo_rectangle(visibleSurfaceCr,
				PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
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
						//-(PaperArea.height-(PaperArea.y + topLine - DRUM_TOP - DRUM_HIGH)));
					
		    //PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,288);
		    //PaperArea.y + topLine - DRUM_TOP -DRUM_HIGH + 600.0);
		    //cairo_set_source_rgba(visibleSurfaceCr,1.0,0.0,0.0,1.0);
		    cairo_rectangle(visibleSurfaceCr,
				    PaperArea.x-DRAWABLE_DRUM_LEFT-1.0,
				    PaperArea.y + topLine - DRUM_TOP - DRUM_HIGH+0.0,
				    PaperArea.width+0.0,
				    -INITIAL_WRAPPED); 
		    //cairo_stroke_preserve(visibleSurfaceCr);
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

	    g_debug("peny=%f FingerPressedAtY=%f\n",peny,FingerPressedAtY);
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

	    
/*	    
	    // Add sticky into visible surface
	    cairo_rectangle(visibleSurfaceCr,
			    bottomStickyArea.x-DRAWABLE_DRUM_LEFT,
			    topLine+ bottomStickyArea.y-DRUM_TOP-DRUM_HIGH,
			    //bottomStickyArea.y-topLine,
			    bottomStickyArea.width,bottomStickyArea.height);
	    cairo_set_source_rgba(visibleSurfaceCr,0.0,0.0,0.0,0.5);
	    cairo_stroke_preserve(visibleSurfaceCr);
	    cairo_set_source_rgba(visibleSurfaceCr,0.3,0.3,0.3,0.5);
	    cairo_fill(visibleSurfaceCr);
*/
	  
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

	    
	}

    }  else if( (trackingHand != NULL) &&
		//(trackingHand->showingHand == HAND_STICKY_TAPE) &&
	     (PlotterPaperFSM.state == PLOTTER_BOTH_FIXED)
	)
    {
	gdouble fingerX,fingerY,topLine;
	topLine = peny - MIDDLE_LINE;
	
	
	fingerX = FingerPressedAtX - DRAWABLE_DRUM_LEFT;
	fingerY = FingerPressedAtY - DRUM_TOP + topLine;
	
	wrapRange(fingerY,DRUM_HIGH);
	//while(fingerY < 0.0) fingerY += 940.0;
	//while(fingerY >= 940.0) fingerY -= 940.0;
	
	g_debug("Press when bots fixed (%f,%f) (%f,%f) (%f,%f) %s\n",
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
		//draw = FALSE;
	    }
	}


	g_debug("Press when bots fixed (%f,%f) (%f,%f) (%f,%f) %s\n",
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
		//draw = FALSE;
	    }
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

	g_debug("Press when both fixed (%d,%d) (%d,%d) (%f,%f)\n",
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

static void wrapY(void)
{

    if(PenY < 0)
    {
	PenY += (2*DRUM_HIGH);
	g_info("Wrap 1 %f\n",PenY);
    }else if(PenY >= (2*DRUM_HIGH))
    {
	PenY -= (2*DRUM_HIGH);
	g_info("Wrap 2 %f\n",PenY);
    }
    
    modf(PenY / 2.0,&peny);
}

static void limitX(void)
{
	if(PenX > 1100)
	{
	    PenX = 1100;
	    penx = 550;
	} else 	if(PenX < 0)
	{
	    PenX = 0;
	    penx = 0;
	}
	else
	    penx = PenX / 2;
}

static void plotPoint(void)
{
    //gdouble halfX,halfY,halfXPlus;
    gboolean draw = TRUE;
    gboolean overPaper = FALSE;
    //halfX = PenX/2;
    //halfXPlus = halfX + 32.5;
    //halfY = PenY/2;

    
#if 0

    /* g_debug("(%f,%f) (%f,%f) (%f,%f)\n",halfXPlus,halfY, */
    /* 	    topStickyVisibleArea.x,topStickyVisibleArea.y, */
    /* 	    topStickyVisibleArea.x+topStickyVisibleArea.width, */
    /* 	    topStickyVisibleArea.y-topStickyVisibleArea.height); */
    if( (penx >= topStickyVisibleArea.x) &&
	(penx <= (topStickyVisibleArea.x+topStickyVisibleArea.width)) &&
	(peny <= topStickyVisibleArea.y) &&
	(peny >= (topStickyVisibleArea.y-topStickyVisibleArea.height)) )
    {
	//g_debug("ON TOP STICKY\n");
	draw = FALSE;
    }

    if(
	(penx >= topStickyVisibleArea.x) &&
	(penx <= (topStickyVisibleArea.x+topStickyVisibleArea.width)) &&
	(peny <= 750.0+topStickyVisibleArea.y) &&
	(peny >= 750.0+(topStickyVisibleArea.y-topStickyVisibleArea.height)) )
    {
	//g_debug("ON TOP STICKY IN DUPE\n");
	draw = FALSE;
    }
#endif


    g_debug("(%f,%f) (%f,%f) (%f,%f) %s\n",penx,peny,
   	    topStickyVisibleArea.left,topStickyVisibleArea.top,
   	    topStickyVisibleArea.right,topStickyVisibleArea.bottom,
	    topStickyVisibleArea.wrapped ? "Wrapped":"Not Wrapped");
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


    
    g_debug("(%f,%f) (%f,%f) (%f,%f) %s\n",penx,peny,
   	    bottomStickyVisibleArea.left,bottomStickyVisibleArea.top,
   	    bottomStickyVisibleArea.right,bottomStickyVisibleArea.bottom,
	    bottomStickyVisibleArea.wrapped ? "Wrapped":"Not Wrapped");
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
	cairo_set_source_rgba(visibleSurfaceCr,0.0,0.0,0.0,1.0);
	cairo_set_line_width (visibleSurfaceCr, 1);
	cairo_set_line_cap  (visibleSurfaceCr, CAIRO_LINE_CAP_ROUND);
	cairo_move_to (visibleSurfaceCr,penx+0.5,peny+0.5);
	cairo_line_to (visibleSurfaceCr,penx+0.5,peny+0.5);
	cairo_stroke (visibleSurfaceCr);


       if( ( (penx >= PaperVisibleArea.x) &&
	     (penx <= (PaperVisibleArea.x+PaperVisibleArea.width)) &&
	     (peny     <= PaperVisibleArea.y) &&
	     (peny     >= (PaperVisibleArea.y-PaperVisibleArea.height)) ) ||
	   ( (penx >= PaperVisibleArea.x) &&
	     (penx <= (PaperVisibleArea.x+PaperVisibleArea.width)) &&
	     (peny     <= 750.0+PaperVisibleArea.y) &&
	     (peny     >= 750.0+(PaperVisibleArea.y-PaperVisibleArea.height)))
	   )
       {
	   overPaper = TRUE;
       }


       
       if(overPaper)
       {
	
	//printf("ON  Paper %d %f\n",PenX,(2.0*PaperArea.x));
	// Draw full resolution version if paper loaded
	   if(paperSurfaceCr != NULL)
	   {
	       gdouble x,y;

	       x =  0.5+PenX-(2*PaperVisibleArea.x);
	       y =  0.5+PenY-(2*(PaperVisibleArea.y-PaperVisibleArea.height));

//	       if(y > (-2.0 * PaperVisibleArea.height))
//		   y -= 400.0;

	       //if(y > 400.0) y -= 400.0;

//	   y -= (2.0 * PaperVisibleArea.height);
	       if(y > 1500.0) y -= 1500.0;

	       //top = 2*(PaperVisibleArea.y-PaperVisibleArea.height);

	       g_debug("(%f,%f) (%f,%f) (%f,%f) [%f,%f] %s\n",PenX,PenY,
		       2*PaperVisibleArea.x,2*PaperVisibleArea.y,
		       2*(PaperVisibleArea.x+PaperVisibleArea.width),
		       2*(PaperVisibleArea.y-PaperVisibleArea.height),
		       x,y,
		       overPaper ? "TRUE":"FALSE");

	       
	       cairo_set_source_rgba(paperSurfaceCr,0.0,0.0,0.0,1.0);
	       cairo_set_line_width (paperSurfaceCr, 1);
	       cairo_set_line_cap  (paperSurfaceCr, CAIRO_LINE_CAP_ROUND);
	       cairo_move_to (paperSurfaceCr,x,y);
	       cairo_line_to (paperSurfaceCr,x,y);
	       cairo_stroke (paperSurfaceCr);
	   }
       }
       else
       {
	   cairo_set_source_rgba(drumSurfaceCr,0.0,0.0,0.0,1.0);
	   cairo_set_line_width (drumSurfaceCr, 1);
	   cairo_set_line_cap  (drumSurfaceCr, CAIRO_LINE_CAP_ROUND);
	   cairo_move_to (drumSurfaceCr,0.5+penx,0.5+peny);
	   cairo_line_to (drumSurfaceCr,0.5+penx,0.5+peny);
	   cairo_stroke (drumSurfaceCr);
	   

	   
	   //printf("OFF Paper\n");
       }
    }
   
}

static void ACTchanged(unsigned int value)
{
    gdouble oldpeny;
    
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
		oldpeny = peny;
		wrapY();
		if( (peny != oldpeny) && (PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED) )
		    wrappingPaper(-1);
		
	    }
	    if(CLines & 8)
	    {
		PenY += 1;
		oldpeny = peny;
		wrapY();
		if( (peny != oldpeny) && (PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED) )
		    wrappingPaper(1);
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
    gdouble oldpeny;
    if(PlotterPowerOn)
    {
	if(drumFastMove != 0)
	{
	    PenY +=  drumFastMove;
	    oldpeny = peny;
	    wrapY();
	    if( (peny != oldpeny) && (PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED) )
		wrappingPaper(drumFastMove);
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
    
    //static cairo_surface_t *paperSurface = NULL;
    static Rectangle Paper = {0.0,0.0,400.0,600.0,0.0};
    HandInfo *trackingHand;

    g_info("squarePaperHandler\n");

    if(PlotterPaperFSM.state == PLOTTER_NO_PAPER)
    {
	trackingHand = getTrackingXY(&FingerPressedAtX,&FingerPressedAtY);
	handHoldingPaper = trackingHand;
	ConfigureHand(trackingHand,0.0,0.0,0,HAND_HOLDING_PAPER);
    
	doFSM(&PlotterPaperFSM,PLOTTER_PRESS_NEW_PAPER,NULL);

	PaperArea = Paper;
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
    gdouble oldpeny;
    
    if( (PlotterPowerOn) && (state != 1) )
    {
	PenY += state -1;
	oldpeny = peny;
	wrapY();
	if(peny != oldpeny)
	{

	    if(PlotterPaperFSM.state == PLOTTER_BOTTOM_FIXED)  wrappingPaper(state - 1);
	    if(PenDown)
	    {
		plotPoint();
	    }
	    plotterMoved = TRUE;
	}
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
	g_string_append_printf(configText,"PaperInfo %f %f %f %f\n",
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


    

    g_string_printf(fileName,"%sPaperSmall.png",userPath->str);
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

    
   
    

    PenX = 0;
    penx = 0;
    //PenY = (PAPER_TOP+PAPER_HIGH);    // 1660 puts pen at 830
    peny = PenY = 0;
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
	    g_error("Loaded paper pixbuf size is wrong. (%d,%d) should be (%f,%f)\n",
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
