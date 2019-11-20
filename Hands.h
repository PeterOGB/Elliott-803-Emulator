enum HandIds {USE_NEITHER_HAND=0,USE_LEFT_HAND,USE_RIGHT_HAND,USE_BOTH_HANDS,USE_TRACKING_HAND};
enum handimages {HAND_NO_CHANGE=-1,HAND_NOT_SHOWN=0,HAND_EMPTY,HAND_HOLDING_TAPE,HAND_SLIDING_TAPE,
		 HAND_HOLDING_REEL,HAND_THREE_FINGERS,HAND_ONE_FINGER,HAND_ALL_FINGERS,
		 HAND_HOLDING_REEL_TOP,HAND_PULLING_TAPE,HAND_STICKY_TAPE,HAND_GRABBING,HAND_HOLDING_PAPER};
enum HandConstraints {HAND_NOT_CONSTRAINED=0,HAND_CONSTRAINED_BY_BACK,HAND_CONSTRAINED_BY_NGA,
		      HAND_CONSTRAINED_BY_TAPE_REEL,HAND_CONSTRAINED_BY_PRESS,HAND_CONSTRAINED_BY_VOLUME};
enum WindowIds {NOWINDOW=0,WORDGENWINDOW,CONTACTORWINDOW,READERWINDOW,CREEDKEYBOARDWINDOW,
		DRAWERSWINDOW,HANDVIEWERWINDOW,PTSWINDOW,PLOTTERWINDOW,TEMPLATEWINDOW};

enum HandMode {NO_CHANGE_HAND=-1,IDLE_HAND,TRACKING_HAND,ANIMATING_HAND,MOVING_HAND,TRACKING_OTHER_HAND};

enum HandFSMEvents {HAND_AQUIRE=0,HAND_START,HAND_RELEASE,HAND_END,HAND_ARRIVED,HAND_TRACK_OTHER};

enum FSMstates {OUTSIDE_WINDOW,INSIDE_WINDOW,ANIMATING_WINDOW,LEFT_WINDOW,ENTERED_WINDOW,
		SLIDING_TAPE_INSIDE,SLIDING_TAPE_OUTSIDE,WAITING_FOR_WARP_ENTRY,
		CONSTRAINED_INSIDE,CONSTRAINED_OUTSIDE,BOTH_CONSTRAINED_INSIDE,BOTH_CONSTRAINED_OUTSIDE,
		WAITING_FOR_WARP_ENTRY2};
extern const char *WindowFSMStateNames[];

enum FSMevents {FSM_ENTER=0,FSM_LEAVE,FSM_START,FSM_END,FSM_START_SLIDE,FSM_STOP_SLIDE,FSM_CONSTRAINED,
		FSM_UNCONSTRAINED};
extern const char *WindowFSMEventNames[];

struct fsm WindowFSM;
struct fsm LeftHandFSM;
struct fsm RightHandFSM;

//gdouble MouseAtX,MouseAtY;
enum ControlBits {SET_RESTINGXY=1,SET_FINGERXY=2,SET_TARGETXY=4,SET_ANIMATEFROMXY=8};

enum AnimationType {LINEAR=0,TAPELOAD};


#include "Items.h"



typedef struct handinfo
{
    const char *handName;
    gdouble FingerAtX;
    gdouble FingerAtY;
    gdouble TargetX;
    gdouble TargetY;
    gdouble RestingX;
    gdouble RestingY;
    enum  handimages showingHand;
    enum HandConstraints handConstrained;
    gboolean fixedY;
    gboolean mouseWasOffHand;
    gdouble AnimationParameter;
    gdouble AnimationStep;
    gdouble AnimateFromX;
    gdouble AnimateFromY;
    int (*AnimationFinishedHandler)(HandInfo *animatingHand);
    int StartMovingDelay;
    struct fsm *Fsm;
    gboolean gotoRestingWhenIdle;
    gboolean Moving;
    gboolean dontAquire;
    gdouble moveBy;
    gint FingersPressed;
    gint IndexFingerBit;
    enum HandIds handId;
    Place *place;
    enum AnimationType animation;
    void (*AnimationCallback)(HandInfo *animatingHand);
    gdouble Position;
    
} HandInfo;




HandInfo LeftHandInfo;
HandInfo RightHandInfo;

Place LeftHand;
Place RightHand;

struct WindowEvent
{
    enum WindowIds windowId;
    gdouble eventX;
    gdouble eventY;
//    int (*handler)(struct WindowEvent *);   Deprecated
    GdkWindow *window;
    gpointer data;
} ;




HandInfo *updateHands(gdouble mx,gdouble my, gdouble *hx,gdouble *hy);
HandInfo *getTrackingXY(gdouble *hx,gdouble *hy);
void getTrackingXY2(HandInfo *hand,gdouble *hx,gdouble *hy);
//gboolean  updateHandsDontDrop(gdouble mx,gdouble my, gdouble *hx,gdouble *hy);
void DrawHandsNew(cairo_t *cr);
void setupHandAnimation(enum HandIds handId,gdouble Tx,gdouble Ty,gdouble transitTime);
void HandsInit(__attribute__((unused)) GtkBuilder *builder,
	       GString *sharedPath,
	       __attribute__((unused))  GString *userPath);
gboolean timerTick(__attribute__((unused)) gpointer user_data);

void ConfigureLeftHandNew(gdouble x,gdouble y,int controlBits,
			  enum  handimages showing);
void setLeftHandMode(enum  HandMode mode);
void ConfigureRightHandNew(gdouble x,gdouble y,int controlBits,
			  enum  handimages showing);
void setRightHandMode(enum  HandMode mode);

void ConfigureHand(HandInfo *hand,
		   gdouble x,gdouble y,int controlBits,
		   enum  handimages showing);

void SetActiveWindow(enum WindowIds Wid);
enum WindowIds GetActiveWindow(void);
void setMouseAtXY(gdouble x,gdouble y);
void  setEnterDelay(int n);
void dropHand(void);
void swapHands(GtkWidget *drawingArea);
gboolean putIntoHand(Item *item, Place *hand);
Item *takeFromHand(Place *hand);

GdkCursor *blankCursor;
GdkCursor *savedCursor;

#define HandIsEmpty(HAND)  (g_list_length(HAND.items)==0)
#define HandItemCount(HAND) (g_list_length(HAND.items))

void register_hand_motion_callback(void (*hmc)(HandInfo *hi));

extern GdkWindow *activeGdkWindow;
