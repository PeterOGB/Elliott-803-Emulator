#define G_LOG_USE_STRUCTURED
#include <stdio.h>
#include <gtk/gtk.h>


#include "Cpu.h"
#include "Wiring.h"

// Currently these are all just stubbs.
// Wire handlers
static void setF1(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
}

static void setN1(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
}

static void setF2(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
}

static void setN2(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
}

static void setRON(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
}

static void setMD(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
}

static void setRESET(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
}

static void setCS(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
}

static void setSS(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
}

static void setOPERATE(unsigned int value)
{
    g_info("%s called value = %d\n",__FUNCTION__,value);
}


void CpuTidy(__attribute__((unused)) GString *userPath)
{
    g_info("%s called\n",__FUNCTION__);
}

void CpuInit(__attribute__((unused)) GtkBuilder *builder,
	     __attribute__((unused)) GString *sharedPath,
	     __attribute__((unused)) GString *userPath)
{
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


}
