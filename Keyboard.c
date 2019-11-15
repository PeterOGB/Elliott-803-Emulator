// New style hand control for 803 Emulator
// P.J.Onion Jan 2019
#define G_LOG_USE_STRUCTURED
#include <gtk/gtk.h>
#include <math.h>
#include <inttypes.h>

#include "Main.h"
#include "Hands.h"

#include "Keyboard.h"
#include "Wiring.h"
#include "wg-definitions.h"
#include "Sound.h"

#include "fsm.h"
#include "Common.h"

#define SOUNDS 1




static GtkWidget *WordGenWindow;
// WordGenDrawingArea should be static, but Hands.c uses it for now in animatiopn tests
GtkWidget *WordGenDrawingArea;
static GdkPixbuf *background_pixbuf;
static gboolean DM160sChanged = TRUE;
static double DM160Values[6] = {0.0,0.0,0.0,0.0,0.0,0.0};
static int DM160s_y[] = {159,193,226,260,293,327};
static int DM160s_x = 342;
#define DM160_WIDE 14
#define DM160_HIGH 3


static gboolean InWordGenWindow = FALSE;
static gboolean deferedMotion = FALSE;
static gdouble deferedMotionX,deferedMotionY;
static gdouble FingerPressedAtX = 0,FingerPressedAtY = 0;
static GList *pressedKeys = NULL;

//static gboolean warpToLeftHand = FALSE;
//static gboolean warpToRightHand = FALSE;

//static guint32 exitTimeStamp;

static GdkSeat *seat = NULL;


enum buttonTypes {F1RED=0,F1BLACK,N1RED,N1BLACK,F2RED,F2BLACK,N2RED,N2BLACK,
		  GUARD,POWERON,POWEROFF,OPERATEBAR,BUTTONTYPES};

static GdkPixbuf *buttonPixbufs[BUTTONTYPES][2];

static const char *gifFileNames[] = 
{
    "F1Red","F1Black",
    "N1Red","N1Black",
    "F2Red","F2Black",
    "N2Red","N2Black",
    "Reset","PowerOn",
    "PowerOff","Operate"
};

static struct buttonInfo
{
    int state;
    int buttonType;
    int rowId;
    unsigned int value;
    int width,height;
    int active;
    //GtkWidget *drawingArea;
    GdkPixbuf *pixbufUp;
    GdkPixbuf *pixbufDown;
    struct buttonInfo *resetButton;
    struct fsmtable *buttonFSM;
    int fsmState;
    struct sndEffect *sndEffects[10];
    int xpos,ypos;
    gboolean changed;
    enum WiringEvent wire;

} buttons[55];     /* There are 55 buttons on the Keyboard */

static int buttonCount = 0;  /* Actual number of buttons created may be less than 55 
				during testing */

static const char *rowNames[ROWCOUNT] = 
{
    "F1","N1","F2","N2","Operate","Read-Normal-Obey","Clear_Store",
    "Manual_Data","Reset","Battery_Off","Battery_On","Computer_Off",
    "Computer_On","Selected_Stop"
}; 


#define WG_F1_BUTTON_WIDE 12
#define WG_F1_BUTTON_HIGH 13
#define WG_F1_BUTTON_SPACE 13

#define WG_N1_BUTTON_WIDE 13
#define WG_N1_BUTTON_HIGH 13
#define WG_N1_BUTTON_SPACE 14


#define WG_F2_BUTTON_WIDE 13
#define WG_F2_BUTTON_HIGH 13
#define WG_F2_BUTTON_SPACE 14

#define WG_N2_BUTTON_WIDE 13
#define WG_N2_BUTTON_HIGH 15
#define WG_N2_BUTTON_SPACE 15

#define WG_RESET_WIDE 22
#define WG_RESET_HIGH 22

#define WG_POWER_WIDE 22
#define WG_POWER_HIGH 16
  
#define WG_BAT_ON_X 417
#define WG_BAT_OFF_X 451
#define WG_E803_ON_X  534
#define WG_E803_OFF_X 568
#define WG_POWER_Y   146
#define WG_OPER_WIDE 113
#define WG_OPER_HIGH 18

/* Definitions for button state machines */
enum {WG_PRESS=1,WG_RELEASE,RR_PRESS,RR_RELEASE};
enum actions {FREE=0,LATCH,TOGGLE,RADIO};
#define BUTTON_UP 0
#define BUTTON_DOWN 1

static int buttonsReleased;
static unsigned int rowValues[ROWCOUNT];

static enum WiringEvent rowToWires[] = {F1WIRES,N1WIRES,F2WIRES,N2WIRES,OPERATEWIRE,RONWIRES,
					CSWIRE,MDWIRE,RESETWIRE,
					BATTERY_ON_PRESSED,BATTERY_OFF_PRESSED,
					COMPUTER_ON_PRESSED,COMPUTER_OFF_PRESSED,
					SSWIRE};


static GdkRGBA LampShades[2];
static gboolean lampOn = FALSE;
static gboolean lampChanged = TRUE;     // Set TRUE so they get drawn on the 
static gboolean volumeChanged = TRUE;   // fisrt call to on_WordGenDrawingArea_draw
static int volumeY = 21;
static int deltaVolume = 0;

static void doButtonFSM(struct buttonInfo *button, int event);
static gboolean startSndEffect(struct sndEffect *effect);

// Prototypes of handlers used in glade file
__attribute__((used)) 
gboolean
on_WordGenWindow_delete_event(__attribute__((unused)) GtkWidget *widget,
			      __attribute__((unused)) gpointer data);

__attribute__((used))
gboolean
on_WordGenDrawingArea_enter_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
				    __attribute__((unused)) GdkEventCrossing *event,
				    __attribute__((unused)) gpointer data);

__attribute__((used))
gboolean
on_WordGenDrawingArea_leave_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
				    __attribute__((unused)) GdkEventCrossing *event,
				    __attribute__((unused)) gpointer data);

__attribute__((used))
gboolean
on_WordGenDrawingArea_button_press_event(__attribute__((unused)) GtkWidget *drawingArea,
					 __attribute__((unused)) GdkEventButton *event,
					 __attribute__((unused)) gpointer data);


__attribute__((used)) 
gboolean
on_WordGenDrawingArea_button_release_event(__attribute__((unused)) GtkWidget *drawingArea,
					   __attribute__((unused)) GdkEventButton *event,
					   __attribute__((unused)) gpointer data);

static void on_Hand_motion_event(HandInfo *MovingHand);


__attribute__((used)) 
gboolean
on_WordGenDrawingArea_motion_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
					  __attribute__((unused)) GdkEventMotion *event,
					  __attribute__((unused)) gpointer data);
__attribute__((used))
gboolean
on_WordGenDrawingArea_draw( __attribute__((unused)) GtkWidget *drawingArea,
			    __attribute__((unused)) cairo_t *cr,
			    __attribute__((unused)) gpointer data);


__attribute__((used))
gboolean
on_WordGenDrawingArea_key_press_event(__attribute__((unused))GtkWidget *widget,
				 GdkEventKey *event,
				 __attribute__((unused)) gpointer data);

__attribute__((used))
gboolean
on_WordGenDrawingArea_key_release_event(__attribute__((unused))GtkWidget *widget,
				 GdkEventKey *event,
				   __attribute__((unused)) gpointer data);

__attribute__((used)) 
gboolean
on_WordGenWindow_delete_event(__attribute__((unused)) GtkWidget *widget,
			   __attribute__((unused)) gpointer data)
{
//    gtk_main_quit();
    return TRUE;
}

extern void
stackTrace(void);

static 
void warpToFinger(GdkWindow *win,HandInfo *hand);





/*
This now renders all the buttons into the background_pixbuf.
The only things draw in the foregrond are the two hands.
 */
static GdkPoint LampPoints[] = { {17,2} , {475,2}, {491,54}, {1,54} };
static GdkPoint VolumePoints[] = { {0,0} , {10,0}, {17,40}, {7,40} };

__attribute__((used))
gboolean
on_WordGenDrawingArea_draw( __attribute__((unused)) GtkWidget *drawingArea,
			    __attribute__((unused)) cairo_t *cr,
			    __attribute__((unused)) gpointer data)
{
    static gboolean firstCall = TRUE;
    static cairo_surface_t *surface = NULL;
    static cairo_t *surfaceCr = NULL;
    GtkAllocation  DrawingAreaAlloc;
    struct buttonInfo *bp;

    if(firstCall)
    {
	firstCall = FALSE;
	gtk_widget_get_allocation(drawingArea, &DrawingAreaAlloc);

	// Create a surface and an associated context
	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			   DrawingAreaAlloc.width,DrawingAreaAlloc.height);

	surfaceCr = cairo_create(surface);

	gdk_cairo_set_source_pixbuf (surfaceCr, background_pixbuf ,0.0,0);
	cairo_paint(surfaceCr);
    }

    // Check for any buttons that had their "changed" flag set and redraw them into the background
    for(int buttonNumber = 0; buttonNumber < buttonCount; buttonNumber += 1)
    {
	bp = &buttons[buttonNumber];
	if(bp->changed)
	{
	    bp->changed = FALSE;
	    if(bp->state)
	    {
		gdk_cairo_set_source_pixbuf (surfaceCr, buttonPixbufs[bp->buttonType][1] ,bp->xpos,bp->ypos);
	    }
	    else
	    {
		gdk_cairo_set_source_pixbuf (surfaceCr, buttonPixbufs[bp->buttonType][0] ,bp->xpos,bp->ypos);
	    }
	    cairo_paint (surfaceCr);
	}
    }

    if(lampChanged)
    {
	int n;
	cairo_set_source_rgba(surfaceCr,0.0,0.0,0.0,1.0);
	cairo_set_line_width(surfaceCr,1);

	for(n = 0;n < 4; n++)
	{
	    cairo_line_to(surfaceCr,98+LampPoints[n].x,21+LampPoints[n].y);
	}
	
	cairo_close_path(surfaceCr);
	cairo_stroke_preserve(surfaceCr);
	n = lampOn ? 1 : 0;
	gdk_cairo_set_source_rgba(surfaceCr,&LampShades[n]);
	cairo_fill(surfaceCr);

	lampChanged = FALSE;
    }

    if(DM160sChanged)
    {
	for(int DM160=0;DM160<6;DM160+=1)
	{
	    cairo_set_source_rgba(surfaceCr,0.0,DM160Values[DM160],0.0,1.0);
	    cairo_rectangle (surfaceCr,DM160s_x, DM160s_y[DM160], DM160_WIDE,DM160_HIGH);
	    cairo_fill (surfaceCr);
	}
	DM160sChanged = FALSE;
    }



    
    if(volumeChanged)
    {
	int n;
	gdouble x,y;
	cairo_set_source_rgba(surfaceCr,0.0,0.0,0.0,1.0);
	cairo_set_line_width(surfaceCr,2);

	for(n = 0;n < 4; n++)
	{
	    cairo_line_to(surfaceCr,550+VolumePoints[n].x,210+VolumePoints[n].y);
	}
	
	cairo_close_path(surfaceCr);
	cairo_stroke_preserve(surfaceCr);
	cairo_set_source_rgba(surfaceCr,1.0,1.0,1.0,1.0);
	
	cairo_fill(surfaceCr);

	x = 550.0 + (7.0*(volumeY + deltaVolume))/40.0;
	y = 210.0 + volumeY + deltaVolume; 


	cairo_set_source_rgba(surfaceCr,0.0,0.0,0.0,1.0);
	cairo_move_to(surfaceCr,x,y);
	cairo_line_to(surfaceCr,x+10.0,y);

	cairo_stroke(surfaceCr);
	

	volumeChanged = FALSE;
    }
    

    cairo_set_source_surface (cr, surface ,0.0,0);
    cairo_paint(cr);
   
    if(InWordGenWindow)
	DrawHandsNew(cr);
    
    return FALSE;
}



static gdouble LeftHandExitedAtX,LeftHandExitedAtY;
static gdouble RightHandExitedAtX,RightHandExitedAtY;
//static gdouble RightHandExitedState,RightHandExitedState;


//static HandInfo *exitedHand;


static int
KeyboardWindowEnterHandler(__attribute__((unused)) int s,
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
    
    SetActiveWindow(WORDGENWINDOW);

    wide = gtk_widget_get_allocated_width(drawingArea);

    if(EnteredAtX < wide/2)
    {
	ConfigureRightHandNew(400.0,250.0,SET_TARGETXY|SET_FINGERXY,HAND_EMPTY);
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
	SetActiveWindow(WORDGENWINDOW);
    }
    else
    {
	ConfigureLeftHandNew (100.0,250.0,SET_TARGETXY|SET_FINGERXY,HAND_EMPTY);
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
	SetActiveWindow(WORDGENWINDOW);
    }


    
    // Stop hand being released immediatly after entering the window.
    setEnterDelay(100);

    LeftHandInfo.gotoRestingWhenIdle = FALSE;
    RightHandInfo.gotoRestingWhenIdle = FALSE;
    
    InWordGenWindow = TRUE;

    // if the entry was defered, check where the cursor has moved to since it entered the window
    if(deferedMotion)
    {
	gdouble hx,hy;
	updateHands(deferedMotionX,deferedMotionY,&hx,&hy);

	
	gtk_widget_queue_draw(WordGenDrawingArea);
	
	deferedMotion = FALSE;
    }

     // Remove cursor     {
    savedCursor = gdk_window_get_cursor(wep->window);
    gdk_window_set_cursor(wep->window,blankCursor);
    

    register_hand_motion_callback(on_Hand_motion_event);
    
    gtk_widget_queue_draw(WordGenDrawingArea);
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
KeyboardWinowLeaveHandler(__attribute__((unused)) int s,
			  __attribute__((unused)) int e,
			  void *p)
{
    struct WindowEvent *wep  = (struct WindowEvent *) p;

    register_hand_motion_callback(NULL);

    getTrackingXY2(&LeftHandInfo,&LeftHandExitedAtX,&LeftHandExitedAtY);
    getTrackingXY2(&RightHandInfo,&RightHandExitedAtX,&RightHandExitedAtY);
    
    SetActiveWindow(NOWINDOW);
    InWordGenWindow = FALSE;

    gdk_window_set_cursor(wep->window,savedCursor);
    
    gtk_widget_queue_draw(WordGenDrawingArea);

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
KeyboardWindowUnconstrainedHandler(__attribute__((unused)) int s,
				   __attribute__((unused)) int e,
				   void *p)
{
    struct WindowEvent *wep  = (struct WindowEvent *) p;

    warpToFinger(wep->window,(HandInfo *)wep->data);
    

    return(-1);
}

/*
static int
KeyboardWindowConstrainedHandler(__attribute__((unused)) int s,
				   __attribute__((unused)) int e,
				   void *p)
{
    HandInfo *movingHand;
    movingHand = (HandInfo *) p;

    printf("%s called for %s\n",__FUNCTION__,movingHand->handName);

    
    return(-1);
}
*/
static struct fsmtable KeyboardWindowTable[] = {
    {OUTSIDE_WINDOW,           FSM_ENTER,           INSIDE_WINDOW,            KeyboardWindowEnterHandler},
    
    {INSIDE_WINDOW,            FSM_LEAVE,           OUTSIDE_WINDOW,           KeyboardWinowLeaveHandler},
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
    

    {-1,-1,-1,NULL}
}; 


static struct fsm KeyboardWindowFSM = { "Keyboard Window FSM",0, KeyboardWindowTable ,
			 WindowFSMStateNames,WindowFSMEventNames,0,-1};



// The INSIDE flag is used to ignore duplicate entry /leave events caused as a side effect
// of the implicte grab that occures when a button is pressed and the mouse leaves the widget
// before the button is released.  It needs to be added to all windows.

static gboolean INSIDE = FALSE;
__attribute__((used))
gboolean
on_WordGenDrawingArea_enter_notify_event(GtkWidget *drawingArea,
				    __attribute__((unused)) GdkEventCrossing *event,
				    __attribute__((unused)) gpointer data)
{
    struct WindowEvent we = {WORDGENWINDOW,event->x,event->y,event->window,(gpointer) drawingArea};

    g_info("drawingArea=%p\n",drawingArea);

    if(INSIDE)
    {
	//printf("%s DUPLICATE IGNORED\n",__FUNCTION__);
    }
    else
    {
	INSIDE = TRUE;
	doFSM(&KeyboardWindowFSM,FSM_ENTER,(void *)&we);
    }
    return TRUE;
}

__attribute__((used))
gboolean
on_WordGenDrawingArea_leave_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
				    __attribute__((unused)) GdkEventCrossing *event,
				    __attribute__((unused)) gpointer data)
{
    struct WindowEvent we = {WORDGENWINDOW,event->x,event->y,event->window,(gpointer) event};

    if(!INSIDE)
    {
	//printf("%s DUPLICATE IGNORED\n",__FUNCTION__);
    }
    else
    {
	INSIDE = FALSE;
	doFSM(&KeyboardWindowFSM,FSM_LEAVE,(void *)&we);
    }
    return TRUE;
}

__attribute__((used))
gboolean
on_WordGenDrawingArea_button_press_event(__attribute__((unused)) GtkWidget *drawingArea,
					 __attribute__((unused)) GdkEventButton *event,
					 __attribute__((unused)) gpointer data)
{
    struct buttonInfo *bp;
    int top,bottom,left,right;
    int row;
    unsigned int value;
    HandInfo *trackingHand;

    if(event->button == 3)
    {
	swapHands(drawingArea);
	return TRUE;
    }
    
    trackingHand = updateHands(event->x,event->y,&FingerPressedAtX,&FingerPressedAtY);

    // Ignore button press if tracking hand is not empty
    if( (trackingHand != NULL) && ( (trackingHand->showingHand == HAND_THREE_FINGERS) ||
				   (trackingHand->showingHand == HAND_ONE_FINGER) ||
				   (trackingHand->showingHand == HAND_ALL_FINGERS)) )
    {
	// Always show the finger down if pressed in an active area
	trackingHand->FingersPressed |= trackingHand->IndexFingerBit;
	bp = buttons;
	for(int buttonNumber = 0; buttonNumber < buttonCount; buttonNumber += 1, bp++)
	{
	
	    left = bp->xpos;
	    right = left + bp->width;
	    top = bp->ypos;
	    bottom = top + bp->height;

	    if((FingerPressedAtX >= left) && (FingerPressedAtX <= right) &&
	       (FingerPressedAtY >= top) && (FingerPressedAtY <= bottom))
	    {
		doButtonFSM(bp,WG_PRESS);

		row = (bp->rowId);
		value = rowValues[row];
	    
		if(bp->wire != 0)
		{
		    wiring(bp->wire,value);
		}
	    }
	}
    }
    return TRUE;
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


__attribute__((used)) 
gboolean
on_WordGenDrawingArea_button_release_event(__attribute__((unused)) GtkWidget *drawingArea,
					   __attribute__((unused)) GdkEventButton *event,
					   __attribute__((unused)) gpointer data)
{
    struct buttonInfo *bp;
    int top,bottom,left,right;
    int row;
    unsigned int value;
    HandInfo *trackingHand;

    if(event->button == 3)
    {
	return TRUE;
    }
    
    trackingHand = updateHands(event->x,event->y,&FingerPressedAtX,&FingerPressedAtY);

    if((trackingHand != NULL) && ( (trackingHand->showingHand == HAND_THREE_FINGERS) ||
				   (trackingHand->showingHand == HAND_ONE_FINGER) ||
				   (trackingHand->showingHand == HAND_ALL_FINGERS)))
    {
	bp = buttons;
	for(int buttonNumber = 0; buttonNumber < buttonCount; buttonNumber += 1, bp++)
	{
	    left = bp->xpos;
	    right = left + bp->width;
	    top = bp->ypos;
	    bottom = top + bp->height;

	    if((FingerPressedAtX >= left) && (FingerPressedAtX <= right) &&
	       (FingerPressedAtY >= top) && (FingerPressedAtY <= bottom))
	    {
		doButtonFSM(bp,WG_RELEASE);
	    
		row = (bp->rowId);
		value = rowValues[row];
		if(bp->wire != 0)
		{
		    wiring(bp->wire,value);
		}
	    }
	}
    }

    // Always release the finger (even if not over a button).
    if(trackingHand != NULL) trackingHand->FingersPressed &= ~trackingHand->IndexFingerBit;;
    
    if((trackingHand != NULL) && (trackingHand->handConstrained != HAND_NOT_CONSTRAINED) &&
       (trackingHand->FingersPressed == 0))
    {
	struct WindowEvent we = {WORDGENWINDOW,0,0,event->window,(gpointer) trackingHand};
	trackingHand->handConstrained = HAND_NOT_CONSTRAINED;
	doFSM(&KeyboardWindowFSM,FSM_UNCONSTRAINED,(void *)&we);
    }

    
    volumeY += deltaVolume;
    if(volumeY < 0) volumeY = 0;
    if(volumeY > 40) volumeY = 40;
    deltaVolume = 0;
    
    return TRUE;
}

static GdkRectangle ThreeFingerAreas[4] =
{
    {116,144,122,24},
    {106,198,242,24},
    {96,258,129,24},
    {84,315,241,24},
   
};

static GdkRectangle OneFingerAreas[10] =
{
    {404,135,80,35},
    {521,135,80,35},
    {529,272,88,24},
    {539,316,99,24},
    {92,143,24,24},
    {82,200,24,24},
    {73,261,24,24},
    {61,317,24,24},
    {438,314,24,24},
    {545,208,20,40}
};

static GdkRectangle AllFingersAreas[1] =
{
   {276,392,131,31} 
};




__attribute__((used)) 
gboolean
on_WordGenDrawingArea_motion_notify_event(__attribute__((unused)) GtkWidget *drawingArea,
					  __attribute__((unused)) GdkEventMotion *event,
					  __attribute__((unused)) gpointer data)
{
    //gdouble hx,hy;
    
    //printf("%s called\n",__FUNCTION__);
#if 0
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
#endif
    
    if(InWordGenWindow)
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



// Called from the timerTick in Hands.c when a hand (not the mouse cursor) moves.
static void on_Hand_motion_event(HandInfo *movingHand)
{

    int n,ix,iy;
    enum  handimages showing;
    gdouble hx,hy;
    static gboolean wasOverVolumeControl = FALSE;
    gboolean overVolumeControl;

    //printf("%s called\n",__FUNCTION__);

    //movingHand = updateHands(event->x,event->y,&hx,&hy);
    getTrackingXY2(movingHand,&hx,&hy);
    ix = (int) hx;
    iy = (int) hy;

    overVolumeControl = FALSE;

    showing = HAND_EMPTY;
    
    for(n=0;n<4;n++)
    {
	if( (ix >= ThreeFingerAreas[n].x) &&
	    (ix <= (ThreeFingerAreas[n].x+ThreeFingerAreas[n].width)) &&
	    (iy >= ThreeFingerAreas[n].y) &&
	    (iy <= (ThreeFingerAreas[n].y+ThreeFingerAreas[n].height)) )
	{
	    //printf("in threebox %d\n",n);
	    showing = HAND_THREE_FINGERS;
	    
	}
    }

    for(n=0;n<10;n++)
    {
	if( (ix >= OneFingerAreas[n].x) &&
	    (ix <= (OneFingerAreas[n].x+OneFingerAreas[n].width)) &&
	    (iy >= OneFingerAreas[n].y) &&
	    (iy <= (OneFingerAreas[n].y+OneFingerAreas[n].height)) )
	{
	    //printf("in onebox %d\n",n);
	    // kludge
	    if(n == 9)
	    {
		overVolumeControl = TRUE;
		if(movingHand->FingersPressed == movingHand->IndexFingerBit)
		{
		    deltaVolume = (int)(hy - FingerPressedAtY);
			//volumeY = iy - OneFingerAreas[n].y;
		    volumeChanged = TRUE;
		}
	    }
	    showing = HAND_ONE_FINGER;
	}
    }

    for(n=0;n<1;n++)
    {
	if( (ix >= AllFingersAreas[n].x) &&
	    (ix <= (AllFingersAreas[n].x+AllFingersAreas[n].width)) &&
	    (iy >= AllFingersAreas[n].y) &&
	    (iy <= (AllFingersAreas[n].y+AllFingersAreas[n].height)) )
	{
	    //printf("in allbox %d\n",n);
	    showing = HAND_ALL_FINGERS;
	}
    }


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

// Keypresses come via the drawing area.
__attribute__((used))
gboolean
on_WordGenDrawingArea_key_press_event(__attribute__((unused))GtkWidget *widget,
				      GdkEventKey *event,
				      __attribute__((unused)) gpointer data)
{
    struct buttonInfo *bp;
    int top,bottom,left,right;
    int row,buttonIndex,fingerOffsetX;
    unsigned int value;
    HandInfo *trackingHand;

    trackingHand = getTrackingXY(&FingerPressedAtX,&FingerPressedAtY);

    // Check that tracking hand is showing three fingers 
    //if((trackingHand == NULL) || (trackingHand->showingHand != HAND_THREE_FINGERS))  return TRUE;

    if((trackingHand != NULL) && ( (trackingHand->showingHand == HAND_THREE_FINGERS) ||
				   (trackingHand->showingHand == HAND_ONE_FINGER) ||
				   (trackingHand->showingHand == HAND_ALL_FINGERS)) )
    
    
    {
    

	// Check if the key is already in the pressedKeys list
	if(g_list_find(pressedKeys,GUINT_TO_POINTER(event->keyval)) == NULL)
	{
	    // Add key to the list
	    pressedKeys = g_list_append(pressedKeys,GUINT_TO_POINTER(event->keyval));
	    //printf("%s added %u to list \n",__FUNCTION__,event->keyval);

	    if( (trackingHand->showingHand == HAND_THREE_FINGERS) ||
		(trackingHand->showingHand == HAND_ALL_FINGERS))
	    {
		// Update the fingers pressed
		switch(event->keyval)
		{
		case GDK_KEY_q:
		case GDK_KEY_Q:
		    trackingHand->FingersPressed |= 1;
		    fingerOffsetX = 0;
		    break;
		case GDK_KEY_w:
		case GDK_KEY_W:
		    trackingHand->FingersPressed |= 2;
		    fingerOffsetX = 13;
		    break;
		case GDK_KEY_e:
		case GDK_KEY_E:
		    trackingHand->FingersPressed |= 4;
		    fingerOffsetX = 26;
		    break;
		default:
		    //printf("No Match\n");
		    trackingHand = NULL;
		    break;
		}
	    }

	    else if(trackingHand->showingHand == HAND_ONE_FINGER)
	    {
		if(trackingHand == &LeftHandInfo)
		{
		    // Update the fingers pressed
		    switch(event->keyval)
		    {
		    case GDK_KEY_e:
		    case GDK_KEY_E:
			trackingHand->FingersPressed |= 4;
			fingerOffsetX = 26;
			break;
		    default:
			//printf("No Match\n");
			trackingHand = NULL;
			break;
		    }
		}
		else if(trackingHand == &RightHandInfo)
		{
		    // Update the fingers pressed
		    switch(event->keyval)
		    {
		    case GDK_KEY_q:
		    case GDK_KEY_Q:
			trackingHand->FingersPressed |= 1;
			fingerOffsetX = 0;
			break;
		    default:
			//printf("No Match\n");
			trackingHand = NULL;
			break;
		    }
		}
	    }
    
	    if(trackingHand != NULL)
	    {
		//if(trackingHand->handId == USE_LEFT_HAND)
		if(trackingHand == &LeftHandInfo)
		{
		    fingerOffsetX = fingerOffsetX -24;
		}
	    
		FingerPressedAtX += fingerOffsetX;
	    
		buttonIndex = -1;
		bp = buttons;
		for(int buttonNumber = 0; buttonNumber < buttonCount; buttonNumber += 1, bp++)
		{

		
		    left = bp->xpos;
		    right = left + bp->width;
		    top = bp->ypos;
		    bottom = top + bp->height;

		    //printf("%s (%d,%d,%d,%d)(%d,%d)\n",__FUNCTION__,
		    //       left,right,top,bottom,FingerPressedAtX,FingerPressedAtY);

		    if((FingerPressedAtX >= left) && (FingerPressedAtX <= right) &&
		       (FingerPressedAtY >= top) && (FingerPressedAtY <= bottom))
		    {
			buttonIndex = buttonNumber;
			break;
		    }
		}


		if(buttonIndex != -1)
		{
		    bp = &buttons[buttonIndex];
		    row = (bp->rowId);
		
		    doButtonFSM(bp,WG_PRESS);

		    row = (bp->rowId);
		    value = rowValues[row];
		    //wiringMulti(rowToWires[row],value);
		    if(bp->wire != 0)
		    {
			wiring(bp->wire,value);
		    }
		}
	    }
	}
    }
    return GDK_EVENT_STOP;
}

// Key release come via the drawing area.
__attribute__((used))
gboolean
on_WordGenDrawingArea_key_release_event(__attribute__((unused))GtkWidget *widget,
					GdkEventKey *event,
					__attribute__((unused)) gpointer data)
{
    struct buttonInfo *bp;
    int top,bottom,left,right;
    int row,buttonIndex,fingerOffsetX;
    unsigned int value;
    HandInfo *trackingHand;

    trackingHand = getTrackingXY(&FingerPressedAtX,&FingerPressedAtY);
    
    // Check that tracking hand is showing three fingers 
    //if((trackingHand == NULL) || (trackingHand->showingHand != HAND_THREE_FINGERS))  return TRUE;

    if((trackingHand != NULL) && ( (trackingHand->showingHand == HAND_THREE_FINGERS) ||
				   (trackingHand->showingHand == HAND_ONE_FINGER) ||
				   (trackingHand->showingHand == HAND_ALL_FINGERS)))
    
    
    {
    
	// Check if the key is already in the pressedKeys list
	if(g_list_find(pressedKeys,GUINT_TO_POINTER(event->keyval)) != NULL)
	{
	    // Remove key to the list
	    pressedKeys = g_list_remove(pressedKeys,GUINT_TO_POINTER(event->keyval));
	    //printf("%s removed %u from list \n",__FUNCTION__,event->keyval);

	    if(trackingHand != NULL)
	    {
		// Update the fingers pressed
		switch(event->keyval)
		{
		case 113:
		    trackingHand->FingersPressed &= 0x6;
		    fingerOffsetX = 0;
		    break;
		case 119:
		    trackingHand->FingersPressed &= 0x5;
		    fingerOffsetX = 13;
		    break;
		case 101:
		    trackingHand->FingersPressed &= 0x3;
		    fingerOffsetX = 26;
		    break;
		default:
		    //printf("No Match\n");
		    trackingHand = NULL;
		    break;
		}
	    }
    
 
	    if(trackingHand != NULL)
	    {
		//if(trackingHand->handId == USE_LEFT_HAND)
		if(trackingHand == &LeftHandInfo)
		{
		    fingerOffsetX = fingerOffsetX - (26);
		}
	    
		FingerPressedAtX += fingerOffsetX;
	    
		buttonIndex = -1;
		bp = buttons;
		for(int buttonNumber = 0; buttonNumber < buttonCount; buttonNumber += 1, bp++)
		{

		
		    left = bp->xpos;
		    right = left + bp->width;
		    top = bp->ypos;
		    bottom = top + bp->height;

		    //printf("%s (%d,%d,%d,%d)(%d,%d)\n",__FUNCTION__,
		    //       left,right,top,bottom,FingerPressedAtX,FingerPressedAtY);

		    if((FingerPressedAtX >= left) && (FingerPressedAtX <= right) &&
		       (FingerPressedAtY >= top) && (FingerPressedAtY <= bottom))
		    {
			buttonIndex = buttonNumber;
			break;
		    }
		}
	
		if(buttonIndex != -1)
		{
		    bp = &buttons[buttonIndex];
		    row = (bp->rowId);
		
		    doButtonFSM(bp,WG_RELEASE);

		    row = (bp->rowId);
		    value = rowValues[row];
		    //wiringMulti(rowToWires[row],value);
		    if(bp->wire != 0)
		    {
			wiring(bp->wire,value);
		    }
		}
	    }
	}
    }
    if((trackingHand != NULL) &&
       (trackingHand->handConstrained != HAND_NOT_CONSTRAINED) &&
       (trackingHand->FingersPressed == 0))
    {
	struct WindowEvent we = {WORDGENWINDOW,0,0,event->window,(gpointer) trackingHand};
	//warpToFinger( gtk_widget_get_parent_window (widget),trackingHand);
	trackingHand->handConstrained = HAND_NOT_CONSTRAINED;
	//printf("*************** %s UNCONSTRAINED *********************\n",__FUNCTION__);
	doFSM(&KeyboardWindowFSM,FSM_UNCONSTRAINED,(void *)&we);
    }

	
  
    return GDK_EVENT_STOP;
}

static gboolean MainsOn = FALSE;
static gboolean PowerOn = FALSE;

static void mainsOn(__attribute__((unused)) unsigned int dummy)
{

    MainsOn = TRUE;
    if(MainsOn && PowerOn)
    {
	lampOn = TRUE;
	lampChanged = TRUE;
	gtk_widget_queue_draw(WordGenDrawingArea);
    }    
}

static void mainsOff(__attribute__((unused)) unsigned int dummy)
{
    MainsOn = FALSE;
    lampOn = FALSE;
    lampChanged = TRUE;
    gtk_widget_queue_draw(WordGenDrawingArea);
    
}

static void powerOn(__attribute__((unused)) unsigned int dummy)
{
    PowerOn = TRUE;
    if(MainsOn && PowerOn)
    {
	lampOn = TRUE;
	lampChanged = TRUE;
	gtk_widget_queue_draw(WordGenDrawingArea);
    }   
}

static void powerOff(__attribute__((unused)) unsigned int dummy)
{
    PowerOn = FALSE;
    lampOn = FALSE;
    lampChanged = TRUE;

    gtk_widget_queue_draw(WordGenDrawingArea);
}


extern int DM160s_bright[6];
static void updateDM160s(__attribute__((unused)) unsigned int dummy)
{

    /* With a 20Hz emulation cycle, max value should be 
       1/288E-6 / 20 = 173.111
    */

    //printf("%s called\n",__FUNCTION__);
    
    // Copy values from Emualte.c and set flag
    if(PowerOn && MainsOn)
    {
	for(int n=0;n<6;n++) DM160Values[n] = DM160s_bright[n] / 174.0;
    }
    else
    {
	for(int n=0;n<6;n++) DM160Values[n] = 0.0;
    }
    DM160sChanged = TRUE;
    if(!InWordGenWindow)
    {
	//printf("%s Redraw queued\n",__FUNCTION__);
	gtk_widget_queue_draw(WordGenDrawingArea);
    }
}






// Imported Wordgenerator code

/* State machines for buttons */

static void doButtonFSM(struct buttonInfo *button, int event)
{
    int (*handler)(int,int,void *);
    int state;
    struct fsmtable *fsmEntry;
 
    fsmEntry = button->buttonFSM;
    state = button->fsmState;

    while(fsmEntry->state != -1)
    {
	if( (fsmEntry->state == state) && (fsmEntry->event == event))
	{
    	    handler = fsmEntry->handler;

	    if(handler == NULL)
	    {
		state = fsmEntry->nextState; 
	    }
	    else
	    {
		state = (handler)(state,event,(void *) button);
		if(state == -1)
		    state = fsmEntry->nextState; 
	    }
	    button->fsmState = state;
	    return;
	}
	fsmEntry++;
    }
   
    //printf("No entry in buttonFSM table for state %d event %d\n",state,event);
    
}

static void redrawButton(struct buttonInfo *button)
{
    button->changed = TRUE;
}




#if SOUNDS
static int doSnd_2(__attribute__((unused)) int state,
	    __attribute__((unused)) int event,
	    void *data)
{
  
    struct buttonInfo *button;
    button = (struct buttonInfo *) data;
 
    startSndEffect(button->sndEffects[2]);
    
    return -1;
}

static int doSnd_3(__attribute__((unused)) int state,
	    __attribute__((unused)) int event,
	    void *data)
{
  
    struct buttonInfo *button;
    button = (struct buttonInfo *) data;
    
    startSndEffect(button->sndEffects[3]);
   
    return -1;
}
#else

static int doSnd_2(__attribute__((unused)) int state,
	    __attribute__((unused)) int event,
	    __attribute__((unused)) void *data)
{
return -1;
}

static int doSnd_3(__attribute__((unused)) int state,
	    __attribute__((unused)) int event,
	    __attribute__((unused)) void *data)
{
return -1;
}



#endif


static int doRR_PRESS(__attribute__((unused)) int state,
		      __attribute__((unused)) int event,
		      void *data)
{
    int n,rowId;
    unsigned buttonValue;
    struct buttonInfo *button,*RRbutton;
    
    

    RRbutton = (struct buttonInfo *) data; 
    RRbutton->state = BUTTON_DOWN;
    redrawButton(RRbutton);
    rowId = RRbutton->rowId;

    buttonsReleased = 0;
    
    button = &buttons[0];
    for(n=0 ; n < buttonCount; n++,button++)
    {	
	if((button->rowId == rowId) && (button != RRbutton))
	{
	    doButtonFSM(button,RR_PRESS);
	}
    }

    
    if(buttonsReleased)
    {
	buttonValue = 1U << (buttonsReleased - 1);
	button = &buttons[0];
	for(n=0 ; n < buttonCount; n++,button++)
	{	
	    if((button->rowId == rowId))
	    {
		//printf("button->value=%d == %d\n",button->value,buttonValue);
	    }

	    if((button->rowId == rowId) && (button->value == buttonValue))
	    {
#if SOUNDS	    	
		startSndEffect(button->sndEffects[7]);
#endif		 
		break;

	    }
	}
    }
    else
    {
#if SOUNDS    	
	startSndEffect(RRbutton->sndEffects[4]);
#endif
    }


    return -1;
}


static int doRR_RELEASE(__attribute__((unused)) int state,
		 __attribute__((unused)) int event,
		 void *data)
{
    int n,rowId;
    struct buttonInfo *button,*RRbutton;
    
    

    RRbutton = (struct buttonInfo *) data;
    RRbutton->state = BUTTON_UP;
    redrawButton(RRbutton);
    rowId = RRbutton->rowId;


    button = &buttons[0];
    for(n=0 ; n < buttonCount; n++,button++)
    {
	if((button->rowId == rowId) && (button != RRbutton))
	{
	    doButtonFSM(button,RR_RELEASE);
	}	
	
    }
#if SOUNDS
    startSndEffect(RRbutton->sndEffects[5]);
#endif
    return -1;
}

static int doWG_PRESS(__attribute__((unused)) int state,
	       __attribute__((unused)) int event,
	       void *data)
{
    struct buttonInfo *button;
    button = (struct buttonInfo *) data;

    button->state = BUTTON_DOWN;
    rowValues[button->rowId] |= button->value;

    redrawButton(button);

#if SOUNDS    
    if(state == 0)
	startSndEffect(button->sndEffects[1]);
    if(state == 3)
	startSndEffect(button->sndEffects[4]);
#endif    
    return -1;
}


static int doWG_RELEASE(__attribute__((unused)) int state,
		 __attribute__((unused))int event,
		 void *data)
{
    struct buttonInfo *button;
    button = (struct buttonInfo *) data;

    buttonsReleased += 1;

    button->state = BUTTON_UP;
    rowValues[button->rowId] &= ~button->value;
    
    redrawButton(button);
#if SOUNDS    
    if(state == 4)
	startSndEffect(button->sndEffects[5]);
#endif    
    return -1;
}


static int doFREE_PRESS(__attribute__((unused)) int state,
		 __attribute__((unused)) int event,
		 void *data)
{
    struct buttonInfo *button;
    button = (struct buttonInfo *) data;

    button->state = BUTTON_DOWN;
    rowValues[button->rowId] |= button->value;

    redrawButton(button);
#if SOUNDS    
    startSndEffect(button->sndEffects[4]);
#endif
    
    return -1;
}

static int doFREE_RELEASE(__attribute__((unused)) int state,
		   __attribute__((unused)) int event,
		   void *data)
{
    struct buttonInfo *button;
    button = (struct buttonInfo *) data;

    //printf(">>>>>>>>>>> %s called\n",__FUNCTION__);

    button->state = BUTTON_UP;
    rowValues[button->rowId] &= ~button->value;
    
    redrawButton(button);
#if SOUNDS    
    startSndEffect(button->sndEffects[5]);
#endif
    
    return -1;
}



static int doRADIO_PRESS(__attribute__((unused)) int state,
		  __attribute__((unused)) int event,
		  void *data)
{
    int n,rowId;
    struct buttonInfo *button,*RRbutton;
    
    

    RRbutton = (struct buttonInfo *) data;
    if(RRbutton->state == BUTTON_DOWN) return -1;
    RRbutton->state = BUTTON_DOWN;
    redrawButton(RRbutton);
    rowId = RRbutton->rowId;
    rowValues[rowId] |= RRbutton->value;

    button = &buttons[0];
    for(n=0 ; n < buttonCount; n++,button++)
    {	
	if((button->rowId == rowId) && (button != RRbutton))
	{
	    button->state = BUTTON_UP;
	    rowValues[button->rowId] &= ~button->value;
	    redrawButton(button);
	}
    }
#if SOUNDS
    startSndEffect(RRbutton->sndEffects[4]);
#endif
    return -1;
}

#if SOUNDS
static int doRADIO_RELEASE(__attribute__((unused)) int state,
		    __attribute__((unused)) int event,
		    void *data)
{

    struct buttonInfo *RRbutton;
    RRbutton = (struct buttonInfo *) data;

    startSndEffect(RRbutton->sndEffects[5]);

    return -1;
}
#else
static int doRADIO_RELEASE(__attribute__((unused)) int state,
		    __attribute__((unused)) int event,
		    __attribute__((unused)) void *data)
{
    return -1;
}


#endif


struct fsmtable RRbuttonFSM[] = {

    { 0, WG_PRESS,      1, doRR_PRESS },
    
    { 1, WG_RELEASE,    0, doRR_RELEASE },    
    {-1,-1,             -1,NULL }
};

struct fsmtable WGbuttonFSM[] = {
    { 0, WG_PRESS,      1, doWG_PRESS },
    { 0, RR_PRESS,      3, NULL },

    { 1, WG_RELEASE,    2, doSnd_2 },
    // { 1, RR_PRESS,      5, NULL },
    { 1, RR_PRESS,      3, doWG_RELEASE },

    { 2, WG_PRESS,      1, doSnd_3 },
    { 2, RR_PRESS,      3, doWG_RELEASE },

    { 3, WG_PRESS,      4, doWG_PRESS },
    { 3, RR_RELEASE,    0, NULL },

    { 4, WG_RELEASE,    3, doWG_RELEASE },
    { 4, RR_RELEASE,    1, NULL },

//    { 5, WG_RELEASE,    1, NULL },
//    { 5, RR_RELEASE,    3, NULL },
    {-1,-1,             -1,NULL }

};

struct fsmtable TOGGLEbuttonFSM[] = {

    { 0, WG_PRESS,      1, doFREE_PRESS },
    { 1, WG_RELEASE,    2, doSnd_2 },
    { 2, WG_PRESS,      3, doSnd_3 },
    { 3, WG_RELEASE,    0, doFREE_RELEASE },
    {-1,-1,             -1,NULL }
};


struct fsmtable FREEbuttonFSM[] = {

    { 0, WG_PRESS,      1, doFREE_PRESS },
    { 1, WG_RELEASE,    0, doFREE_RELEASE },    
    {-1,-1,             -1,NULL }
};

struct fsmtable RADIObuttonFSM[] = {

    { 0, WG_PRESS,      0, doRADIO_PRESS },
    { 0, WG_RELEASE,    0, doRADIO_RELEASE },
    {-1,-1,             -1,NULL }
};


// BUTTON SOUNDS 

static GSList *runingSndEffects = NULL;
GString *SoundEffectsDirectory = NULL;

static void loadSndEffects(struct buttonInfo *button)
{
    int n;
    GString *fileName = NULL;

    fileName = g_string_new("");
   	 
    for(n=1; n <= 7; n++)
    {
	g_string_printf(fileName,"%s%s.%d.%d.wav",
			SoundEffectsDirectory->str,
			rowNames[button->rowId],button->value,n);

	//printf("readWavData(%s)\n",fileName->str);
	button->sndEffects[n] = readWavData(fileName->str);
    }
    g_string_free(fileName,TRUE);
}

static gboolean startSndEffect(struct sndEffect *effect)
{
    struct sndEffect *playing;

    if(effect == NULL) return FALSE;

    playing = (struct sndEffect *) malloc(sizeof(struct sndEffect));

    *playing = *effect;

    runingSndEffects = g_slist_append(runingSndEffects,playing);

    return TRUE;
}


static void keyboardSoundFunc(void *buffer, int sampleCount, 
			      __attribute__((unused)) double time, 
			      __attribute__((unused)) int wordtimes)
{
    GSList *effects,*next;
    struct sndEffect *effect;
    gint16 *dst,*src,sample;
    int n;

    effects = runingSndEffects; 

    while(effects != NULL )
    {
	effect = (struct sndEffect *) effects->data;

	if(effect->frameCount >= sampleCount)
	{
	    //printf("len=%d\n",effect->frameCount);
	    dst = buffer;
	    src = effect->frames;
	    n = sampleCount;
	    while(n--)
	    {
		sample = *src++;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
		*dst++ +=  sample;
		*dst++ +=  sample;
#pragma GCC diagnostic pop
	    }
			    
	    // Update pointer adn counter
	    effect->frameCount -= sampleCount;
	    effect->frames =  src;
	    effects = g_slist_next(effects);
	}
	else
	{
	    //printf("LEN=%d\n",effect->frameCount);
	    dst = buffer;
	    src = effect->frames;
	    n = effect->frameCount;
	    while(n--)
	    {
		sample = *src++;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
		*dst++ +=  sample;
		*dst++ +=  sample;
#pragma GCC diagnostic pop
	    }
	    // Effect finished so remove from list
	    next = g_slist_next(effects);
	    runingSndEffects = g_slist_remove_link(runingSndEffects,effects);
	    g_slist_free_1(effects);
	    free(effect);
	    effects = next;
	}
    }
}





void WordGenTidy(GString *userPath)
{
    GString *configText;
    int windowXpos,windowYpos;
    int buttonNumber;

    //printf("%s called\n",__FUNCTION__);

    configText = g_string_new("# Word Generator/Console  configuration\n");

    gtk_window_get_position(GTK_WINDOW(WordGenWindow), &windowXpos, &windowYpos);
    g_string_append_printf(configText,"WindowPosition %d %d\n",windowXpos,windowYpos);

    for(buttonNumber = 0; buttonNumber < buttonCount; buttonNumber += 1)
    {
	g_string_append_printf(configText,"ButtonState %d %d %d\n",
			       buttonNumber,
			       buttons[buttonNumber].state,
			       buttons[buttonNumber].fsmState);
    }

    g_string_append_printf(configText,"Volume %d\n",volumeY);


    updateConfigFile("KeyboardState",userPath,configText);

    g_string_free(configText,TRUE);


    gtk_widget_destroy(WordGenWindow);

}





static int savedWindowPositionHandler(int nn)
{
    int windowXpos,windowYpos;
    windowXpos = atoi(getField(nn+1));
    windowYpos = atoi(getField(nn+2));
    gtk_window_move(GTK_WINDOW(WordGenWindow),windowXpos,windowYpos);
    return TRUE;
}

static int savedButtonHandler(int nn)
{
    int buttonNumber,state,fsmState;
    struct buttonInfo *button;

    buttonNumber = atoi(getField(nn+1));
    state = atoi(getField(nn+2));
    fsmState = atoi(getField(nn+3));

    button = &buttons[buttonNumber];
    button->state = state;
    button->fsmState = fsmState;

    if(state == 0)
    {
    	rowValues[button->rowId] &= ~button->value;
    }
    else
    {
    	rowValues[button->rowId] |= button->value;
    }
    return 0;
}


static int savedVolumeHandler(int nn)
{
    volumeY = atoi(getField(nn+1));
    // TEMPREMOVE setCPUVolume((int16_t) (400 * volumeY));
    return 0;
}




static Token savedStateTokens[] = {
    {"WindowPosition",0,savedWindowPositionHandler},
    {"ButtonState",0,savedButtonHandler},
    {"Volume",0,savedVolumeHandler},
    {NULL,0,NULL}
};


__attribute__((used))
void WordGenInit(GtkBuilder *builder,
		 GString *sharedPath,
		 GString *userPath)
{
    GString *fileName = NULL;
    int width,height;
    struct buttonInfo *button,*reset;
    int xpos,ypos,buttonNumber;
    GError *error;
    
    WordGenWindow = GTK_WIDGET(gtk_builder_get_object_checked (builder, "WordGenWindow"));
    WordGenDrawingArea = GTK_WIDGET(gtk_builder_get_object_checked(builder,"WordGenDrawingArea"));
    
    fileName = g_string_new(NULL);
    g_string_printf(fileName,"%sgraphics/wg.xpm",sharedPath->str);
    
    background_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
    
    width = gdk_pixbuf_get_width(background_pixbuf);
    height = gdk_pixbuf_get_height(background_pixbuf);

    gtk_window_set_default_size(GTK_WINDOW(WordGenWindow), width, height);


    for(int n = 0; n < BUTTONTYPES; n++)
    {
	error = NULL;
	g_string_printf(fileName,"%sgraphics/%sDown.gif",sharedPath->str,gifFileNames[n]);
	buttonPixbufs[n][1] = gdk_pixbuf_new_from_file(fileName->str,&error);
	//printf("Reading %s %p\n",fileName->str,buttonPixbufs[n][1]);

	if(error != NULL)
	    g_error("Failed to read image file %s due to %s\n",fileName->str,error->message);

	
	error = NULL;
	g_string_printf(fileName,"%sgraphics/%sUp.gif",sharedPath->str,gifFileNames[n]);
	buttonPixbufs[n][0] = gdk_pixbuf_new_from_file(fileName->str,&error);
	//printf("Reading %s %p\n",fileName->str,buttonPixbufs[n][0]);

	if(error != NULL)
	    g_error("Failed to read image file %s due to %s\n",fileName->str,error->message);
    }
   
    g_string_free(fileName,TRUE);
    fileName = NULL;

    SoundEffectsDirectory = g_string_new(sharedPath->str);
    g_string_append(SoundEffectsDirectory,"sounds/");

    
    buttonNumber = 0;

    xpos = 100;
    ypos = 148;


  /* F1 Reset button */
    reset = button = &buttons[buttonNumber];
    
    button->xpos = xpos; button->ypos = ypos;
    button->width = WG_F1_BUTTON_WIDE;
    button->height = WG_F1_BUTTON_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = F1RED;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];
    button->rowId =  F1;
    button->value = 0;
    //button->active = FALSE;
    button->resetButton = NULL;
    button->buttonFSM = RRbuttonFSM;
    button->changed = TRUE;
    button->wire = F1WIRES;
    loadSndEffects(button);

    buttonNumber += 1;

    xpos += 40-13;
    /* F1 Row */
    for(unsigned int value = 32; value != 0; value >>= 1)
    {
	button = &buttons[buttonNumber];

	button->xpos = xpos; button->ypos = ypos;
	button->width = WG_F1_BUTTON_WIDE;
	button->height = WG_F1_BUTTON_HIGH;
	
	button->state = button->fsmState = 0;
	button->buttonType = F1BLACK;
	button->pixbufUp   = buttonPixbufs[button->buttonType][0];
	button->pixbufDown = buttonPixbufs[button->buttonType][1];
	button->rowId = F1;
	button->value = value;
	button->resetButton = reset;
	button->buttonFSM = WGbuttonFSM;
	button->changed = TRUE;
	button->wire = F1WIRES;
	loadSndEffects(button);
	xpos +=  WG_F1_BUTTON_SPACE;
	buttonNumber += 1;

    }
    

    /* N1 Reset */
    xpos = 104 - WG_N1_BUTTON_SPACE;
    ypos = 206;

    reset = button = &buttons[buttonNumber];

    button->xpos = xpos; button->ypos = ypos;
    button->width = WG_N1_BUTTON_WIDE;
    button->height = WG_N1_BUTTON_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = N1RED;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];
    button->rowId = N1;
    button->value = 0;
    button->resetButton = NULL;
    button->buttonFSM = RRbuttonFSM;
    button->changed = TRUE;
    button->wire = N1WIRES;
    loadSndEffects(button);
    buttonNumber += 1;
    xpos += 40-13;

   /* N1 Row */
    for(unsigned int value = 8192; value != 0; value >>= 1)
    {
	button = &buttons[buttonNumber];

	button->xpos = xpos; button->ypos = ypos;
	button->width = WG_N1_BUTTON_WIDE;
	button->height = WG_N1_BUTTON_HIGH;
	button->state = button->fsmState = 0;
	if(value == 1)
	    button->buttonType = N1RED;
	else
	    button->buttonType = N1BLACK;
	button->pixbufUp   = buttonPixbufs[button->buttonType][0];
	button->pixbufDown = buttonPixbufs[button->buttonType][1];
	button->rowId = N1;
	button->value = value;
	button->resetButton = reset;
	button->buttonFSM = WGbuttonFSM;
	button->changed = TRUE;
	button->wire = N1WIRES;
	loadSndEffects(button);
	xpos +=  WG_N1_BUTTON_SPACE;
	buttonNumber += 1;
    }

    
  /* F2 Reset */

    xpos = 94 - WG_F2_BUTTON_SPACE ;
    ypos = 267;

    reset = button = &buttons[buttonNumber];

    button->xpos = xpos; button->ypos = ypos;
    button->width = WG_F2_BUTTON_WIDE;
    button->height = WG_F2_BUTTON_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = F2RED;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];   
    button->rowId = F2;
    button->value = 0;
    button->resetButton = NULL;
    button->buttonFSM = RRbuttonFSM;
    button->changed = TRUE;
    button->wire = F2WIRES;
    loadSndEffects(button);
 
    /* F2 Row */
    buttonNumber += 1;
    xpos += 40-13;

    for(unsigned int value = 32; value != 0; value >>= 1)
    {
	button = &buttons[buttonNumber];

	button->xpos = xpos; button->ypos = ypos;
	button->width = WG_F2_BUTTON_WIDE;
	button->height = WG_F2_BUTTON_HIGH;
	button->state = button->fsmState = 0;
	button->buttonType = F2BLACK;
	button->pixbufUp   = buttonPixbufs[button->buttonType][0];
	button->pixbufDown = buttonPixbufs[button->buttonType][1];
	button->rowId = F2;
	button->value = value;
	button->resetButton = reset;
	button->buttonFSM = WGbuttonFSM;
	button->changed = TRUE;
	button->wire = F2WIRES;
	loadSndEffects(button);
	xpos +=  WG_F2_BUTTON_SPACE;
	buttonNumber += 1;
    }


    /* N2 Reset */

    xpos = 99 - (2*WG_N2_BUTTON_SPACE) ;
    ypos = 322;
  
    reset = button = &buttons[buttonNumber];

    button->xpos = xpos; button->ypos = ypos;
    button->width = WG_N2_BUTTON_WIDE;
    button->height = WG_N2_BUTTON_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = N2RED;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];   
    button->rowId = N2;
    button->value = 0;
    button->resetButton = NULL;
    button->buttonFSM = RRbuttonFSM;
    button->changed = TRUE;
    button->wire = N2WIRES;
    loadSndEffects(button);

    /* N2 Row */
    
    buttonNumber += 1;
    xpos += 40-13;

    for(unsigned int value = 4096; value != 0; value >>= 1)
    {
	button = &buttons[buttonNumber];
	button->xpos = xpos; button->ypos = ypos;
	button->width = WG_N2_BUTTON_WIDE;
	button->height = WG_N2_BUTTON_HIGH;
	button->state = button->fsmState = 0;
	button->buttonType = N2BLACK;
	button->pixbufUp   = buttonPixbufs[button->buttonType][0];
	button->pixbufDown = buttonPixbufs[button->buttonType][1];
	button->rowId = N2;
	button->value = value;
	button->resetButton = reset;
	button->buttonFSM = WGbuttonFSM;
	button->changed = TRUE;
	button->wire = N2WIRES;
	loadSndEffects(button);
	xpos +=  WG_N2_BUTTON_SPACE;
	buttonNumber += 1;

    }
/* Clear Store */

    xpos = 535;
    ypos = 273;

    button = &buttons[buttonNumber];
    button->xpos = xpos; button->ypos = ypos;
    button->width = WG_RESET_WIDE;
    button->height = WG_RESET_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = GUARD;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];
    button->rowId = CS;
    button->value = 1;
    button->resetButton = NULL;
    button->buttonFSM = TOGGLEbuttonFSM;
    button->changed = TRUE;
    button->wire = CSWIRE;
    loadSndEffects(button);
    buttonNumber += 1;

/* Manual Data */

    xpos = 570;
    ypos = 278;
  
    button = &buttons[buttonNumber];
    button->xpos = xpos; button->ypos = ypos;
    button->width = WG_F2_BUTTON_WIDE;
    button->height = WG_F2_BUTTON_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = F2BLACK;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];
    button->rowId = MD;
    button->value = 1;
    button->resetButton = NULL;
    button->buttonFSM = TOGGLEbuttonFSM;
    button->changed = TRUE;
    button->wire = MDWIRE;
    loadSndEffects(button);
    buttonNumber += 1;

/* Reset */

    xpos = 595;
    ypos = 273;
  
    button = &buttons[buttonNumber];
    button->xpos = xpos; button->ypos = ypos;
    button->width = WG_RESET_WIDE;
    button->height = WG_RESET_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = GUARD;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];
    button->rowId = RESET;
    button->value = 1;
    button->resetButton = NULL;
    button->buttonFSM = FREEbuttonFSM;
    button->changed = TRUE;
    button->wire = RESETWIRE;
    loadSndEffects(button);
    buttonNumber += 1;


/* Read Normal Obey */

    xpos = 550;
    ypos = 323;

    {
	static unsigned int buts[] = {WG_read, WG_normal, WG_obey };
	
	for(int value = 0; value < 3; value++)
	{
	    button = &buttons[buttonNumber];
	    button->xpos = xpos; button->ypos = ypos;
	    button->width = WG_N2_BUTTON_WIDE;
	    button->height = WG_N2_BUTTON_HIGH;
	    button->state = button->fsmState = 0;
	    button->buttonType = N2BLACK;
	    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
	    button->pixbufDown = buttonPixbufs[button->buttonType][1];
	    button->rowId = RON;
	    button->value = buts[value];
	    button->resetButton = NULL;
	    button->buttonFSM = RADIObuttonFSM;
	    button->changed = TRUE;
	    button->wire = RONWIRES;
	    loadSndEffects(button);

	    xpos +=  30;
	    buttonNumber += 1;

	}
    }
    
/* Selected Stop */

    xpos = 446;
    ypos = 320;

    button = &buttons[buttonNumber];
    button->xpos = xpos; button->ypos = ypos;
    button->width = WG_F2_BUTTON_WIDE;
    button->height = WG_F2_BUTTON_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = F2BLACK;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];
    button->rowId = SELSTOP;
    button->value = 1;
    button->resetButton = NULL;
    button->buttonFSM = TOGGLEbuttonFSM;
    button->changed = TRUE;
    button->wire = SSWIRE;
    loadSndEffects(button);

    buttonNumber += 1;

/* Battery On */
    xpos = WG_BAT_ON_X;
    ypos = WG_POWER_Y;

    button = &buttons[buttonNumber];
    button->xpos = xpos; button->ypos = ypos;
    button->width = WG_POWER_WIDE;
    button->height = WG_POWER_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = POWERON;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];
    button->rowId = BATON;
    button->value = 1;
    button->resetButton = NULL;
    button->buttonFSM = FREEbuttonFSM;
    button->changed = TRUE;
    button->wire = BATTERY_ON_PRESSED;
    loadSndEffects(button);

    buttonNumber += 1;

/* Battery Off */
    xpos = WG_BAT_OFF_X;
    ypos = WG_POWER_Y;
  
    button = &buttons[buttonNumber];
    button->xpos = xpos; button->ypos = ypos;
    button->width = WG_POWER_WIDE;
    button->height = WG_POWER_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = POWEROFF;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];
    button->rowId = BATOFF;
    button->value = 1;
    button->resetButton = NULL;
    button->buttonFSM = FREEbuttonFSM;
    button->changed = TRUE;
    button->wire = BATTERY_OFF_PRESSED;
    loadSndEffects(button);

    buttonNumber += 1;

/* CPU On */
    xpos = WG_E803_ON_X;
    ypos = WG_POWER_Y;
  
    button = &buttons[buttonNumber];
    button->xpos = xpos; button->ypos = ypos;
        button->width = WG_POWER_WIDE;
    button->height = WG_POWER_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = POWERON;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];
    button->rowId = CPUON;
    button->value = 1;
    button->resetButton = NULL;
    button->buttonFSM = FREEbuttonFSM;
    button->changed = TRUE;
    button->wire = COMPUTER_ON_PRESSED;
    loadSndEffects(button);
    
    buttonNumber += 1;

/* CPU Off */
    xpos = WG_E803_OFF_X;
    ypos = WG_POWER_Y;

    button = &buttons[buttonNumber];
    button->xpos = xpos; button->ypos = ypos;
    button->width = WG_POWER_WIDE;
    button->height = WG_POWER_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = POWEROFF;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];    
    button->rowId = CPUOFF;
    button->value = 1;
    button->resetButton = NULL;
    button->buttonFSM = FREEbuttonFSM;
    button->changed = TRUE;
    button->wire = COMPUTER_OFF_PRESSED;
    loadSndEffects(button);

    buttonNumber += 1;
    
/* Operate */

    xpos = 290;
    ypos = 400;
  
    button = &buttons[buttonNumber];
    button->xpos = xpos; button->ypos = ypos;
    button->width = WG_OPER_WIDE;
    button->height = WG_OPER_HIGH;
    button->state = button->fsmState = 0;
    button->buttonType = OPERATEBAR;
    button->pixbufUp   = buttonPixbufs[button->buttonType][0];
    button->pixbufDown = buttonPixbufs[button->buttonType][1];    
    button->rowId = OPERATE;
    button->value = 1;
    button->resetButton = NULL;
    button->buttonFSM = FREEbuttonFSM;
    button->changed = TRUE;
    button->wire = OPERATEWIRE;
    loadSndEffects(button);
    
    buttonNumber += 1;

/* Volume Control */

    





    
    buttonCount = buttonNumber;

    gdk_rgba_parse(&LampShades[0],"gray32");
    gdk_rgba_parse(&LampShades[1],"yellow1");

    connectWires(MAINS_SUPPLY_ON,mainsOn);
    connectWires(MAINS_SUPPLY_OFF,mainsOff);
    
    connectWires(SUPPLIES_ON, powerOn);
    connectWires(SUPPLIES_OFF,powerOff);
    connectWires(UPDATE_DISPLAYS,updateDM160s);
    
    
    gtk_window_set_deletable(GTK_WINDOW(WordGenWindow),FALSE);


    readConfigFile("KeyboardState",userPath,savedStateTokens);


    for(int row=0; row < ROWCOUNT; row += 1)
    {
	wiring(rowToWires[row],rowValues[row]);
    }


    
    addSoundHandler(keyboardSoundFunc);
    
    gtk_widget_show(WordGenWindow);
}
