/*  The Battery Charger for the Elliott 803 emulator.

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

#include <stdlib.h>
#include <gtk/gtk.h>
#include <math.h>

#include "Charger.h"
#include "Wiring.h"
#include "Common.h"


static GtkWidget *window;
#define WINDOWNAME "Battery Charger"
static GtkWidget *meterDrawingArea;
static cairo_surface_t *mask1, *image;

/*
Constants for current meter damping from PID.c
*/
//static double KPcurrentMeter = 0.1;
//static double KIcurrentMeter = 0.45;

static double Va,Vb,Vc,Vd,Ve,Vf,Vg;
static double Ia,Ib,Ic,Id,Ie,If;

static double KPvoltageMeter = 0.10;

static double KAcharger = 0.029;
//static double KBcharger = 0.0;
static double KCcharger = 0.83;

static double Rbat = (1.0/88.0);


static double Icharger = 0.0;
static double Icomputer = 0.0;
static double Ipts = 0.0;
static double Ireader = 0.0;
//extern gboolean Clutch;
static double Ipunch = 0.0;
//extern unsigned int Punched; 
static double Ibat;
static double CurrentMeterReading;
static double Vop;
double Vbat = 0.0;
static double VoltageReading;
static gboolean mainsAvailable = FALSE;

static gboolean drawMeter(__attribute__((unused)) GtkWidget *widget, 
			  cairo_t *cr, 
			  __attribute__((unused)) gpointer user_data)
{

// V = 0.105 ..  
// I = 0.105 .. 

    double angleV;
    double angleI;

    //printf("angleV=%f\n",angleV);  
    cairo_set_source_surface(cr,image, 0,0);
    cairo_paint(cr);

// Set size and colour of the pointer
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0,1.0);
    cairo_set_line_width(cr, 1.5);

    if(CurrentMeterReading <= 10.0)
    {
	angleI = 0.105 + ((0.123 - 0.105) * CurrentMeterReading * 0.1); 
    }
    else if( (CurrentMeterReading > 10.0) && (CurrentMeterReading <= 20.0) )
    {
	angleI = 0.123 + ((0.158 - 0.123) * (CurrentMeterReading - 10.0)  * 0.1); 
    }
    else if( (CurrentMeterReading > 20.0) && (CurrentMeterReading <= 30.0) ) 
    {
	angleI = 0.158 + ((0.215 - 0.157) * (CurrentMeterReading - 20.0)  * 0.1); 
    }
    else if( (CurrentMeterReading > 30.0) && (CurrentMeterReading <= 40.0) ) 
    {
	angleI = 0.215 + ((0.280 - 0.215) * (CurrentMeterReading - 30.0)  * 0.1); 
    }
    else if( (CurrentMeterReading > 40.0) && (CurrentMeterReading <= 50.0) ) 
    {
	angleI = 0.280 + ((0.341 - 0.280) * (CurrentMeterReading - 40.0)  * 0.1); 
    }
    else if( (CurrentMeterReading > 50.0) && (CurrentMeterReading <= 60.0) ) 
    {
	angleI = 0.341 + ((0.397 - 0.341) * (CurrentMeterReading - 50.0)  * 0.1); 
    }
    else 
	angleI = 0.397;


    if(VoltageReading <= 5.0)
    {
	angleV = 0.105 + ((0.152 - 0.105) * VoltageReading * 0.2); 
    }
    else if( (VoltageReading > 5.0) && (VoltageReading <= 10.0) )
    {
	angleV = 0.152 + ((0.203 - 0.152) * (VoltageReading - 5.0)  * 0.2); 
    }
    else if( (VoltageReading > 10.0) && (VoltageReading <= 15.0) ) 
    {
	angleV = 0.203 + ((0.256 - 0.203) * (VoltageReading - 10.0)  * 0.2); 
    }
    else if( (VoltageReading > 15.0) && (VoltageReading <= 20.0) ) 
    {
	angleV = 0.256 + ((0.308 - 0.256) * (VoltageReading - 15.0)  * 0.2); 
    }
    else if( (VoltageReading > 20.0) && (VoltageReading <= 25.0) ) 
    {
	angleV = 0.308 + ((0.357 - 0.308) * (VoltageReading - 20.0)  * 0.2); 
    }
    else if( (VoltageReading > 25.0) && (VoltageReading <= 30.0) ) 
    {
	angleV = 0.357 + ((0.394 - 0.357) * (VoltageReading - 25.0)  * 0.2); 
    }
    else 
	angleV = 0.394;



    //printf("%f %f\n",CurrentMeterReading,angleI);
    //printf("%f %f\n",Voltage,angleV);


   // Draw the meter pointer
    cairo_move_to(cr, 123.0, 211.0);
    cairo_line_to(cr, 123.0 - 83.0 * cos(2.0 * M_PI * angleV), 211.0  -83.0 * sin(2.0 * M_PI * angleV) );

    cairo_move_to(cr, 390.0, 216.0);
    cairo_line_to(cr, 390.0 - 83.0 * cos(2.0 * M_PI * angleI), 216.0  -83.0 * sin(2.0 * M_PI * angleI) );


    cairo_stroke(cr); 
    
    cairo_set_source_surface(cr,image, 0,0);
    cairo_mask_surface(cr,mask1, 0, 0);
    //cairo_fill(cr);

    ///angleV += 0.001;
    
    return FALSE;

}

// Testing wiring

//static gboolean batteryState = FALSE;
static gboolean chargerConnected = FALSE;
//static gboolean computerOn = FALSE;
//static gboolean ptsOn = FALSE;

static void ChargerConnected(__attribute__((unused)) unsigned int dummy)
{
    //printf("%s called\n",__FUNCTION__);
    Vbat = 26.5;
    //Icharger = 0.0;
    //Icomputer = 0.0;
    Va = Vb = Vc = Vd = Ve =  Vf = Vg = 0.0;
    chargerConnected  = TRUE;
}

static void ChargerDisconnected(__attribute__((unused)) unsigned int dummy)
{
    //printf("%s called\n",__FUNCTION__);
    chargerConnected = FALSE;
    /*
    Vbat = 0.0;
    batteryOn = FALSE;
    */
}

static void CpuLoadOn(__attribute__((unused)) unsigned int dummy)
{
    //printf("%s called\n",__FUNCTION__);
    Icomputer = 40.0;
}  

static void CpuLoadOff(__attribute__((unused)) unsigned int dummy)
{
    //printf("%s called\n",__FUNCTION__);
    Icomputer = 0.0;
} 

static void MainsSupplyOn(__attribute__((unused)) unsigned int dummy)
{
    //printf("%s called\n",__FUNCTION__);
    mainsAvailable = TRUE;
}  

static void MainsSupplyOff(__attribute__((unused)) unsigned int dummy)
{
    //printf("%s called\n",__FUNCTION__);
    mainsAvailable = FALSE;
} 

// Called 100 times/sec to model the bahaviour of the Voltage and Current  meters */
static void updateCharger(__attribute__((unused)) unsigned int dummy)
{
    static int displayRate = 0;
    double error,dt;
    static double Ierror;
    double C,R;

    Ipunch = 0.0;


    // Time constants etc
    dt = 0.015;
    C = 1.0E-3;
    R = 52.0 * 2.0;

    if(chargerConnected)
    {
	// Model the slow current rise when the computer is turned on.
	Vbat += Ibat * KAcharger * dt; // Battery charge/discahrge slope

	Vop = Vbat + (Ibat * Rbat);

	Ibat = Icharger - (Icomputer + Ipts + Ireader + Ipunch) ;

	if(mainsAvailable)
	{

	    Ierror = (27.0 - Vop) * 88.0 * KCcharger;
	    // Cap Ierror rather tnat Icharger to give a smoother meter motion
	    if(Ierror > 50.0) Ierror = 50.0;
	    Va = Ierror;

	    Ia = (Va - Vb) / R;
	    Vb += Ia * dt / C;

	    Ib = (Vb - Vc) / R;
	    Vc += Ib * dt / C;

	    Ic = (Vc - Vd) / R;
	    Vd += Ic * dt / C;

	    Id = (Vd - Ve) / R;
	    Ve += Id * dt / C;
	
	    Ie = (Ve - Vf) / R;
	    Vf += Ie * dt / C;

	    If = (Vf - Vg) / R;
	    Vg += If * dt / C;

	    Icharger = Vg;
	}
	else
	{
	    	    Icharger *= 0.95;
	}
    }
    else
    {
	// Model the quick current fall when the computer is turned off.
	Vop *= 0.99;
	Icharger *= 0.95;
	Va = Vb = Vc = Vd = Ve =  Vf = Vg = 0.0;
    }


    CurrentMeterReading = Icharger;
    error = Vop - VoltageReading;
    VoltageReading += error * KPvoltageMeter ;

    // Only update display every other call.
    if(displayRate++ == 1) 
    {
	displayRate = 0;
	gtk_widget_queue_draw(meterDrawingArea);
    }
}


// Repositon the window from config file
static int savedWindowPositionHandler(int nn)
{
    int windowXpos,windowYpos;
    windowXpos = atoi(getField(nn+1));
    windowYpos = atoi(getField(nn+2));
    gtk_window_move(GTK_WINDOW(window),windowXpos,windowYpos);
    return TRUE;
}

static Token savedStateTokens[] = {
    {"WindowPosition",0,savedWindowPositionHandler},
    {NULL,0,NULL}
};


// Save the window position in the "ChargerState" config file.
void ChargerTidy(GString *userPath)
{
    GString *configText;
    int windowXpos,windowYpos;

    configText = g_string_new("# Charger configuration\n");

    gtk_window_get_position(GTK_WINDOW(window), &windowXpos, &windowYpos);
    g_string_append_printf(configText,"WindowPosition %d %d\n",windowXpos,windowYpos);

    updateConfigFile("ChargerState",userPath,configText);

    g_string_free(configText,TRUE);

    gtk_widget_destroy(window);
}



void ChargerInit(__attribute__((unused)) GtkBuilder *builder,
		 GString *sharedPath,
		 __attribute__((unused)) GString *userPath)
{
    int width,height;
   
    GString *fileName;
    cairo_status_t surfaceStatus;
    fileName = g_string_new(NULL);

    // Mask that hides the bottom of the pointer
    g_string_assign(fileName,sharedPath->str);
    g_string_append(fileName,"graphics/ChargerMask.png");
    mask1  = cairo_image_surface_create_from_png(fileName->str);
    surfaceStatus = cairo_surface_status(mask1);
    if(surfaceStatus != CAIRO_STATUS_SUCCESS)
    {
	g_error("Failed to create cairo surface due to %s (%s)\n",
		cairo_status_to_string(surfaceStatus),fileName->str);

    }

    // Background image of the meters without pointers
    g_string_assign(fileName,sharedPath->str);
    g_string_append(fileName,"graphics/VandIsmall.png");
    image = cairo_image_surface_create_from_png(fileName->str);
    surfaceStatus = cairo_surface_status(image);
    if(surfaceStatus != CAIRO_STATUS_SUCCESS)
    {
	g_error("Failed to create cairo surface due to %s (%s)\n",
		cairo_status_to_string(surfaceStatus),fileName->str);

    }
    g_string_free(fileName,TRUE);
    
    width =  cairo_image_surface_get_width(image);
    height =  cairo_image_surface_get_height(image);

    // Create window sized to the background image
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title( GTK_WINDOW(window),WINDOWNAME);
    gtk_window_set_default_size(GTK_WINDOW(window), width, height);
 
    // Add drawing area for the meter needles
    meterDrawingArea = gtk_drawing_area_new();
    gtk_widget_set_size_request(meterDrawingArea,width,height);
    gtk_container_add (GTK_CONTAINER (window), meterDrawingArea);

    g_signal_connect (meterDrawingArea, "draw", (GCallback) drawMeter, NULL);
      
    gtk_widget_set_app_paintable(meterDrawingArea, TRUE);

    // Show the window and stop it being resized.
    gtk_widget_show_all (window);
    gtk_window_set_resizable(GTK_WINDOW(window),FALSE);

    // Read config file (which will reposition the window).
    readConfigFile("ChargerState",userPath,savedStateTokens);

    // Wire up the battery charger
    connectWires(CHARGER_CONNECTED,ChargerConnected);
    connectWires(CHARGER_DISCONNECTED,ChargerDisconnected);
    connectWires(SUPPLIES_ON,CpuLoadOn);
    connectWires(SUPPLIES_OFF,CpuLoadOff);
    connectWires(MAINS_SUPPLY_ON,MainsSupplyOn);
    connectWires(MAINS_SUPPLY_OFF,MainsSupplyOff);

    // Hook updateCharger into the 100Hz timer.
    connectWires(TIMER100HZ,updateCharger);
   
}



