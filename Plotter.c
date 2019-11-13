#define G_LOG_USE_STRUCTURED

#include <gtk/gtk.h>

#include "Logging.h"
#include "Plotter.h"
#include "Wiring.h"
#include "Hands.h"
#include "Common.h"
#include "fsm.h"


extern int CPU_word_time_count;

static GtkWidget *PlotterWindow;
GtkWidget *PlotterDrawingArea;
static GdkPixbuf *background_pixbuf;
static GdkPixbuf *paper_pixbuf;
static GdkPixbuf *knobPixbufs[9];
static GdkPixbuf *carriage_pixbuf;
int knobCount;
gboolean plotterMoved = FALSE;
static int drumFastMove = 0;
static int plotterBusyUntil = 0;

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

} knobs[6];     /* There are 6 knobs on the Plotter */


static int PenX,PenY;
static gboolean PenDown = TRUE;
static cairo_t *drumSurfaceCr = NULL;

//void PlotterTidy()

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
    struct WindowEvent *wep  = (struct WindowEvent *) p;
    enum handimages image;
    guint32 entryTimeStamp;

    g_info("Entered\n");

    activeGdkWindow = wep->window;   // BUG FIX ?
    
    entryTimeStamp = ((GdkEventCrossing *)wep->data)->time;

    //printf("%s TIME: %"PRIu32"\n",__FUNCTION__,entryTimeStamp - exitTimeStamp);
     
    //EnteredAtX = wep->eventX;
    //EnteredAtY = wep->eventY;

    // updateHands not called so need to do this
    setMouseAtXY(wep->eventX,wep->eventY);

    if((entryTimeStamp - exitTimeStamp > 500))
    {
	image = HandIsEmpty(LeftHand) ? HAND_EMPTY  : HAND_NO_CHANGE;
	ConfigureLeftHandNew (100.0,250.0,SET_TARGETXY|SET_RESTINGXY|SET_FINGERXY,image);

	// TODO This will need more when hands can hold things other than tapes.
	image = HandIsEmpty(RightHand) ? HAND_EMPTY  : HAND_NO_CHANGE;
	ConfigureRightHandNew(400.0,250.0,SET_TARGETXY|SET_RESTINGXY|SET_FINGERXY,image);
    }
    else
    {
	// Quick re-entry
	//printf("RE-ENTRY AT %f %f\n",wep->eventX,wep->eventY);
	image = HandIsEmpty(LeftHand) ? HAND_EMPTY : HAND_NO_CHANGE;
	ConfigureLeftHandNew (LeftHandExitedAtX,LeftHandExitedAtY,SET_TARGETXY|SET_RESTINGXY|SET_FINGERXY,image);
	image = HandIsEmpty(RightHand) ? HAND_EMPTY : HAND_NO_CHANGE;
	ConfigureRightHandNew(RightHandExitedAtX,RightHandExitedAtY,SET_TARGETXY|SET_RESTINGXY|SET_FINGERXY,image);

    }

    // Set flags to move the pointer to the default hand position if tracking a hand on enty.
    if(LeftHandInfo.Fsm->state == TRACKING_HAND)
    {
	warpToLeftHand = TRUE;
    }
    if(RightHandInfo.Fsm->state == TRACKING_HAND)
    {
	warpToRightHand = TRUE;
    }

    SetActiveWindow(PLOTTERWINDOW);
#if 0    
    if(HandIsEmpty(LeftHand))
	ConfigureLeftHandNew (100.0,250.0,SET_RESTINGXY|SET_FINGERXY,HAND_THREE_FINGERS);
    else
	ConfigureLeftHandNew (100.0,250.0,SET_RESTINGXY|SET_FINGERXY,HAND_HOLDING_REEL);
    
    if(HandIsEmpty(RightHand))
	ConfigureRightHandNew(400.0,250.0,SET_RESTINGXY|SET_FINGERXY,HAND_THREE_FINGERS);
    else
	ConfigureRightHandNew(400.0,250.0,SET_RESTINGXY|SET_FINGERXY,HAND_HOLDING_REEL);
    
    //ConfigureLeftHandNew (100.0,250.0,SET_RESTINGXY|SET_FINGERXY,HAND_SLIDING_TAPE);
    //ConfigureRightHandNew(300.0,250.0,SET_RESTINGXY|SET_FINGERXY,HAND_SLIDING_TAPE);
    
    printf("%s called EnteredAtX = %f \n",__FUNCTION__,EnteredAtX);
    if(EnteredAtX < 250.0)
    {
//	ConfigureLeftHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_SLIDING_TAPE);
	if(HandIsEmpty(LeftHand))
	{
	    ConfigureLeftHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_THREE_FINGERS);
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
//	ConfigureRightHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_SLIDING_TAPE);
	if(HandIsEmpty(RightHand))
	{
	    ConfigureRightHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_THREE_FINGERS);
	}
	else
	{
	    ConfigureRightHandNew(EnteredAtX,EnteredAtY,SET_TARGETXY|SET_FINGERXY,HAND_HOLDING_REEL);
	}
	setRightHandMode (TRACKING_HAND);
	setLeftHandMode(IDLE_HAND);
	SetActiveWindow(PLOTTERWINDOW);
    }

#endif
    
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

     // Remove cursor if tracking a hand on entry
    if(warpToLeftHand || warpToRightHand)
    {
	savedCursor = gdk_window_get_cursor(wep->window);
	gdk_window_set_cursor(wep->window,blankCursor);
    }

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



static struct fsmtable PlotterWindowTable[] = {
    {OUTSIDE_WINDOW,           FSM_ENTER,           INSIDE_WINDOW,            PlotterWindowEnterHandler},
    
    {INSIDE_WINDOW,            FSM_LEAVE,           OUTSIDE_WINDOW,           PlotterWinowLeaveHandler},
/*
    {INSIDE_WINDOW,            FSM_CONSTRAINED,     CONSTRAINED_INSIDE,       NULL}, 
    
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
on_PlotterDrawingArea_enter_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
				    __attribute__((unused)) GdkEventCrossing *event,
				    __attribute__((unused)) gpointer data)
{
    struct WindowEvent we = {PLOTTERWINDOW,event->x,event->y,event->window,(gpointer) event};

    //

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
#define PAPER_LEFT 50
#define PAPER_TOP 200
#define DRUM_WIDE 616
#define DRUM_HIGH 750
#define DRUM_LEFT 39.0
#define DRUM_TOP 18.0



__attribute__((used))
gboolean
on_PlotterDrawingArea_draw( __attribute__((unused)) GtkWidget *drawingArea,
			    __attribute__((unused)) cairo_t *cr,
			    __attribute__((unused)) gpointer data)
{
    static gboolean firstCall = TRUE;
    static cairo_surface_t *backgroundSurface = NULL;
    static cairo_t *backgroundSurfaceCr = NULL;
    static cairo_surface_t *drumSurface = NULL;
    static cairo_surface_t *carriageSurface = NULL;
     static cairo_t *carriageSurfaceCr = NULL;
    //static cairo_t *drumSurfaceCr = NULL;
    GtkAllocation  DrawingAreaAlloc;
    struct knobInfo *knob;
    //static int yOffset = 0;

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


	// Create surface for the drum
	drumSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						  DRUM_WIDE,DRUM_HIGH+LINES_VISIBLE);

	drumSurfaceCr = cairo_create(drumSurface);
	// Set to grey (and should draw sprockets)
	cairo_set_source_rgba(drumSurfaceCr,0.8,0.8,0.8,1.0);
	cairo_paint(drumSurfaceCr);

	// Add a piece of white paper
	cairo_set_source_rgba(drumSurfaceCr,1.0,1.0,1.0,1.0);
	cairo_rectangle(drumSurfaceCr, PAPER_LEFT,PAPER_TOP,PAPER_WIDE,PAPER_HIGH);
	cairo_stroke_preserve(drumSurfaceCr);
	cairo_fill(drumSurfaceCr);

	// Add duplicate at bottom of surface
	cairo_set_source_rgba(drumSurfaceCr,1.0,1.0,1.0,1.0);
	cairo_rectangle(drumSurfaceCr, PAPER_LEFT,PAPER_TOP+DRUM_HIGH,PAPER_WIDE,PAPER_HIGH);
	cairo_stroke_preserve(drumSurfaceCr);
	cairo_fill(drumSurfaceCr);
	

	// Red outline for testing 
	//cairo_set_source_rgba(drumSurfaceCr,1.0,0.0,0.0,1.0);
	//cairo_rectangle(drumSurfaceCr,PAPER_LEFT,PAPER_TOP,PAPER_WIDE,PAPER_HIGH);
	//cairo_stroke(drumSurfaceCr);

#if 0
	cairo_set_line_width (drumSurfaceCr, 10);
	cairo_set_source_rgba(drumSurfaceCr,0.0,1.0,0.0,1.0);
	cairo_move_to(drumSurfaceCr,0.0,0.0);
	cairo_line_to(drumSurfaceCr,DRUM_WIDE,0.0);
	cairo_stroke(drumSurfaceCr);

	cairo_set_source_rgba(drumSurfaceCr,0.0,0.0,1.0,1.0);
	cairo_move_to(drumSurfaceCr,0.0,DRUM_HIGH);
	cairo_line_to(drumSurfaceCr,DRUM_WIDE,DRUM_HIGH);
	cairo_stroke(drumSurfaceCr);
	
	cairo_set_source_rgba(drumSurfaceCr,1.0,0.0,0.0,1.0);
	cairo_move_to(drumSurfaceCr,0.0,DRUM_HIGH+LINES_VISIBLE);
	cairo_line_to(drumSurfaceCr,DRUM_WIDE,DRUM_HIGH+LINES_VISIBLE);
	cairo_stroke(drumSurfaceCr);
#endif



    }


    

    // Check for any knobs that had their "changed" flag set and red1aw them into the background
    for(int knobNumber = 0; knobNumber < knobCount; knobNumber += 1)
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

    /*
    cairo_set_source_rgba(drumSurfaceCr,0.0,0.0,0.0,1.0);
    cairo_set_line_width (drumSurfaceCr, 1);
    cairo_set_line_cap  (drumSurfaceCr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to (drumSurfaceCr, counter, 50.0); cairo_line_to (drumSurfaceCr, counter, 50.0);
    cairo_stroke (drumSurfaceCr);
    counter += 1;
    */
    
    cairo_set_source_surface (cr, backgroundSurface ,0.0,0.0);
    cairo_paint(cr);
    
    //cairo_set_source_surface (cr, drumSurface ,39.0,670); // 835-(PenY/2));
    //cairo_rectangle(cr, 39.0, 20.0, 616.0-20.0, 330);

    PenY +=  10 * drumFastMove;

    if(PenY<LINES_VISIBLE)    // PenY/2 < MIDDLE_LINE
    {
	PenY += (2*DRUM_HIGH);
    }
    else if((PenY/2) > (DRUM_HIGH+MIDDLE_LINE))   // PenY/2 > DRUM_HIGH+MIDDLE_LINE
    {
	PenY -= (2*DRUM_HIGH);
    } 
    // Draw the part of the drum surface visible in the window
    cairo_set_source_surface (cr, drumSurface ,DRUM_LEFT,DRUM_TOP+MIDDLE_LINE-(PenY/2)); //

    
    cairo_rectangle(cr, DRUM_LEFT, DRUM_TOP,DRUM_WIDE, LINES_VISIBLE);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);


// Draw the carriage
    cairo_set_source_surface(cr,carriageSurface,40.0+(PenX/2), DRUM_TOP+MIDDLE_LINE-32.0);
    cairo_rectangle(cr, 40.0+(PenX/2), 24.0+165-32.0, 64.0, 64.0);
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
			g_info("Right Hit button number %d %d ",knobNumber,knob->state);
		    }
		    else
		    {
			if(knob->state > 0) knob->state -= 1;
			g_info("Left  Hit button number %d %d",knobNumber,knob->state);
		    }
		    knob->changed = TRUE;
		    break;
		case 1:
		    if((FingerPressedAtX - left) >= (knob->width/2))
		    {
			if(knob->state < 2) knob->state = 2;
			g_info("Right Hit button number %d %d ",knobNumber,knob->state);
		    }
		    else
		    {
			if(knob->state > 0) knob->state = 0;
			g_info("Left  Hit button number %d %d",knobNumber,knob->state);
		    }
		    knob->changed = TRUE;
		    break;

		    


		    

		default:
		    break;
		}
		if(knob->handler != NULL) (knob->handler)(knob->state);
		break;
	    }
 
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

    g_info("Releaase\n");
    
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


static GdkRectangle OneFingerAreas[10];



// Called from the timerTick in Hands.c when a hand (not the mouse cursor) moves.
static void on_Hand_motion_event(HandInfo *movingHand)
{

    int n,ix,iy;
    enum  handimages showing;
    gdouble hx,hy;

    //printf("%s called\n",__FUNCTION__);

    //movingHand = updateHands(event->x,event->y,&hx,&hy);
    getTrackingXY2(movingHand,&hx,&hy);
    ix = (int) hx;
    iy = (int) hy;

    showing = HAND_EMPTY;
    

    for(n=0;n<knobCount;n++)
    {
	if( (ix >= OneFingerAreas[n].x) &&
	    (ix <= (OneFingerAreas[n].x+OneFingerAreas[n].width)) &&
	    (iy >= OneFingerAreas[n].y) &&
	    (iy <= (OneFingerAreas[n].y+OneFingerAreas[n].height)) )
	{
	    showing = HAND_ONE_FINGER;
	}
    }

/*
    if(movingHand->handConstrained == HAND_NOT_CONSTRAINED)
    {
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
	    ConfigureLeftHandNew (0.0,0.0,0,showing);
    }

    if(movingHand == &RightHandInfo)
    {
	if(RightHandInfo.showingHand != showing)
	    ConfigureRightHandNew (0.0,0.0,0,showing);
    }
}


// CPU interface functions

static unsigned int CLines;
static gboolean PLOTTERF72;

static void F72changed(unsigned int value)
{
    //printf("%s %u\n",__FUNCTION__,value);
    if(value == 1)
    {
	if((CLines & 7168) == 7168)
	{
	    PLOTTERF72 = TRUE;
	    if(CPU_word_time_count >= plotterBusyUntil)
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
static void ACTchanged(unsigned int value)
{
    if(value == 1)
    {
	if(PLOTTERF72)
	{
	    

	    if(CLines & 1)
	    {
		PenX += 1;
		if(PenX > 1100) PenX = 1100;
	    }
	    if(CLines & 2)
	    {
		PenX -= 1;
		if(PenX < 0) PenX = 0;
	    }
	    if(CLines & 4)
	    {
		PenY -= 1;
	    }
	    if(CLines & 8)
	    {
		PenY += 1;
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
		//printf("Pen at %d,%d\n",PenX,PenY);
		cairo_set_source_rgba(drumSurfaceCr,0.0,0.0,0.0,1.0);
		cairo_set_line_width (drumSurfaceCr, 1);
		cairo_set_line_cap  (drumSurfaceCr, CAIRO_LINE_CAP_ROUND);
		cairo_move_to (drumSurfaceCr, 32.5+(PenX/2),0.5+(PenY/2));
		cairo_line_to (drumSurfaceCr, 32.5+(PenX/2),0.5+(PenY/2));
		cairo_stroke (drumSurfaceCr);


		// Duplicate at bottom
		if((PenY/2)<=LINES_VISIBLE)
		{
		    cairo_move_to (drumSurfaceCr, 32.5+(PenX/2),0.5+(PenY/2)+DRUM_HIGH);
		    cairo_line_to (drumSurfaceCr, 32.5+(PenX/2),0.5+(PenY/2)+DRUM_HIGH);
		    cairo_stroke (drumSurfaceCr);


		}


		
	    }
	    //printf("(%x,%d)%d\n",PenX,PenY,PenY/2);
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


static void powerKnobHandler(int state)
{
    g_info("state = %d\n",state);
}
    
static void carriageSingleKnobHandler(int state)
{
    g_info("state = %d\n",state);
}


static void DrumFastKnobHandler(int state)
{
    drumFastMove = state -1;
}


static const char *knobPngFileNames[] =
{
    "CalcompDownLeft.png","CalcompLeft.png","CalcompUpLeft.png",
    "CalcompUp.png",
    "CalcompUpRight.png","CalcompDownRight.png",
    "CalcompManualInOff.png","CalcompManualInOn.png","CalcompManualOut.png"
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
    g_string_printf(fileName,"%sgraphics/calcomp.xpm",sharedPath->str);
    
    background_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
    
    width = gdk_pixbuf_get_width(background_pixbuf);
    height = gdk_pixbuf_get_height(background_pixbuf);

    gtk_window_set_default_size(GTK_WINDOW(PlotterWindow), width, height);

    g_string_printf(fileName,"%sgraphics/carriage.png",sharedPath->str);
    
    carriage_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);


    

    for(n=0;n<9;n++)
    {
	g_string_printf(fileName,"%sgraphics/%s",sharedPath->str,knobPngFileNames[n]);
	knobPixbufs[n] = gdk_pixbuf_new_from_file(fileName->str,&error);

	if(error != NULL)
	    g_error("Failed to read image file %s due to %s\n",fileName->str,error->message);
    }

    knobNumber = 0;

/* POWER knob*/
    knob = &knobs[knobNumber];

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

    knobNumber += 1;







    
    knobCount = knobNumber;

    PenX = 100;
    PenY = (PAPER_TOP+PAPER_HIGH);    // 1660 puts pen at 830 
    PenDown = TRUE;

    g_string_free(fileName,TRUE);
    fileName = NULL;
    
    
    connectWires(F72, F72changed);
    connectWires(ACT, ACTchanged);
    connectWires(CLINES,ClinesChanged);

    gtk_widget_show(PlotterWindow);
}

/*
  on_plotterDrawingArea_draw



*/
