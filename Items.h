#pragma once


typedef struct tapeReel
{
    char *fileName;
    char *writing;
    gdouble fxpos,fypos,fzpos,fradius,fangle;  /* values from INFO file */
    gdouble ReelRadius;   // Computed from length when the tapeArray is initialise
                          // when the tape is removed from a drawer.
    gdouble ReelRadiusReduced;   // May be less if tape unwinding.
    
    int tapeColour;
    GByteArray *tapeArray;
    guint tapeFirst;      // index of first non runout in tapeBuffer
    guint remaining;      // number of chars after first non runout
    gboolean woundup;
} TapeReel;

typedef struct 
{
    guint32 HolePixels[4][4];
    guint32 SprocketPixels[4][4];
    guint32 TapePixels[4][4];
} TapeImageInfo;



enum ItemType { NOTHING=0, TAPEREEL };


typedef struct item
{
    enum ItemType itemType;    // What it is
    GList *places;             // Where it is (An item may be in more than one place)
    union
    {
	TapeReel *aTapeReel;

    };
} Item;




enum PlaceType { NOWHERE=0,ADRAWER,AHAND,TAPE_HOLDER,READER};

// FOrward definition to avoid dependency loop.
typedef struct handinfo HandInfo;

typedef struct drawer
{
    char *drawerName;
    int drawerId;
     
} Drawer;




typedef struct place
{
    enum PlaceType placeType;
    GList *items;
    union
    {
	Drawer *drawer;
	HandInfo *hand;
    };
} Place;

// TEMPREMOVE extern
GList *Places;
