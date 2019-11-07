#define G_LOG_USE_STRUCTURED
#include <stdio.h>
#include <gtk/gtk.h> 
#include <stdint.h>
// There is no real gtk+ code here, so g_boolean is not used.
#include <stdbool.h>

#include "Cpu.h"
#include "Wiring.h"
#include "Sound.h"

#include "E803-types.h"
#include "wg-definitions.h"
#include "Emulate.h"



extern E803word WG;
unsigned int WG_ControlButtons;    
//unsigned int WG_ControlButtonPresses;
//unsigned int WG_ControlButtonReleases;

bool WG_operate_pressed = false;
unsigned int volume;

static
void dumpWord(E803word word)
{
    unsigned int f1,n1,b,f2,n2;
    
    f1 = (word >> 33) & 077;
    n1 = (word >> 20) & 017777;
    b =  (word >> 19) & 01;
    f2 = (word >> 13) & 077;
    n2 = word & 017777;

    printf("%02o %4d %c %02o %4d\n",f1,n1,b?'/':':',f2,n2);
    
}


// Wire handlers

static void setF1(unsigned int value)
{
    E803word bits = value;
    g_info("%s called value = %d\n",__FUNCTION__,value);

    WG &= 00077777777777;   // F1
    WG |= 07700000000000 & (bits << 33);
    dumpWord(WG);
}

static void setN1(unsigned int value)
{
    E803word bits = value;
    g_info("%s called value = %d\n",__FUNCTION__,value);
    
    WG &= 07700001777777;   // N1 + B
    WG |= 00077776000000 & (bits << 19);
    dumpWord(WG);
}

static void setF2(unsigned int value)
{
    E803word bits = value;
    g_info("%s called value = %d\n",__FUNCTION__,value);
    WG &= 07777776017777;   // F1
    WG |= 00000001760000 & (bits << 13);
    dumpWord(WG);
}

static void setN2(unsigned int value)
{
    E803word bits = value;
    g_info("%s called value = %d\n",__FUNCTION__,value);
    WG &= 07777777760000;   // N2
    WG |= 00000000017777 & bits;
    dumpWord(WG);
}

static void setRON(unsigned int button)
{
    g_info("%s called value = %d\n",__FUNCTION__,button);
    WG_ControlButtons &= ~(WG_read | WG_normal | WG_obey);
    WG_ControlButtons |= button;
}

static void setMD(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
    if(value == 0)
	WG_ControlButtons &= ~WG_manual_data;
    else
	WG_ControlButtons |= WG_manual_data;
}

static void setRESET(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
    if(value == 0)
	WG_ControlButtons &= ~WG_reset;
    else
	WG_ControlButtons |= WG_reset;
}

static void setCS(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
    if(value == 0)
	WG_ControlButtons &= ~WG_clear_store;
    else
	WG_ControlButtons |= WG_clear_store;
}

static void setSS(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
    if(value == 0)
    {
	WG_ControlButtons &= ~WG_selected_stop;
	//WG_ControlButtonReleases |= WG_selected_stop;
    }
    else
    {
	WG_ControlButtons |= WG_selected_stop;
	//WG_ControlButtonPresses |= WG_selected_stop;	   
    }
}

static void setOPERATE(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
    if(value == 0)
    {
	WG_ControlButtons &= ~WG_operate;
	//WG_ControlButtonReleases |= WG_operate;
    }
    else
    {
	WG_ControlButtons |= WG_operate;
	WG_operate_pressed = true;
	//WG_ControlButtonPresses |= WG_operate;
    }

}


static void CPU_sound(__attribute__((unused)) void *buffer, 
		      __attribute__((unused))int sampleCount,
		      __attribute__((unused))double bufferTime,
		      int wordTimes)
{
    static bool first = true;
    static bool updateFlag = true;
    static int updateRate  = UPDATE_RATE;
    static int callCount = 1;


    if (first)
    {
    	first = false;
    	PreEmulate(false);
    }
    else
    {
	if(updateRate == UPDATE_RATE)
	{
	    updateRate = 1;
	    updateFlag = true;
	    //printf("updateFlag true at %d calls\n",callCount);
	}
	else
	{
	    updateRate += 1;
	    updateFlag = false;
	    
	}
    }

    // Simplified version for testing during development

    PostEmulate(updateFlag);

    PreEmulate(updateFlag);

    //doExternalDeviceFSM(SOUNDTICK);
    Emulate(wordTimes);

    //WG_ControlButtonPresses = WG_ControlButtonReleases = 0;
    WG_operate_pressed = false;
    
    //gtk_widget_queue_draw(CoreDrawingArea);


    callCount += 1;
		
}





void CpuTidy(__attribute__((unused)) GString *userPath)
{
    g_info("%s called\n",__FUNCTION__);
}


void CpuInit(__attribute__((unused)) GtkBuilder *builder,
	     __attribute__((unused)) GString *sharedPath,
	     __attribute__((unused)) GString *userPath,
	     gchar *coreFileName)
    
{
    GString *CoreImageFileName = NULL;
    gchar *core = NULL;
    GError *error = NULL;
    
    //g_info("%s called\n",__FUNCTION__);

    connectWires(F1WIRES,setF1);
    connectWires(N1WIRES,setN1);
    connectWires(F2WIRES,setF2);
    connectWires(N2WIRES,setN2);
    connectWires(RONWIRES,setRON);
    connectWires(MDWIRE,setMD);
    connectWires(RESETWIRE,setRESET);
    connectWires(CSWIRE,setCS);
    connectWires(SSWIRE,setSS);
    connectWires(OPERATEWIRE,setOPERATE);
    connectWires(VOLUME_CONTROL,setCPUVolume);


    CoreImageFileName = g_string_new(userPath->str);
    if(coreFileName != NULL)
    {
	g_string_append(CoreImageFileName,coreFileName);
    }
    else
    {
	g_string_append(CoreImageFileName,"CoreImage");
    }



    {

	GFile *gf;
	GBytes *gb;
	GByteArray *gba;
	

	gf = g_file_new_for_path(CoreImageFileName->str);
	
	
	gb = g_file_load_bytes(gf,
			       NULL,NULL,&error);

	if(error != NULL)
	{
	    g_warning("Failed to open core file %s (%s), using a clear store\n",
		      CoreImageFileName->str,error->message);
	    core = calloc(8192,sizeof(E803word));
	
	}
	else
	{
	    // Don't unref the gba as it's data IS the core store.
	    gba = g_bytes_unref_to_array(gb);
	    g_byte_array_ref(gba);
	    

	    core = (char *) gba->data;

	    g_info("Core image file Length = %u\n",gba->len);
	}
	g_object_unref(gf);
    }
    
    StartEmulate(core);
    
    g_string_free(CoreImageFileName,TRUE);

    addSoundHandler(CPU_sound);
}
