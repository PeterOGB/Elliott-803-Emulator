/*  This file is part of the Elliott 803 emulator.

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
#include <locale.h>
#include <pwd.h>

#include "Common.h"
#include "Logging.h"
#include "fsm.h"

#include "Emulator.h"
#include "Hands.h"
#include "Keyboard.h"
#include "Contactor.h"
#include "Charger.h"
#include "PowerCabinet.h"
#include "Cpu.h"
#include "HoursRun.h"
#include "Wiring.h"
#include "Sound.h"

// TEMPREMOVE#include "Items.h"
// TEMPREMOVE#include "HandViewer.h"
// TEMPREMOVE#include "SimpleDrawers.h"
// TEMPREMOVE#include "PTS.h"

#include "Main.h"
// TEMPREMOVE#include "Reader.h"
// TEMPREMOVE#include "CreedKeyboard.h"
// TEMPREMOVE#include "Drawers.h"

// Paths to directories. 
static GString *sharedPath = NULL;
static GString *configPath = NULL;


#if 0
// Stub for use when more peripherals are added
static void testMainsOn(__attribute__((unused)) unsigned int dummy)
{
    printf("%s called\n",__FUNCTION__);
}

static void testMainsOff(__attribute__((unused)) unsigned int dummy)
{
    printf("%s called\n",__FUNCTION__);
}
#endif

// Use the wiring system to call handlers 100 times a second
static gboolean timerTick100(__attribute__((unused)) gpointer user_data)
{
    wiring(TIMER100HZ,0);
    return  G_SOURCE_CONTINUE;
}



extern GIOChannel *traceChannel;

// Call the "Tidy" functions which save current emulator state into files.
void EmulatorShutdown(void)
{
    // TEMPREMOVE ReaderBinTidy(configPath);
    //ScopeTidy(configPath);
    // TEMPREMOVE PunchTidy(configPath);
    ChargerTidy(configPath);
    ContactorTidy(configPath);
    // TEMPREMOVE PrinterTidy(configPath);
    // TEMPREMOVE EditorTidy(configPath);
    // TEMPREMOVE ReaderTidy(configPath);
    // TEMPREMOVE HandViewerTidy(configPath);
    // TEMPREMOVE DrawersTidy(configPath);
    // TEMPREMOVE PTSTidy(configPath);
    WordGenTidy(configPath);
    HoursRunTidy(configPath);
    // TEMPREMOVEPTSTidy(configPath);
    // TEMPREMOVE CPUTidy(configPath);
/* TEMPREMOVE 
    if(traceChannel != NULL)
    {

	g_io_channel_shutdown (traceChannel,
                       TRUE,
                       NULL);
    }
*/

    gtk_main_quit();
}

gchar *coreFileName =  NULL;

static GOptionEntry entries[] =
{

    { "corefile", 'c', 0, G_OPTION_ARG_FILENAME, &coreFileName, "Initial core store file", NULL },
    { NULL }
};





int main(int argc, char *argv[])
{
    GtkBuilder *builder;
    //GString *path,*userpath;
    struct passwd *pw;
    uid_t uid;
    GString *gladeFileName;
    GError *error = NULL;
    GOptionContext *context;
    
    
    /* Set the locale according to environment variable */
    if (!setlocale(LC_CTYPE, ""))
    {
	fprintf(stderr, "Can't set the specified locale! "
		"Check LANG, LC_CTYPE, LC_ALL.\n");
	exit(EXIT_FAILURE);
    }

    /* Set global path to user's configuration and machine state files */
   
    uid = getuid();
    pw = getpwuid(uid);

    configPath = g_string_new(pw->pw_dir);
    configPath = g_string_append(configPath,"/.803-Emulator/");
    
    /* Set global path to shared icons, pictures and sound effect files */
    sharedPath = g_string_new("/usr/local/share/803-Emulator/");



    // Normal GTK application start up
    gtk_init (&argc, &argv);

    LoggingInit();

    // New command line option parsing....

    context = g_option_context_new ("- Elliott 803 Emulator");
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("option parsing failed: %s\n", error->message);
      exit (1);
    }

    g_info("Core file set to %s\n",coreFileName);




    
    // Create the GUI from 
    builder = gtk_builder_new();


    gladeFileName = g_string_new(sharedPath->str);
    g_string_append(gladeFileName,"803-Emulator.glade");
    
    if(gtk_builder_add_from_file(builder, gladeFileName->str, NULL) == 0)
	g_error("Failed to find glade file (%s)\n",gladeFileName->str);

    SoundInit(builder,sharedPath,configPath);

    // Cpu must be set up before the keyboard so that the initial states of the
    // buttons can be sent 
    CpuInit(builder,sharedPath,configPath,coreFileName);
    
    HandsInit(builder,sharedPath,configPath);
    WordGenInit(builder,sharedPath,configPath);
    ContactorInit(builder,sharedPath,configPath);
    ChargerInit(builder,sharedPath,configPath);
    PowerCabinetInit(builder,sharedPath,configPath);
    // TEMPREMOVEHandViewerInit(builder,sharedPath,configPath);
    // TEMPREMOVEReaderInit(builder,sharedPath,configPath);
// TEMPREMOVE    CreedKeyboardInit(builder,path,userpath);
    // TEMPREMOVESimpleDrawersInit(builder,sharedPath,configPath);
    
    //PTSInit(builder,sharedPath,configPath);

// TEMPREMOVE    connectWires(MAINS_SUPPLY_ON,testMainsOn);
// TEMPREMOVE    connectWires(MAINS_SUPPLY_OFF,testMainsOff);
    HoursRunInit(builder,sharedPath,configPath);
    
    gtk_builder_connect_signals (builder, NULL);
    g_object_unref (G_OBJECT (builder));

    
    // Setup a couple of periodic timeouts 
    g_timeout_add(20,timerTick,NULL);
    g_timeout_add(10,timerTick100,NULL);


    // Start the GTK+ main event loop.
    gtk_main();
    
    return(EXIT_SUCCESS);
}
