#define G_LOG_USE_STRUCTURED
#include <gtk/gtk.h>

#include <math.h>
#include <assert.h>

#include "fsm.h"

#include "Hands.h"
#include "Common.h"

// TEMPREMOVE #include "HandViewer.h"

// TEMPREMOVE #include "Reader.h"
// TEMPREMOVE #include "CreedKeyboard.h"


static GdkPixbuf *RightHandEmpty_pixbuf;
static GdkPixbuf *RightHandHoldingReel_pixbuf;
static GdkPixbuf *RightHandHoldingReelThumb_pixbuf;
static GdkPixbuf *RightHandHoldingTape_pixbuf;
static GdkPixbuf *RightHandHoldingTapeFinger_pixbuf;
GdkPixbuf *RightHandHoldingReelTop_pixbuf;


//static GdkPixbuf *RightHandPressing_pixbuf;
//static GdkPixbuf *RightHandOneFingerUp_pixbuf;
//static GdkPixbuf *RightHandOneFingerDown_pixbuf;
//static GdkPixbuf *RightHandThreeFingers_pixbuf;
//static GdkPixbuf *RightHandThreeFingersUp_pixbuf;
//static GdkPixbuf *RightHandThreeFingersDown_pixbuf;
//static GdkPixbuf *RightHandEmpty_pixbuf;
//static GdkPixbuf *RightHandHoldingReel_pixbuf;
//static GdkPixbuf *RightHandHoldingReelThumb_pixbuf;
static GdkPixbuf *RightHandPressing_pixbuf;
static GdkPixbuf *RightHandOneFingerUp_pixbuf;
static GdkPixbuf *RightHandOneFingerDown_pixbuf;

static GdkPixbuf *LeftHandEmpty_pixbuf;
static GdkPixbuf *LeftHandHoldingReel_pixbuf;
static GdkPixbuf *LeftHandHoldingReelThumb_pixbuf;
static GdkPixbuf *LeftHandPressing_pixbuf;
static GdkPixbuf *LeftHandOneFingerUp_pixbuf;
static GdkPixbuf *LeftHandOneFingerDown_pixbuf;

static GdkPixbuf  *LeftHandThreeFingers_pixbufs[8];
static GdkPixbuf *RightHandThreeFingers_pixbufs[8];
GdkPixbuf *LeftHandPullingTape_pixbuf;
GdkPixbuf *LeftHandPullingTapeThumb_pixbuf;

//static gboolean Moving = FALSE;
//static gdouble startedFromX,startedFromY;
//static gdouble FilteredMouseAtX,FilteredMouseAtY;
static gboolean DropHand = FALSE;
static gboolean SwapedHands = FALSE;

static enum WindowIds activeWindowId = NOWINDOW;
GdkWindow *activeGdkWindow = NULL;

HandInfo LeftHandInfo  = {"Left Hand",0.0,0.0,0.0,0.0,0.0,0.0, HAND_NOT_SHOWN, HAND_NOT_CONSTRAINED,FALSE,FALSE,
			  0.0,0.0,0.0,0.0,NULL,0,&LeftHandFSM,FALSE,FALSE,FALSE,0.0,0,0x4,USE_LEFT_HAND,
			  &LeftHand,LINEAR,NULL,0.0};
HandInfo RightHandInfo = {"Right Hand",0.0,0.0,0.0,0.0,0.0,0.0, HAND_NOT_SHOWN, HAND_NOT_CONSTRAINED,FALSE,FALSE,
			  0.0,0.0,0.0,0.0,NULL,0,&RightHandFSM,FALSE,FALSE,FALSE,0.0,0,0x1,USE_RIGHT_HAND,
			  &RightHand,LINEAR,NULL,0.0};

static int threeBitsReversed[8] = {0,4,2,6,1,5,3,7};


Place LeftHand = {.placeType = AHAND,
		  .items = NULL,
		  .hand = &LeftHandInfo};
Place RightHand = {.placeType = AHAND,
		  .items = NULL,
		  .hand = &RightHandInfo};


const char *handImageNames[] = {"HAND_NOT_SHOWN","HAND_EMPTY","HAND_HOLDING_TAPE","HAND_SLIDING_TAPE",
			  "HAND_HOLDING_REEL","HAND_THREE_FINGERS","HAND_ONE_FINGER","HAND_ALL_FINGERS",
			  "HAND_HOLDING_REEL_TOP","HAND_PULLING_TAPE"};

#if 0
#include <execinfo.h>

#define BT_BUF_SIZE 100
void
stackTrace(void)
{
    int j, nptrs;
    void *buffer[BT_BUF_SIZE];
    char **strings;

    nptrs = backtrace(buffer, BT_BUF_SIZE);
    printf("backtrace() returned %d addresses\n", nptrs);
    
    /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
       would produce similar output to the following: */
    
    strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
	perror("backtrace_symbols");
	exit(EXIT_FAILURE);
           }
    
    for (j = 0; j < nptrs; j++)
	printf("%s\n", strings[j]);
    
    free(strings);
}
#endif






//HandInfo *HandTrackingMouse = NULL;
gdouble MouseAtX,MouseAtY;

void HandsInit(__attribute__((unused)) GtkBuilder *builder,
	       GString *sharedPath,
	       __attribute__((unused))  GString *userPath)
{
    GString *fileName;
    int handImageNumber;
    

    fileName = g_string_new(NULL);

    g_string_printf(fileName,"%shands/RightHandEmpty.png",sharedPath->str);
    RightHandEmpty_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);

/*
    g_string_printf(fileName,"%shands/RHandHoldingReel.png",sharedPath->str);
    RightHandHoldingReel_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
	

    g_string_printf(fileName,"%shands/RHandHoldingReelThumb.png",sharedPath->str);
    RightHandHoldingReelThumb_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
    

    g_string_printf(fileName,"%shands/RHandHoldingTape.png",sharedPath->str);
    RightHandHoldingTape_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);

    g_string_printf(fileName,"%shands/RHandHoldingTapeFinger.png",sharedPath->str);
    RightHandHoldingTapeFinger_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
*/

    
    //g_string_printf(fileName,"%sgraphics/RNewHand2.png",sharedPath->str);
    //g_string_printf(fileName,"hands/RHandPressing.png");
    //RightHandPressing_pixbuf =
    //gdk_pixbuf_new_from_file(fileName->str, NULL);

    //g_string_printf(fileName,"hands/RightHandThreeFingers.png");
    //RightHandThreeFingers_pixbuf =
//	gdk_pixbuf_new_from_file(fileName->str, NULL);
    
    //g_string_printf(fileName,"hands/RightHandThreeFingersUp.png");
    //RightHandThreeFingersUp_pixbuf =
//	gdk_pixbuf_new_from_file(fileName->str, NULL);

/*
    g_string_printf(fileName,"%shands/RHandHoldingReelTop.png",sharedPath->str);
    RightHandHoldingReelTop_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
*/
    g_string_printf(fileName,"%shands/RightHandOneFingerUp.png",sharedPath->str);
    RightHandOneFingerUp_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
    
    g_string_printf(fileName,"%shands/RightHandOneFingerDown.png",sharedPath->str);
    RightHandOneFingerDown_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
    
    //g_string_printf(fileName,"hands/RightHandThreeFingersDown.png");
    //RightHandThreeFingersDown_pixbuf =
    //    gdk_pixbuf_new_from_file(fileName->str, NULL);
    
    //g_string_printf(fileName,"%sgraphics/LNewHand3.png",sharedPath->str);

    g_string_printf(fileName,"%shands/LeftHandEmpty.png",sharedPath->str);
    //g_string_printf(fileName,"hands/TESTING.png");
    LeftHandEmpty_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
/*
    //g_string_printf(fileName,"%sgraphics/LNewHand4.png",sharedPath->str);
    g_string_printf(fileName,"%shands/LHandHoldingReel.png",sharedPath->str);
    LeftHandHoldingReel_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
    
    g_string_printf(fileName,"%shands/LHandHoldingReelThumb.png",sharedPath->str);
    LeftHandHoldingReelThumb_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);

    //g_string_printf(fileName,"%sgraphics/LNewHand2.png",sharedPath->str);
    g_string_printf(fileName,"%shands/LHandPressing.png",sharedPath->str);
    LeftHandPressing_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
*/
    
//    g_string_printf(fileName,"hands/LeftHandThreeFingers.png");
//    LeftHandThreeFingers_pixbuf =
//	gdk_pixbuf_new_from_file(fileName->str, NULL);
    
    g_string_printf(fileName,"%shands/LeftHandOneFingerUp.png",sharedPath->str);
    LeftHandOneFingerUp_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
    
    g_string_printf(fileName,"%shands/LeftHandOneFingerDown.png",sharedPath->str);
    LeftHandOneFingerDown_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
    
/*    
    g_string_printf(fileName,"%shands/LHandPullingTape.png",sharedPath->str);
    LeftHandPullingTape_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);

    g_string_printf(fileName,"%shands/LHandPullingTapeThumb.png",sharedPath->str);
    LeftHandPullingTapeThumb_pixbuf =
	my_gdk_pixbuf_new_from_file(fileName->str);
*/
    // Load 8 images for left hand with fingers pressed
    for(handImageNumber = 0; handImageNumber<8; handImageNumber += 1)
    {
	g_string_printf(fileName,"%shands/LeftHand%d.png",sharedPath->str,handImageNumber);
	LeftHandThreeFingers_pixbufs[handImageNumber] =
	    my_gdk_pixbuf_new_from_file(fileName->str);
    }

    // Load 8 images for right hand with fingers pressed
    for(handImageNumber = 0; handImageNumber<8; handImageNumber += 1)
    {
	g_string_printf(fileName,"%shands/RightHand%d.png",sharedPath->str,handImageNumber);
	RightHandThreeFingers_pixbufs[handImageNumber] =
	    my_gdk_pixbuf_new_from_file(fileName->str);
    }
	    
    blankCursor = gdk_cursor_new_from_name (gdk_display_get_default(),"none");
}


void SetActiveWindow(enum WindowIds Wid)
{
    activeWindowId = Wid;
}

enum WindowIds GetActiveWindow(void)
{
    return(activeWindowId);
}

void setLeftHandMode(enum  HandMode mode)
{
    LeftHandInfo.Fsm->state = mode;
}
    
void ConfigureLeftHandNew(gdouble x,gdouble y,int controlBits,
			  enum  handimages showing)
{

    //printf("%s  ",__FUNCTION__);
    //stackTrace();
    
    if(controlBits & SET_RESTINGXY)
    {
	//printf("SET_RESTINGXY ");
	LeftHandInfo.RestingX = x;
	LeftHandInfo.RestingY = y;
    }

    if(controlBits & SET_FINGERXY)
    {
	//printf("SET_FINGERXY ");
	LeftHandInfo.FingerAtX = x;
	LeftHandInfo.FingerAtY = y;
    }

    if(controlBits & SET_TARGETXY)
    {
	//printf("SET_TARGETXY ");
	LeftHandInfo.TargetX = x;
	LeftHandInfo.TargetY = y;
    }
    
    if(controlBits & SET_ANIMATEFROMXY)
    {
	//printf("SET_ANIMATEFROMXY ");
	LeftHandInfo.AnimateFromX = x;
	LeftHandInfo.AnimateFromY = y;
    }
    //printf("\n");
    if(showing != HAND_NO_CHANGE)
	LeftHandInfo.showingHand = showing;
}


void setRightHandMode(enum  HandMode mode)
{
    RightHandInfo.Fsm->state = mode;
}


void ConfigureHand(HandInfo *hand,
		   gdouble x,gdouble y,int controlBits,
		   enum  handimages showing)
{
    //printf("%s\n",__FUNCTION__);
    //stackTrace();
    if(controlBits & SET_RESTINGXY)
    {
	hand->RestingX = x;
	hand->RestingY = y;
    }

    if(controlBits & SET_FINGERXY)
    {
	hand->FingerAtX = x;
	hand->FingerAtY = y;
    }

    if(controlBits & SET_TARGETXY)
    {
	hand->TargetX = x;
	hand->TargetY = y;
    }
    
    if(controlBits & SET_ANIMATEFROMXY)
    {
	hand->AnimateFromX = x;
	hand->AnimateFromY = y;
    }
    hand->showingHand = showing;
}

void ConfigureRightHandNew(gdouble x,gdouble y,int controlBits,
			  enum  handimages showing)
{
    //printf("%s\n",__FUNCTION__);
    //stackTrace();
    if(controlBits & SET_RESTINGXY)
    {
	RightHandInfo.RestingX = x;
	RightHandInfo.RestingY = y;
    }

    if(controlBits & SET_FINGERXY)
    {
	RightHandInfo.FingerAtX = x;
	RightHandInfo.FingerAtY = y;
    }

    if(controlBits & SET_TARGETXY)
    {
	RightHandInfo.TargetX = x;
	RightHandInfo.TargetY = y;
    }
    
    if(controlBits & SET_ANIMATEFROMXY)
    {
	RightHandInfo.AnimateFromX = x;
	RightHandInfo.AnimateFromY = y;
    }
    if(showing != HAND_NO_CHANGE)
	RightHandInfo.showingHand = showing;
}






void setupHandAnimation(enum HandIds handId, gdouble Tx,gdouble Ty, gdouble transitTime)
{
    gdouble distance;
    
    switch(handId)
    {
    case USE_LEFT_HAND:
	LeftHandInfo.AnimateFromX = LeftHandInfo.FingerAtX;
	LeftHandInfo.AnimateFromY = LeftHandInfo.FingerAtY;
	LeftHandInfo.TargetX = Tx;
	LeftHandInfo.TargetY = Ty;
	LeftHandInfo.animation = LINEAR;
	LeftHandInfo.AnimationParameter = 0.0;
	distance = hypot(LeftHandInfo.FingerAtX - Tx,LeftHandInfo.FingerAtY - Ty);
	LeftHandInfo.AnimationStep = 0.01 * 1500.0 / (distance * transitTime) ;
	break;
    case USE_RIGHT_HAND:
	RightHandInfo.AnimateFromX = RightHandInfo.FingerAtX;
	RightHandInfo.AnimateFromY = RightHandInfo.FingerAtY;
	RightHandInfo.TargetX = Tx;
	RightHandInfo.TargetY = Ty;
	RightHandInfo.animation = LINEAR;
	RightHandInfo.AnimationParameter = 0.0;
	distance = hypot(RightHandInfo.FingerAtX - Tx,RightHandInfo.FingerAtY - Ty);
	RightHandInfo.AnimationStep = 0.01 * 1500.0 / (distance * transitTime) ;
	break;
    default:
	break;
    }
}

//#define TS 150.0
//#define TSBT 75.0
extern GdkRGBA tapeColours[5];

static void DrawLeftHandNew(cairo_t *cr)
{
    GdkPixbuf *HandPixbuf,*HoldingReelPixbuf,*HoldingTapePixbuf;
    gboolean drawFingers,__attribute__((unused))drawDot;
    gdouble activeX,activeY;

/******************************* LEFT HAND *********************/

    //printf("%s called %s\n",__FUNCTION__,handImageNames[LeftHandInfo.showingHand]);
    
    HandPixbuf = HoldingReelPixbuf = HoldingTapePixbuf = NULL;
    drawDot = FALSE;
    drawFingers = FALSE;
    
    switch(LeftHandInfo.showingHand)
    {
    case HAND_NO_CHANGE:
    case HAND_NOT_SHOWN:
	break;
    case HAND_EMPTY:
	HandPixbuf = LeftHandEmpty_pixbuf;
	activeX = -136.0;
	activeY = -13.0;
	drawFingers = TRUE;
	break;
    case HAND_HOLDING_REEL:
	activeX = 136.0;
	activeY = 6.0;
	HandPixbuf = LeftHandHoldingReel_pixbuf;
	HoldingReelPixbuf = LeftHandHoldingReelThumb_pixbuf;
	break;
    case HAND_SLIDING_TAPE:
	activeX = 120.0;
	activeY = 8.0;
	HandPixbuf = LeftHandPressing_pixbuf;
	break;
    case HAND_HOLDING_TAPE:
	break;
    case HAND_THREE_FINGERS:
	activeX = -136.0;
	activeY = -13.0;
	HandPixbuf = LeftHandThreeFingers_pixbufs[threeBitsReversed[
		(LeftHandInfo.FingersPressed&7)]];
	drawFingers = TRUE;
	break;
    case HAND_ONE_FINGER:
	activeX = -136.0;
	activeY = -13.0;	
	HandPixbuf = (LeftHandInfo.FingersPressed&4) ?
	    LeftHandOneFingerDown_pixbuf : LeftHandOneFingerUp_pixbuf;
	drawFingers = TRUE;
	break;
    case HAND_ALL_FINGERS:
	activeX = -136.0;
	activeY = -13.0;	
	HandPixbuf = (LeftHandInfo.FingersPressed == 0 ) ?
	    LeftHandThreeFingers_pixbufs[0] : LeftHandThreeFingers_pixbufs[7];
	drawFingers = TRUE;
	break;


/*	
    case HAND_PULLING_TAPE:
	activeX = 190.0;
	activeY = 40.0;
	HandPixbuf = LeftHandPullingTape_pixbuf;
	//HoldingReelPixbuf = LeftHandPullingTapeThumb_pixbuf;
	printf("%s *********** LEFT HAND HAND_PULLING_TAPE *******************\n",__FUNCTION__);
	break;
*/
    default:
	break;
    }

    if(HandPixbuf != NULL)
    {
	if(drawFingers)
	{

	    gdk_cairo_set_source_pixbuf (cr,
					 HandPixbuf,
					 LeftHandInfo.FingerAtX+activeX,
					 LeftHandInfo.FingerAtY+activeY);
	    cairo_paint (cr);

	}


#if 0	
	if(HoldingReelPixbuf != NULL)
	{
	    GList *tapes = NULL;
	    TapeReel *tape;
	    Item *item;
	    gdouble xOffset,yOffset;
	    
	    tapes = LeftHand.items; 

	    item = (Item *) tapes->data;
	    tape = item->aTapeReel;
		    


	    gdk_cairo_set_source_rgba(cr,&tapeColours[tape->tapeColour]);

	    xOffset = LeftHandInfo.FingerAtX;
	    yOffset = LeftHandInfo.FingerAtY;
	    //If tape is too big, move it up 
	    if(tape->fradius > 0.7)
	    {
		gdouble moveBy;

		LeftHandInfo.moveBy = moveBy = (tape->fradius - 0.7)  * TSBT;
		yOffset -= moveBy;
	    }
	    else
		LeftHandInfo.moveBy = 0.0;

	    
	    // Draw two circles 
	    cairo_move_to (cr,xOffset + (TSBT*0.1),yOffset);

	    cairo_arc(cr,xOffset,yOffset,
		      (TSBT*0.1),
		      0.0, 2*M_PI);
	    
	    cairo_line_to (cr,xOffset+(TSBT * tape->fradius),yOffset);
	    cairo_arc(cr,xOffset,yOffset,
		      TSBT * tape->fradius,
		      0.0, 2*M_PI);
	    cairo_line_to (cr,
			   xOffset+(TSBT*0.1),yOffset);
    

	    // Change fill rule so that the inner circle is not filled 
	    cairo_set_fill_rule(cr,CAIRO_FILL_RULE_EVEN_ODD);
    
	    cairo_fill(cr);
	    // Restore default fill rule
	    cairo_set_fill_rule(cr,CAIRO_FILL_RULE_WINDING);


	    // Draw the overlaping thumb
	    gdk_cairo_set_source_pixbuf (cr,
					 HoldingReelPixbuf ,
					 LeftHandInfo.FingerAtX+activeX-56.0, LeftHandInfo.FingerAtY+activeY+9.0);
	    cairo_paint (cr);
	}
#endif
    // Add a dot at the mouse cursor location when hand is constrained so that
    // the hand may be reaquired easily
#if 1	
	if(LeftHandInfo.handConstrained != HAND_NOT_CONSTRAINED)
	{
	    cairo_set_source_rgb(cr, 0.5, 0.0, 0.0);
	    cairo_move_to(cr, LeftHandInfo.TargetX-5, LeftHandInfo.TargetY-5);
	    cairo_line_to(cr, LeftHandInfo.TargetX+5, LeftHandInfo.TargetY+5);
	    cairo_move_to(cr, LeftHandInfo.TargetX+5, LeftHandInfo.TargetY-5);
	    cairo_line_to(cr, LeftHandInfo.TargetX-5, LeftHandInfo.TargetY+5);
	    cairo_stroke(cr);
	    //cairo_arc(cr, LeftHandInfo.TargetX, LeftHandInfo.TargetY, 5, 0, 2 * M_PI);
	    //cairo_fill(cr);
	}
#endif
	

    }

}






static void DrawRightHandNew(cairo_t *cr)
{
    GdkPixbuf *HandPixbuf,*HoldingReelPixbuf,*HoldingTapePixbuf;
    gboolean drawFingers,__attribute__((unused))drawDot;
    gdouble activeX,activeY;
    
/************************* RIGHT HAND *********************/

    //printf("%s called %s\n",__FUNCTION__,handImageNames[RightHandInfo.showingHand]);
    
    HandPixbuf = HoldingReelPixbuf = HoldingTapePixbuf = NULL;
    HoldingReelPixbuf = NULL;
    drawDot = FALSE;
    drawFingers = FALSE;

    switch(RightHandInfo.showingHand)
    {
    case HAND_NO_CHANGE:
    case HAND_NOT_SHOWN:
	break;
    case HAND_EMPTY:
	HandPixbuf = RightHandEmpty_pixbuf;
	activeX = -65.0;
	activeY = -13.0;
	drawFingers = TRUE;
	break;
    case HAND_HOLDING_REEL:
	activeX = 36.0;
	activeY = 6.0;
	HandPixbuf = RightHandHoldingReel_pixbuf;
	HoldingReelPixbuf = RightHandHoldingReelThumb_pixbuf;
	break;
    case HAND_SLIDING_TAPE:
	activeX = 8.0;
	activeY = 8.0;
	HandPixbuf = RightHandPressing_pixbuf;
	break;
    case HAND_HOLDING_TAPE:
	activeX = 18.0;
	activeY = 8.0;
	HandPixbuf = RightHandHoldingTape_pixbuf;
	HoldingTapePixbuf = RightHandHoldingTapeFinger_pixbuf;
	break;
    case HAND_THREE_FINGERS:
	activeX = -65.0;
	activeY = -13.0;
	HandPixbuf = RightHandThreeFingers_pixbufs[(RightHandInfo.FingersPressed&7)];
	drawFingers = TRUE;
	break;
    case HAND_ONE_FINGER:
	activeX = -65.0;
	activeY = -13.0;	
	HandPixbuf = (RightHandInfo.FingersPressed&1) ?
	    RightHandOneFingerDown_pixbuf : RightHandOneFingerUp_pixbuf;
	drawFingers = TRUE;
	break;
    case HAND_ALL_FINGERS:
	activeX = -65.0;
	activeY = -13.0;	
	HandPixbuf = (RightHandInfo.FingersPressed == 0 ) ?
	    RightHandThreeFingers_pixbufs[0] : RightHandThreeFingers_pixbufs[7];
	drawFingers = TRUE;
	break;

	
/*
  case HAND_HOLDING_REEL_TOP:
  activeX = 10.0;
  activeY = 38.0;
  HandPixbuf = RightHandHoldingReelTop_pixbuf;
  printf("%s *********** RIGHT HAND HAND_HOLDING_REEL_TOP *******************\n",__FUNCTION__);
  break;
*/
    default:
	break;
    }

    
    if(HandPixbuf != NULL)
    {
	if(drawFingers)
	{
	    gdk_cairo_set_source_pixbuf (cr,
					 HandPixbuf ,
					 RightHandInfo.FingerAtX+activeX,
					 RightHandInfo.FingerAtY+activeY);
	    cairo_paint (cr);
	}


	

#if 0
	if(HoldingTapePixbuf != NULL)
	{
	    // TEMPREMOVE GList *tapes = NULL;
	    // TEMPREMOVE TapeReel *tape;
	    // TEMPREMOVE Item *item;
	    
	    
	    // TEMPREMOVE tapes = RightHand.items; 

	    // TEMPREMOVE item = (Item *) tapes->data;
	    // TEMPREMOVE tape = item->aTapeReel;

	    // TEMPREMOVE gdk_cairo_set_source_rgba(cr,&tapeColours[tape->tapeColour]);
    
	    cairo_move_to(cr,
			  RightHandInfo.FingerAtX-50.0,
			  RightHandInfo.FingerAtY+25.0);
		
		
	    cairo_curve_to(cr,
			   RightHandInfo.FingerAtX-25.0,
			   RightHandInfo.FingerAtY+0.0,
			   RightHandInfo.FingerAtX+25.0,
			   RightHandInfo.FingerAtY+0.0,
			   RightHandInfo.FingerAtX+50.0,
			   RightHandInfo.FingerAtY+25.0);


	    cairo_line_to(cr,
			  RightHandInfo.FingerAtX+50.0-6.0,
			  RightHandInfo.FingerAtY+25.0+4.0);
	    cairo_line_to(cr,
			  RightHandInfo.FingerAtX+50.0,
			  RightHandInfo.FingerAtY+25.0+16.0);

	    cairo_curve_to(cr,
			   RightHandInfo.FingerAtX+25.0,
			   RightHandInfo.FingerAtY+0.0+16.0,
			   RightHandInfo.FingerAtX-25.0,
			   RightHandInfo.FingerAtY+0.0+16.0,
			   RightHandInfo.FingerAtX-50.0,
			   RightHandInfo.FingerAtY+25.0+16.0);

	    cairo_line_to(cr,
			  RightHandInfo.FingerAtX-50.0,
			  RightHandInfo.FingerAtY+25.0);
	    
	    cairo_fill(cr);
		

	    // Draw the overlaping finger
	    gdk_cairo_set_source_pixbuf (cr,
					 HoldingTapePixbuf ,
					 RightHandInfo.FingerAtX+activeX-28.0,
					 RightHandInfo.FingerAtY+activeY-16.0);
	    cairo_paint (cr);
	}


	// Used for hands holding tape
	if( (HoldingReelPixbuf != NULL) && (RightHandInfo.showingHand == HAND_HOLDING_REEL))
	{
	    GList *tapes = NULL;
	    TapeReel *tape;
	    Item *item;
	    gdouble xOffset,yOffset;
	    
	    tapes = RightHand.items; 

	    item = (Item *) tapes->data;
	    tape = item->aTapeReel;

	    gdk_cairo_set_source_rgba(cr,&tapeColours[tape->tapeColour]);

	    xOffset = RightHandInfo.FingerAtX;
	    yOffset = RightHandInfo.FingerAtY;
	    //If tape is too big, move it up 

	    if(tape->fradius > 0.7)
	    {
		gdouble moveBy;

		RightHandInfo.moveBy = moveBy = (tape->fradius - 0.7) * TSBT;
		yOffset -= moveBy ;
	    }
	    else
		RightHandInfo.moveBy = 0.0;
	

	    // Draw two circles 
	    cairo_move_to (cr,xOffset + (TSBT*0.1),yOffset);

	    cairo_arc(cr,xOffset,yOffset,
		      (TSBT*0.1),
		      0.0, 2*M_PI);
	    
	    cairo_line_to (cr,xOffset+(TSBT * tape->fradius),yOffset);
	    cairo_arc(cr,xOffset,yOffset,
		      TSBT * tape->fradius,
		      0.0, 2*M_PI);
	    cairo_line_to (cr,
			   xOffset+(TSBT*0.1),yOffset);
    

	    // Change fill rule so that the inner circle is not filled 
	    cairo_set_fill_rule(cr,CAIRO_FILL_RULE_EVEN_ODD);
    
	    cairo_fill(cr);
	    // Restore default fill rule
	    cairo_set_fill_rule(cr,CAIRO_FILL_RULE_WINDING);


	    // Draw the overlaping thumb
	    gdk_cairo_set_source_pixbuf (cr,
					 HoldingReelPixbuf ,
					 RightHandInfo.FingerAtX+activeX-41.0, RightHandInfo.FingerAtY+activeY+9.0);
	    cairo_paint (cr);
	}
#endif

	// Add a dot at the mouse cursor location when hand is constrained so that
	// the hand may be reaquired easily
	if(RightHandInfo.handConstrained != HAND_NOT_CONSTRAINED)
	{
	    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	    cairo_arc(cr, RightHandInfo.TargetX, RightHandInfo.TargetY, 5, 0, 2 * M_PI);
	    cairo_fill(cr);
	}

    }
}
	

#if 0
    switch(RightHandInfo.Fsm->state)
    {
    case IDLE_HAND:
	break;
    case TRACKING_HAND:
	cairo_set_source_rgb(cr, 0.69, 0.19, 0);
	drawDot = TRUE;
	break;
    case ANIMATING_HAND:
	cairo_set_source_rgb(cr, 0.0, 0.8, 0.0);
	drawDot = TRUE;
	break;Intransigence
    case MOVING_HAND:
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.8);
	drawDot = TRUE;
	break;
    }

    if(drawDot)
    {
	cairo_arc(cr, RightHandInfo.FingerAtX, RightHandInfo.FingerAtY, 10, 0, 2 * M_PI);
	cairo_fill(cr);
    }	

    if(RightHandInfo.Moving)
    {
	cairo_set_source_rgb(cr, 0.69, 0.69, 0.69);
	cairo_arc(cr, RightHandInfo.FingerAtX, RightHandInfo.FingerAtY, 5, 0, 2 * M_PI);
	cairo_fill(cr);
    }
#endif


void DrawHandsNew(cairo_t *cr)
{
    if(LeftHandInfo.Fsm->state == TRACKING_HAND)
    {
	DrawRightHandNew(cr);
	DrawLeftHandNew(cr);
    }
    else 
    {
	DrawLeftHandNew(cr);
	DrawRightHandNew(cr);
    }
    
}




// Called from on_button_press/release event handlers.
void dropHand(void)
{
    //printf("%s called\n",__FUNCTION__);
    DropHand = TRUE;
}

// Swap hands over
void swapHands(GtkWidget *drawingArea)
{
    int t,ox,oy;
    GdkWindow *win;
    
    //printf("%s called\n",__FUNCTION__);

    t = LeftHandInfo.Fsm->state;
    LeftHandInfo.Fsm->state = RightHandInfo.Fsm->state;
    RightHandInfo.Fsm->state = t;

    win = gtk_widget_get_parent_window (drawingArea);
    gdk_window_get_origin (win,&ox,&oy);
    
    // Warp cursor to left hand
    if(LeftHandInfo.Fsm->state == TRACKING_HAND)
    {
	ox += ((gint) LeftHandInfo.FingerAtX);
	oy += ((gint) LeftHandInfo.FingerAtY);
    }

    // Warp cursor to right hand
    if(RightHandInfo.Fsm->state == TRACKING_HAND)
    {
	ox += ((gint) RightHandInfo.FingerAtX);
	oy += ((gint) RightHandInfo.FingerAtY);
    }

    gdk_device_warp (gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_display_get_default())),
		     gdk_screen_get_default(),
		     ox,oy);

    
    SwapedHands = TRUE;
}
static gboolean mouseReset = TRUE;

// Call this when mouse enters a widow to reset mouse motion detection
void setMouseAtXY(gdouble x,gdouble y)
{
    MouseAtX = x;
    MouseAtY = y;
    mouseReset = TRUE;
}

HandInfo *getTrackingXY(gdouble *hx,gdouble *hy)
{
    HandInfo *tracking = NULL;
    
    if(LeftHandInfo.Fsm->state == TRACKING_HAND)
    {
	*hx = LeftHandInfo.FingerAtX;
	*hy = LeftHandInfo.FingerAtY;
	
	tracking = &LeftHandInfo;
    }

    if(RightHandInfo.Fsm->state == TRACKING_HAND)
    {
	*hx = RightHandInfo.FingerAtX;
	*hy = RightHandInfo.FingerAtY;

	tracking = &RightHandInfo;
    }

    return(tracking);
}

void getTrackingXY2(HandInfo *hand,gdouble *hx,gdouble *hy)
{
    *hx = hand->FingerAtX;
    *hy = hand->FingerAtY;
}

// This updates the hand's TargetX & TargetY but doesn't actually move the hand.
// The hand os moved in the timer tick handler in Hands.c which updates the hand's
// FingerAtX & FingerAtY.

HandInfo *updateHands(gdouble mx,gdouble my, gdouble *hx,gdouble *hy)
{
    HandInfo *tracking = NULL;
    
    MouseAtX = mx;
    MouseAtY = my;
    
    if(LeftHandInfo.Fsm->state == TRACKING_HAND)
    {
	if(LeftHandInfo.handConstrained == HAND_NOT_CONSTRAINED)
	{
	    LeftHandInfo.TargetX = mx;
	    LeftHandInfo.TargetY = my;
	}
	if(hx) *hx = LeftHandInfo.FingerAtX;
	if(hy) *hy = LeftHandInfo.FingerAtY;
	
	tracking = &LeftHandInfo;
    }

    if(RightHandInfo.Fsm->state == TRACKING_HAND)
    {
	if(RightHandInfo.handConstrained == HAND_NOT_CONSTRAINED)
	{
	    RightHandInfo.TargetX = mx;
	    RightHandInfo.TargetY = my;
	}
	if(hx) *hx = RightHandInfo.FingerAtX;
	if(hy) *hy = RightHandInfo.FingerAtY;

	tracking = &RightHandInfo;
    }

    return(tracking);
}

static int EnterDelay = 0;

void setEnterDelay(int n)
{
    EnterDelay = n;
}

//extern GtkWidget *ReaderDrawingArea;
extern GtkWidget *WordGenDrawingArea;
extern GtkWidget *ContactorDrawingArea;
//extern GtkWidget *CreedKeyboardDrawingArea;
//extern GtkWidget *DrawersDrawingArea;
//extern GtkWidget *HandViewerDrawingArea;
//extern GtkWidget *PTSDrawingArea;

static void (*Hand_motion_callback)(HandInfo *hi) = NULL;

void register_hand_motion_callback(void (*hmc)(HandInfo *hi))
{
    Hand_motion_callback = hmc;
}


gboolean timerTick(__attribute__((unused)) gpointer user_data)
{
    //static guint ticks = 0; //,startedAtTime = 0;
    //static guint filteredStillUntilTime = 0;
    //gboolean drop;
    gdouble distance,p; //,filteredDistance;
    //gdouble FingerWasAtX,FingerWasAtY;    // DONT REMOVE, WILL BE NEEDED FOR CONSTRAINTS
    //static gdouble prevMouseAtX,prevMouseAtY;
    //static gdouble prevFilteredMouseAtX,prevFilteredMouseAtY;
    static int rate = 0;
    double oldX,newX,oldY,newY;
     

    if(rate == 0)
    {
	//static int h = 0;
	//printf("%d\n",h++);
	rate = 2;
    }
    else
    {
	rate -= 1;
    }

    
#if 0
    static FILE *fp = NULL;

    if(fp == NULL)
    {
	fp = fopen("/tmp/motion","w");
    }
#endif


    // Is there anything to do here ?
    if(SwapedHands)
    {

	SwapedHands = FALSE;
    }

    
    if(mouseReset)
    {
	//prevFilteredMouseAtX = FilteredMouseAtX =
	//prevMouseAtX = MouseAtX;
	//prevFilteredMouseAtY = FilteredMouseAtY =
	//prevMouseAtY = MouseAtY;
	mouseReset = FALSE;
	//distance = 0.0;
    }
    else
    {
	
	//FilteredMouseAtX = (0.3 * MouseAtX) + (0.7 * FilteredMouseAtX);
	//FilteredMouseAtY = (0.3 * MouseAtY) + (0.7 * FilteredMouseAtY);

	//distance = hypot(prevMouseAtX - MouseAtX ,prevMouseAtY - MouseAtY);
	//filteredDistance = hypot(prevFilteredMouseAtX - FilteredMouseAtX,
	//			 prevFilteredMouseAtY - FilteredMouseAtY);

	//fprintf(fp,"%d %f %f\n",ticks,distance,filteredDistance);

	
	//prevMouseAtX = MouseAtX;
	//prevMouseAtY = MouseAtY;

	//prevFilteredMouseAtX = FilteredMouseAtX;
	//prevFilteredMouseAtY = FilteredMouseAtY;
	
    }


    
    //ticks += 1;
    if(EnterDelay) EnterDelay -= 1;
    
    // LEFT HAND

    if(LeftHandInfo.Fsm->state == TRACKING_HAND)
    {
	// Don't update FingerAt[XY] if the hand is constrained
	if(LeftHandInfo.handConstrained == HAND_NOT_CONSTRAINED)
	{
	   
	    oldX = LeftHandInfo.FingerAtX;
	    newX  = LeftHandInfo.FingerAtX = (0.3 * LeftHandInfo.TargetX) + (0.7 * LeftHandInfo.FingerAtX);
	    

	    oldY = LeftHandInfo.FingerAtY;
	    if(!LeftHandInfo.fixedY)
		newY = LeftHandInfo.FingerAtY = (0.3 * LeftHandInfo.TargetY) + (0.7 * LeftHandInfo.FingerAtY);
	    else
		newY = oldY;

	    
	}
	else
	{
	    //  If the hand is constraind, call motion handler every time so it can
	    // Un-constrain the hand.
	    if(Hand_motion_callback != NULL)
	    {
		(Hand_motion_callback)(&LeftHandInfo);
	    }
	}

	if(DropHand)
	{
	    //ConfigureLeftHandNew(startedFromX,startedFromY ,SET_FINGERXY,HAND_SLIDING_TAPE);
	    //printf("%s HAND RELEASE 1\n",__FUNCTION__);
	    gdk_window_set_cursor(activeGdkWindow,savedCursor);
	    doFSM(LeftHandInfo.Fsm,HAND_RELEASE,(void *) &LeftHandInfo);
	    DropHand = FALSE;
	    LeftHandInfo.dontAquire = TRUE;
	}
	
/*
	else
	{
	    handDistance = 0.0;
	}
*/


	// Only call the motion handler if the Hand has moved
	if(hypot(newX-oldX,newY-oldY) >= 0.1)
	{
	    //printf("X Moved by %f\n",newX-oldX);
	    if(Hand_motion_callback != NULL)
	    {
		(Hand_motion_callback)(&LeftHandInfo);
	    }
	}
	
    }


    if(LeftHandInfo.Fsm->state == MOVING_HAND)
    {
	if(LeftHandInfo.StartMovingDelay > 0)
	{
	    LeftHandInfo.StartMovingDelay -= 1;
	    //printf("%d\n",LeftHandInfo.StartMovingDelay);
	}
	else
	{
	    LeftHandInfo.FingerAtX = (0.05 * LeftHandInfo.TargetX) + (0.95 * LeftHandInfo.FingerAtX);
	    LeftHandInfo.FingerAtY = (0.05 * LeftHandInfo.TargetY) + (0.95 * LeftHandInfo.FingerAtY);
	    distance = hypot(LeftHandInfo.TargetX - LeftHandInfo.FingerAtX,LeftHandInfo.TargetY - LeftHandInfo.FingerAtY);
	    if(distance < 1.0)
	    {
		doFSM(&LeftHandFSM,HAND_ARRIVED,(void *) &LeftHandInfo);
	    }
	}
    }
    
    if(LeftHandInfo.Fsm->state == ANIMATING_HAND)
    {
	if(LeftHandInfo.animation == LINEAR)
	{
	    p = LeftHandInfo.AnimationParameter += LeftHandInfo.AnimationStep;// * 0.5;
	    LeftHandInfo.FingerAtX = (p * LeftHandInfo.TargetX) + ((1.0 - p) * LeftHandInfo.AnimateFromX);
	    LeftHandInfo.FingerAtY = (p * LeftHandInfo.TargetY) + ((1.0 - p) * LeftHandInfo.AnimateFromY);

	    //printf("%s p=%f\n",__FUNCTION__,p);
	
	    if(p >= 1.0)
	    {
		LeftHandInfo.FingerAtX = LeftHandInfo.TargetX;
		LeftHandInfo.FingerAtY = LeftHandInfo.TargetY;
	    
		//printf("%s HandTrackingMouse set to NULL 5\n",__FUNCTION__);
	    
		doFSM(&WindowFSM,FSM_END,NULL);
		doFSM(LeftHandInfo.Fsm,HAND_END,&LeftHandInfo);
	    }
	}
	else if(LeftHandInfo.animation == TAPELOAD)
	{
	    if(LeftHandInfo.AnimationCallback != NULL)
	    {
		(LeftHandInfo.AnimationCallback)(&LeftHandInfo);
	    }
	}
    }



    // RIGHT HAND

    if(RightHandInfo.Fsm->state == TRACKING_HAND)
    {
	// Don't update FingerAt[XY] if the hand is constrained
	if(RightHandInfo.handConstrained == HAND_NOT_CONSTRAINED)
	{
	   
	    oldX = RightHandInfo.FingerAtX;
	    newX  = RightHandInfo.FingerAtX = (0.3 * RightHandInfo.TargetX) + (0.7 * RightHandInfo.FingerAtX);
	    

	    oldY = RightHandInfo.FingerAtY;
	    if(!RightHandInfo.fixedY)
		newY = RightHandInfo.FingerAtY = (0.3 * RightHandInfo.TargetY) + (0.7 * RightHandInfo.FingerAtY);
	    else
		newY = oldY;

	    
	}
	else
	{
	    //  If the hand is constraind, call motion handler every time so it can
	    // Un-constrain the hand.
	    if(Hand_motion_callback != NULL)
	    {
		(Hand_motion_callback)(&RightHandInfo);
	    }
	}



/*
	
	if(activeWindowId == READERWINDOW)
	{
	    // ReaderTrackHand  deals with had constraints around the reader.
	    // TEMPREMOVEReaderTrackHand(&RightHandInfo);   
	}
	else
	{
	    RightHandInfo.FingerAtX = (0.3 * RightHandInfo.TargetX) + (0.7 * RightHandInfo.FingerAtX);
	    RightHandInfo.FingerAtY = (0.3 * RightHandInfo.TargetY) + (0.7 * RightHandInfo.FingerAtY);
	}
*/	    
	if(DropHand)
	{
	    //ConfigureRightHandNew(startedFromX,startedFromY ,SET_FINGERXY,HAND_SLIDING_TAPE);
	    //printf("%s HAND RELEASE 3\n",__FUNCTION__);
	    gdk_window_set_cursor(activeGdkWindow,savedCursor);
	    doFSM(RightHandInfo.Fsm,HAND_RELEASE,(void *) &RightHandInfo);
	    DropHand = FALSE;
	    RightHandInfo.dontAquire = TRUE;
	}	    

/*	
	else
	{
	    handDistance = 0.0;
	}
*/
	// Only call the motion handler if the Hand has moved
	if(hypot(newX-oldX,newY-oldY) >= 0.1)
	{
	    //printf("X Moved by %f\n",newX-oldX);
	    if(Hand_motion_callback != NULL)
	    {
		(Hand_motion_callback)(&RightHandInfo);
	    }
	}
	
    }	

    if(RightHandInfo.Fsm->state == MOVING_HAND)
    {
	if(RightHandInfo.StartMovingDelay > 0)
	{
	    RightHandInfo.StartMovingDelay -= 1;
	    //printf("%d\n",RightHandInfo.StartMovingDelay);
	}
	else
	{
	    RightHandInfo.FingerAtX = (0.05 * RightHandInfo.TargetX) + (0.95 * RightHandInfo.FingerAtX);
	    RightHandInfo.FingerAtY = (0.05 * RightHandInfo.TargetY) + (0.95 * RightHandInfo.FingerAtY);
	    distance = hypot(RightHandInfo.TargetX - RightHandInfo.FingerAtX,RightHandInfo.TargetY - RightHandInfo.FingerAtY);
	    if(distance < 1.0)
	    {
		doFSM(&RightHandFSM,HAND_ARRIVED,(void *) &RightHandInfo);
	    }
	}
    }

    if(RightHandInfo.Fsm->state == ANIMATING_HAND)
    {
	if(RightHandInfo.animation == LINEAR)
	{
	    p = RightHandInfo.AnimationParameter += RightHandInfo.AnimationStep;// * 0.5;
	    RightHandInfo.FingerAtX = (p * RightHandInfo.TargetX) + ((1.0 - p) * RightHandInfo.AnimateFromX);
	    RightHandInfo.FingerAtY = (p * RightHandInfo.TargetY) + ((1.0 - p) * RightHandInfo.AnimateFromY);

	    //printf("%s p=%f\n",__FUNCTION__,p);
	
	    if(p >= 1.0)
	    {
		RightHandInfo.FingerAtX = RightHandInfo.TargetX;
		RightHandInfo.FingerAtY = RightHandInfo.TargetY;
		
		//printf("%s HandTrackingMouse set to NULL 5\n",__FUNCTION__);
		
		doFSM(&WindowFSM,FSM_END,NULL);
		doFSM(RightHandInfo.Fsm,HAND_END,&RightHandInfo);
	    }
	}
	else if(RightHandInfo.animation == TAPELOAD)
	{
	    if(RightHandInfo.AnimationCallback != NULL)
	    {
		(RightHandInfo.AnimationCallback)(&RightHandInfo);
	    }
	}
    }
    if(RightHandInfo.Fsm->state == TRACKING_OTHER_HAND)
    {
	RightHandInfo.FingerAtY = LeftHandInfo.FingerAtY;
	RightHandInfo.FingerAtX = 376.0 - (((LeftHandInfo.FingerAtY - 217.0)/ (280.0-217.0)) * (384.0-370.0));

    }





    
    // WINDOWS
    
    if(activeWindowId == CREEDKEYBOARDWINDOW)
    {
//	printf("%s activeWindowId == CREEDKEYBOARDWINDOW %d %d\n",__FUNCTION__,
//	       LeftHandInfo.Fsm->state,RightHandInfo.Fsm->state);
	
	if((LeftHandInfo.Fsm->state != TRACKING_HAND) && (RightHandInfo.Fsm->state != TRACKING_HAND)) 
	{
	    double dl,dr;
	    dl = hypot(LeftHandInfo.FingerAtX-MouseAtX,LeftHandInfo.FingerAtY-MouseAtY);
	    dr = hypot(RightHandInfo.FingerAtX-MouseAtX,RightHandInfo.FingerAtY-MouseAtY);


	    //printf("%s dl=%f dr=%f\n",__FUNCTION__,dl,dr);
	    
	    // Pickup hand when mouse gets close enough.	
	    if(dl < dr)
	    {
		if(dl > 8.0)  
		{
		    LeftHandInfo.dontAquire = FALSE;
		}
		else
		{
		    if(LeftHandInfo.dontAquire == FALSE)
		    {
			gdk_window_set_cursor(activeGdkWindow,blankCursor);
			doFSM(LeftHandInfo.Fsm,HAND_AQUIRE,(void *) &LeftHandInfo);
			//printf("%s tracking LEFT HAND 3\n",__FUNCTION__);
		    }
		}
	    }
	    else
	    {
		if(dr > 8.0)  
		{
		    RightHandInfo.dontAquire = FALSE;
		}
		else
		{
		    if(RightHandInfo.dontAquire == FALSE)
		    {
			gdk_window_set_cursor(activeGdkWindow,blankCursor);
			doFSM(RightHandInfo.Fsm,HAND_AQUIRE,(void *) &RightHandInfo);
			//printf("%s tracking RIGHT HAND 3b\n",__FUNCTION__);
		    }
		}
	    }
	}
    }


    if(activeWindowId == READERWINDOW)
    {
	if((LeftHandInfo.Fsm->state != TRACKING_HAND) && (RightHandInfo.Fsm->state != TRACKING_HAND)) 
	{
	    double dl,dr;
	    dl = hypot(LeftHandInfo.FingerAtX-MouseAtX,LeftHandInfo.FingerAtY-MouseAtY);
	    dr = hypot(RightHandInfo.FingerAtX-MouseAtX,RightHandInfo.FingerAtY-MouseAtY);
	    // Pickup hand when mouse gets close enough.	
	    if(dl < dr)
	    {
		if(dl > 8.0)  
		{
		    LeftHandInfo.dontAquire = FALSE;
		}
		else
		{
		    if(LeftHandInfo.dontAquire == FALSE)
		    {
			gdk_window_set_cursor(activeGdkWindow,blankCursor);
			doFSM(LeftHandInfo.Fsm,HAND_AQUIRE,(void *) &LeftHandInfo);
			//printf("%s tracking LEFT HAND 3\n",__FUNCTION__);
		    }
		}
	    }
	    else
	    {
		if(dr > 8.0)  
		{
		    RightHandInfo.dontAquire = FALSE;
		}
		else
		{
		    if(RightHandInfo.dontAquire == FALSE)
		    {
			gdk_window_set_cursor(activeGdkWindow,blankCursor);
			doFSM(RightHandInfo.Fsm,HAND_AQUIRE,(void *) &RightHandInfo);
			//printf("%s tracking RIGHT HAND 3c\n",__FUNCTION__);
		    }
		}
	    }
	   
	}
    }

    if((activeWindowId == WORDGENWINDOW) || (activeWindowId == HANDVIEWERWINDOW) || (activeWindowId == PTSWINDOW))
    {
	if((LeftHandInfo.Fsm->state != TRACKING_HAND) && (RightHandInfo.Fsm->state != TRACKING_HAND))
	{
	    double dl,dr;
	
	    // Pickup hand when mouse gets close enough.
	    dl = hypot(LeftHandInfo.FingerAtX-MouseAtX,LeftHandInfo.FingerAtY-MouseAtY);
	    dr = hypot(RightHandInfo.FingerAtX-MouseAtX,RightHandInfo.FingerAtY-MouseAtY);
	
	    if(dl < dr)
	    {
		if(dl > 8.0)  
		{
		    LeftHandInfo.dontAquire = FALSE;
		}
		else
		{
		    if(LeftHandInfo.dontAquire == FALSE)
		    {
			gdk_window_set_cursor(activeGdkWindow,blankCursor);
			doFSM(LeftHandInfo.Fsm,HAND_AQUIRE,(void *) &LeftHandInfo);
			//printf("%s tracking LEFT HAND 3\n",__FUNCTION__);
		    }
		}
	    }
	    else
	    {
		if(dr > 8.0)  
		{
		    RightHandInfo.dontAquire = FALSE;
		}
		else
		{
		    if(RightHandInfo.dontAquire == FALSE)
		    {
			gdk_window_set_cursor(activeGdkWindow,blankCursor);
			doFSM(RightHandInfo.Fsm,HAND_AQUIRE,(void *) &RightHandInfo);
			//printf("%s tracking RIGHT HAND 3d\n",__FUNCTION__);
		    }
		}
	    }
	}
    }


    
    if(activeWindowId == CONTACTORWINDOW)
    {

	if((LeftHandInfo.Fsm->state != TRACKING_HAND) && (RightHandInfo.Fsm->state != TRACKING_HAND))
	{
	    double dl,dr;
	
	    // Pickup hand when mouse gets close enough.
	    dl = hypot(LeftHandInfo.FingerAtX-MouseAtX,LeftHandInfo.FingerAtY-MouseAtY);
	    dr = hypot(RightHandInfo.FingerAtX-MouseAtX,RightHandInfo.FingerAtY-MouseAtY);
	
	    if(dl < dr)
	    {
		if(dl > 8.0)  
		{
		    LeftHandInfo.dontAquire = FALSE;
		}
		else
		{
		    if(LeftHandInfo.dontAquire == FALSE)
		    {
			gdk_window_set_cursor(activeGdkWindow,blankCursor);
			doFSM(LeftHandInfo.Fsm,HAND_AQUIRE,(void *) &LeftHandInfo);
			//printf("%s tracking LEFT HAND 3\n",__FUNCTION__);
		    }
		}
	    }
	    else
	    {
		if(dr > 8.0)  
		{
		    RightHandInfo.dontAquire = FALSE;
		}
		else
		{
		    if(RightHandInfo.dontAquire == FALSE)
		    {
			gdk_window_set_cursor(activeGdkWindow,blankCursor);
			doFSM(RightHandInfo.Fsm,HAND_AQUIRE,(void *) &RightHandInfo);
			//printf("%s tracking RIGHT HAND 3d\n",__FUNCTION__);
		    }
		}
	    }
	}
    }

    if(activeWindowId == DRAWERSWINDOW)
    {

	if((LeftHandInfo.Fsm->state != TRACKING_HAND) && (RightHandInfo.Fsm->state != TRACKING_HAND))
	{
	    double dl,dr;
	
	    // Pickup hand when mouse gets close enough.
	    dl = hypot(LeftHandInfo.FingerAtX-MouseAtX,LeftHandInfo.FingerAtY-MouseAtY);
	    dr = hypot(RightHandInfo.FingerAtX-MouseAtX,RightHandInfo.FingerAtY-MouseAtY);
	

	    if(dl < dr)
	    {
		if(dl > 8.0)  
		{
		    LeftHandInfo.dontAquire = FALSE;
		}
		else
		{
		    if(LeftHandInfo.dontAquire == FALSE)
		    {
			gdk_window_set_cursor(activeGdkWindow,blankCursor);
			doFSM(LeftHandInfo.Fsm,HAND_AQUIRE,(void *) &LeftHandInfo);
			//printf("%s tracking LEFT HAND 3\n",__FUNCTION__);
		    }
		}
	    }
	    else
	    {
		if(dr > 8.0)  
		{
		    RightHandInfo.dontAquire = FALSE;
		}
		else
		{
		    if(RightHandInfo.dontAquire == FALSE)
		    {
			gdk_window_set_cursor(activeGdkWindow,blankCursor);
			doFSM(RightHandInfo.Fsm,HAND_AQUIRE,(void *) &RightHandInfo);
			//printf("%s tracking RIGHT HAND 3e\n",__FUNCTION__);
		    }
		}
	    }
	}
    }
if(rate == 0)
{	
    switch(activeWindowId)
    {

    case WORDGENWINDOW:
	gtk_widget_queue_draw(WordGenDrawingArea);
	break;

    case CONTACTORWINDOW:
	gtk_widget_queue_draw(ContactorDrawingArea);
	break;

	
    case READERWINDOW:
	// TEMPREMOVE gtk_widget_queue_draw(ReaderDrawingArea);
	break;
    case CREEDKEYBOARDWINDOW:
	// TEMPREMOVE gtk_widget_queue_draw(CreedKeyboardDrawingArea);
	break;


   case DRAWERSWINDOW:
       //gtk_widget_queue_draw(DrawersDrawingArea);
	break;


   case HANDVIEWERWINDOW:
       //gtk_widget_queue_draw(HandViewerDrawingArea);
	break;

   case PTSWINDOW:
       //gtk_widget_queue_draw(PTSDrawingArea);
	break;

	
    default:
	break;
    }
}


    return TRUE;
}

  




static struct WindowEvent deferedLeave;
static struct WindowEvent deferedEnter;

static int EnterHandler(__attribute__((unused)) int s,
			__attribute__((unused)) int e,
			__attribute__((unused))void *p)
{

    //struct WindowEvent *wep;

    //printf("%s called with e=%d\n",__FUNCTION__,e);

#if 0
// FIX THIS    
    if(p != NULL)
    {
	wep = (struct WindowEvent *) p;

	activeGdkWindow = wep->window;

	(wep->handler)(wep);
    }
#endif
    return -1;
}




static int LeaveWhileAnimatingHandler(__attribute__((unused)) int s,
			__attribute__((unused)) int e,
			__attribute__((unused)) void *p)
{
   
    deferedLeave = *(struct WindowEvent *) p;
        
    return -1;
}

static int EnterWhileAnimatingHandler(__attribute__((unused)) int s,
			__attribute__((unused)) int e,
			__attribute__((unused)) void *p)
{
   
    deferedEnter = *(struct WindowEvent *) p;

    if(deferedLeave.windowId == deferedEnter.windowId)
    {
	//printf("%s returning ANIMATING\n",__FUNCTION__);
	return ANIMATING_WINDOW;
    }
    else
    {
	//printf("%s returning ENTERED\n",__FUNCTION__);
	return ENTERED_WINDOW;
    }
}




static int DeferedLeaveHandler(__attribute__((unused)) int s,
			__attribute__((unused)) int e,
			__attribute__((unused)) void *p)
{
    
    //if(deferedLeave != NULL)
    {
	activeGdkWindow = NULL;
// FIX THIS	
//	(deferedLeave.handler)(&deferedLeave);
    } 
    return -1;
}

static int DeferedLeaveAndEnterHandler(__attribute__((unused)) int s,
			__attribute__((unused)) int e,
			__attribute__((unused)) void *p)
{
    
    //if(deferedLeave != NULL)
    {
	activeGdkWindow = NULL;
	// FIX THIS (deferedLeave.handler)(&deferedLeave);
	activeGdkWindow = deferedEnter.window;
	// FIX THIS (deferedEnter.handler)(&deferedEnter);
    } 
    return -1;
}

// This is the generic window fsm.  Windows can have their own versions
static struct fsmtable WindowTable[] = {
    {OUTSIDE_WINDOW,       FSM_ENTER,       INSIDE_WINDOW,          EnterHandler},

    {INSIDE_WINDOW,        FSM_LEAVE,       OUTSIDE_WINDOW,         EnterHandler},
    {INSIDE_WINDOW,        FSM_START,       ANIMATING_WINDOW,       NULL},

    {ANIMATING_WINDOW,     FSM_END,         INSIDE_WINDOW,          NULL},
    {ANIMATING_WINDOW,     FSM_LEAVE,       LEFT_WINDOW,            LeaveWhileAnimatingHandler},

    {LEFT_WINDOW,          FSM_END,         OUTSIDE_WINDOW,         DeferedLeaveHandler},
    // EnterWhileAnimatingHandler can return  ANIMATING_WINDOW or ENTERED_WINDOW
    {LEFT_WINDOW,          FSM_ENTER,       -1,                     EnterWhileAnimatingHandler},
    
    {ENTERED_WINDOW,       FSM_LEAVE,       LEFT_WINDOW,            NULL},
    {ENTERED_WINDOW,       FSM_END,         INSIDE_WINDOW,          DeferedLeaveAndEnterHandler},

    {-1,-1,-1,NULL}
}; 

const char *WindowFSMEventNames[] =
{"Enter window","Leave window","Start animation","End animation","Start sliding a tape","Stop sliding a tape",
 "Constrained","Unconstrained"};
const char *WindowFSMStateNames[]  =
{"Outside all windows","Inside a window","Animation running","Left while animating","Entered while animating",
 "Sliding a tape inside a window","Sliding a tape outside a window","Waiting for cursor warp","Constrained inside the window","Constrained outside the window" };


struct fsm WindowFSM = { "Window FSM",0, WindowTable ,
			 WindowFSMStateNames,WindowFSMEventNames,0,-1};


// Currently this only does the rigth thing for the Creed Keyboard,
static int HandAnimationEndHandler(__attribute__((unused)) int s,
				   __attribute__((unused)) int e,
				   __attribute__((unused)) void *p)
    
{
    HandInfo *theHand; //,*otherHand;
    int nextState;
    //printf("%s ----------\n",__FUNCTION__);
    theHand = (HandInfo *) p;
    //otherHand = (theHand == &LeftHandInfo) ? &RightHandInfo : &LeftHandInfo;

    theHand->TargetX = theHand->RestingX;
    theHand->TargetY = theHand->RestingY;
    theHand->StartMovingDelay = 50;
    nextState = MOVING_HAND;
    if(theHand->AnimationFinishedHandler != NULL)
    {
	nextState = (theHand->AnimationFinishedHandler)(theHand);
	theHand->AnimationFinishedHandler = NULL;
    }

    if( (LeftHandInfo.Fsm->state != TRACKING_HAND) && (RightHandInfo.Fsm->state != TRACKING_HAND) ) 
    {
	gdk_window_set_cursor(activeGdkWindow,savedCursor);


    }
    return nextState;
}

static int HandAnimationStartHandler(__attribute__((unused)) int s,
				     __attribute__((unused)) int e,
				     __attribute__((unused)) void *p)
    
{

    //printf("%s ----------\n",__FUNCTION__);

    return -1;
}

static int HandAquireHandler(__attribute__((unused)) int s,
				   __attribute__((unused)) int e,
				   __attribute__((unused)) void *p)
    
{
    HandInfo *theHand;
    //printf("%s ----------\n",__FUNCTION__);
    theHand = (HandInfo *) p;

    theHand->TargetX = theHand->FingerAtX = MouseAtX;
    theHand->TargetY = theHand->FingerAtY = MouseAtY;

    theHand->Moving = FALSE;

    
    
    
    return -1;
}

static int HandArrivedHandler(__attribute__((unused)) int s,
				   __attribute__((unused)) int e,
				   __attribute__((unused)) void *p)
    
{
    HandInfo *theHand;
    //printf("%s ----------\n",__FUNCTION__);
    theHand = (HandInfo *) p;

    theHand->FingerAtX = theHand->TargetX;
    theHand->FingerAtY = theHand->TargetY ;
    
    
    return -1;
}

static int HandReleasedHandler(__attribute__((unused)) int s,
				   __attribute__((unused)) int e,
				   __attribute__((unused)) void *p)
    
{
    HandInfo *theHand;
    //printf("%s ----------\n",__FUNCTION__);
    theHand = (HandInfo *) p;

    theHand->handConstrained = HAND_NOT_CONSTRAINED;
    
    if(theHand->gotoRestingWhenIdle == TRUE)
    {
	theHand->TargetX = theHand->RestingX;
	theHand->TargetY = theHand->RestingY;
	theHand->StartMovingDelay = 50;
    
	return MOVING_HAND;
    }

    return -1;
}


struct fsmtable HandTable[] = {
    {IDLE_HAND,          HAND_AQUIRE,      TRACKING_HAND,       HandAquireHandler},
    {IDLE_HAND,          HAND_START,       ANIMATING_HAND,      HandAnimationStartHandler},

    {TRACKING_HAND,      HAND_RELEASE,     IDLE_HAND,           HandReleasedHandler},
    {TRACKING_HAND,      HAND_START,       ANIMATING_HAND,      NULL},


    {ANIMATING_HAND,     HAND_END,         -1,                  HandAnimationEndHandler},
    {ANIMATING_HAND,     HAND_TRACK_OTHER, TRACKING_OTHER_HAND, NULL},

    {MOVING_HAND,        HAND_ARRIVED,     IDLE_HAND,           HandArrivedHandler},
    {MOVING_HAND,        HAND_START,       ANIMATING_HAND,      NULL},
    {MOVING_HAND,        HAND_AQUIRE,    TRACKING_HAND,       HandAquireHandler},

    {TRACKING_OTHER_HAND,HAND_RELEASE,     MOVING_HAND,         NULL},
    {-1,-1,-1,NULL}
}; 

static const char *HandFSMStateNames[] =
{"Hand Idle","Hand Tracking Mouse","Hand being animated","Hand moving to point","Tracking other hand"};

static const char *HandFSMEventNames[]  =
{"Hand aquired","Animation started","Hand released","Animation ended","Hand arrived","Track other hand"};


struct fsm LeftHandFSM  = { "Left Hand FSM", 0,HandTable,
			    HandFSMStateNames,HandFSMEventNames,0,-1};
struct fsm RightHandFSM = { "Right Hand FSM",0,HandTable,
			    HandFSMStateNames,HandFSMEventNames,0,-1};

//#define HandIsEmpty(HAND)  (g_list_length(HAND->items)==0)

// Should be a generic takeFromPlace ?
Item *takeFromHand(Place *hand)
{
    Item  *item;
    //TapeReel *tape;
    GList *first;


    //assert(hand->placeType == AHAND);
    assert(HandIsEmpty((*hand)) == FALSE);
        
    //printf("%s called \n",__FUNCTION__);

    first = g_list_first(hand->items);

    item = (Item *) first->data;

    //printf("%s hand holding %d items\n",__FUNCTION__, g_list_length(hand->items));
    hand->items = g_list_remove(hand->items,item);
    //printf("%s hand holding %d items\n",__FUNCTION__, g_list_length(hand->items));
    
    //printf("%s item in %d places\n",__FUNCTION__, g_list_length(item->places));
    item->places = g_list_remove(item->places,hand);
    //printf("%s item in %d places\n",__FUNCTION__, g_list_length(item->places));
    
    return(item);
}

// Should be a generic putIntoPlace ?
gboolean putIntoHand(Item *item, Place *hand)
{
    GList **tapeList;
    //HandInfo *handInfo;

    //printf("%s called \n",__FUNCTION__);

    assert(g_list_length(hand->items) == 0);
    assert(item != NULL);
    assert(hand->placeType == AHAND);

    //handInfo = hand->hand;
    
    // Needs to be set before call to on_tapeImage_expose_event
    // itemInHand = item;
    // tapeMoved = false;

    switch(item->itemType)
    {
    case TAPEREEL:
	//printf("%s TAPEREEL\n",__FUNCTION__);
        //tape = item->aTapeReel;
        item->places = g_list_append(item->places,hand);

        tapeList = &hand->items;
        *tapeList = g_list_append(*tapeList,item);

	//updateHandViewer(item,hand);
#if 0
        writing = g_string_new("Tape: ");
        g_string_append(writing,tape->writing);

        // Code from handPostDrop
        gtk_entry_set_text(tapeWriting, writing->str);
        g_string_free(writing,TRUE);

        //setCmap(tape);
        gtk_widget_queue_draw(handDrawingArea);
        //on_tapeImage_expose_event(tapeImage, NULL);

        makeTapeCursor(tape);
        setCursors(tape,FALSE);
//      gdk_window_set_cursor (gtk_widget_get_window(window),
//                             tape->handCursor);
#endif
        break;
    default:
	break;
	    
    }
    
    return TRUE;
}

