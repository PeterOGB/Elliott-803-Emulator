// Cut down PTS that just implements the PLTS network interface

#include <gtk/gtk.h>
#include "PTS.h"
#include "Wiring.h"






static gboolean initPLTS(void);

static gboolean PTSF71 = FALSE;    // F71 and F74 signals in the PTS. 
static gboolean PTSF74 = FALSE;

static void F71changed(unsigned int value)
{
    //printf("%s %u\n",__FUNCTION__,value);
    if(value == 1)
    {
	PTSF71 = TRUE;
	wiring(READY,1);
    }
    else
    {
	PTSF71 = FALSE;
    }
}

static void F74changed(unsigned int value)
{
    //printf("%s %u\n",__FUNCTION__,value);
    if(value == 1)
    {
	PTSF74 = TRUE;
	wiring(READY,1);
    }
    else
    {
	PTSF74 = FALSE;
    }
}

static unsigned int CLines;

static void ClinesChanged(unsigned int value)
{
    //printf("%s %u\n",__FUNCTION__,value);
    CLines = value;
}


static unsigned char tapeRxBuffer[65536];
static int PLTStapePosition;
static GIOChannel *peripheral_channel;
static gboolean PLTSReaderEcho = FALSE;

static void ACTchanged(unsigned int value)
{
    char character;
    //printf("%s %u\n",__FUNCTION__,value);
    if(value == 1)
    {
	gsize written;
	GError *error = NULL;
	if(PTSF71)
	{
	    //wiring(TRLINES,tapeBuffer[counter++] & 0x1F);
	    character = tapeRxBuffer[PLTStapePosition++] & 0x1F; 
	    wiring(TRLINES,(unsigned int) character);
	    wiring(READY,0);
	    if(PLTSReaderEcho)
	    {

		character |= 0x20;
		g_io_channel_write_chars(peripheral_channel,&character,1,&written,&error);
		g_io_channel_flush(peripheral_channel,NULL);
	    }
	}
	if(PTSF74)
	{
	    character = (CLines & 0x1F);
	    g_io_channel_write_chars(peripheral_channel,&character,1,&written,&error);
	    g_io_channel_flush(peripheral_channel,NULL);
	}
	
    }
    else
	if(PTSF71)
	    wiring(TRLINES,0);
}


__attribute__((used))
void PTSInit( __attribute__((unused)) GtkBuilder *builder,
	      __attribute__((unused))  GString *sharedPath,
	      __attribute__((unused))  GString *userPath)
{
    connectWires(F71, F71changed);
    connectWires(F74, F74changed);
    connectWires(ACT, ACTchanged);
    connectWires(CLINES,ClinesChanged);




    initPLTS();
    
}


/************************** PLTS *****************************/
/* For network sockets */
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h>

static GIOChannel *listening_channel;

//static int PLTStapePosition;

static gboolean PLTSReaderOnline = FALSE;
static gsize tapeRxBlockLength,tapeRxOffset;

//static int onlineRd = 0;
static int onlineWr = 0;
static gchar onlineBuffer[32];
//static unsigned char tapeRxBuffer[65536];

static gboolean  process_message(GIOChannel *source,
				 __attribute__((unused))GIOCondition condition,
				 __attribute__((unused))gpointer data)
{
    static gchar message[300];
    gsize length,written;
    GError *error = NULL;
    GIOStatus status;
    static gsize messageLength = 1;
    static gsize offset = 0;
    char value[3];
    static unsigned char cmd = 0;
    static gchar onlineLS = 0;
    

    status = g_io_channel_read_chars(source,&message[offset],messageLength,&length,&error);
	
    if(status != G_IO_STATUS_NORMAL)
    {
	g_io_channel_shutdown(source,FALSE,NULL);
	printf("Disconnect from PLTS\n");
	return FALSE;
    }
    else
    {
	messageLength -= length;
	offset += length;
	//printf("RX(%d,%d,%d,cmd=0x%02X): ",offset,(int)length,(int)messageLength,cmd);
	//for(n=0;n<offset;n++) printf("0x%02X ",message[n] & 0xFF);
	//printf("\n");

	if(messageLength == 0)
	{
	    if(cmd == 0)
	    {
		switch(message[0] & 0xFF)
		{
		case 0x80:
		    cmd = 0x80;
		    messageLength = 2;
		    break;
		case 0x81:
		    cmd = 0x81;
		    messageLength = 1;
		    break;
		case 0x82:
		    PLTStapePosition = 0;
		    cmd = 0;
		    offset = 0;
		    messageLength = 1;
		    break;
		case 0x84:
		    PLTSReaderEcho = TRUE;
		    cmd = 0;
		    messageLength = 1;
		    offset = 0;
		    break;
		case 0x85:
		    PLTSReaderEcho = FALSE;
		    cmd = 0;
		    messageLength = 1;
		    offset = 0;
		    break;
		case 0x88:
		    PLTSReaderOnline = TRUE;
		    cmd = 0;
		    messageLength = 1;
		    offset = 0;
		    break;
		case 0x89:
		    PLTSReaderOnline = FALSE;
		    cmd = 0;
		    messageLength = 1;
		    offset = 0;
		    break;
		case 0x8A:
		    cmd = 0x8A;
		    messageLength = 1;

		    break;	    

		}
	    }
	    else
	    {
		switch(cmd)
		{
		case 0x80:
		    value[0] = '\x81';
		    g_io_channel_write_chars(source,value,1,&written,&error);
		    g_io_channel_flush(source,NULL);
		    cmd = 0;
		    messageLength = 1;
		    offset = 0;
		    tapeRxOffset = 0;
		    PLTStapePosition = 0;
		    break;
		case 0x81:
		    cmd = 0xFF;
		    messageLength = message[1] & 0xFF ;
		    if(messageLength == 0) messageLength = 256;
		    tapeRxBlockLength = messageLength;
		    break;

		case 0x8A:
		{
		    gboolean setShift = FALSE;

		    // Test is "force shift" bit set
		    if((message[1] & 0x80) == 0x80)
		    {
			setShift = TRUE;
		    }
		    // Test is "shift independent" bit set
		    else if((message[1] & 0x40) == 0x00)
		    {
			// Test is shift has changed
			if(((message[1] ^ onlineLS) & 0x20) == 0x20)
			{
			    setShift = TRUE;
			}
		    }

		    
		    if(setShift)
		    {
			onlineLS = (char) (0x1B + ((message[1] >> 3) & 0x4));
			
			

			onlineBuffer[onlineWr++] = onlineLS;
			onlineWr &= 0x1F;
			onlineLS = message[1];
		    }
		    

		    
		    onlineBuffer[onlineWr++] =  message[1] & 0x1F;
		    onlineWr &= 0x1F;

		    

		    
		    cmd = 0;
		    messageLength = 1;
		    offset = 0;
		    break;
		}
		    
		case 0xFF:
		    
		    memcpy(&tapeRxBuffer[tapeRxOffset],&message[2],tapeRxBlockLength);
		    tapeRxOffset += tapeRxBlockLength;
		    value[0] = '\x81';
		    g_io_channel_write_chars(source,value,1,&written,&error);
		    g_io_channel_flush(source,NULL);
		    cmd = 0;
		    offset = 0;
		    messageLength = 1;
		    break;
		    


		}
	    }

	
	    //parse(message,starters,0);
	}
    
    }    
    return TRUE;
}


static gboolean
accept_new_connection(GIOChannel *source,
		      __attribute__((unused))GIOCondition condition,
		      __attribute__((unused))gpointer data)
{
    int listen_socket;  
    struct sockaddr_in from; 
    socklen_t fromlen;
    int peripheral_socket;
    


    listen_socket = g_io_channel_unix_get_fd(source);

    fromlen = sizeof (from); 
    peripheral_socket = accept(listen_socket, (struct sockaddr *)&from, &fromlen);
    if(peripheral_socket >= 0)
    {
	struct hostent *hp;
	//char logMessage[100];
           
	/* Add this as an channel as well */
	peripheral_channel = g_io_channel_unix_new(peripheral_socket);




	g_io_add_watch(peripheral_channel,G_IO_IN ,process_message,NULL);
	g_io_channel_set_encoding(peripheral_channel,NULL,NULL);
//	g_io_add_watch(peripheral_channel,G_IO_HUP ,peripheral_message,NULL);
//	g_io_add_watch(peripheral_channel,G_IO_ERR ,peripheral_message,NULL);

	hp = gethostbyaddr((char *) &from.sin_addr.s_addr,4,AF_INET);
	if(hp != NULL)
	    printf("accepted connection from %s\n",hp->h_name);
	else
	    printf("accepted connection form unresoved address\n");

    } 
    return TRUE;
}




static gboolean initPLTS(void)
{
    struct sockaddr_in name; 
    socklen_t length;
    int listen_socket;
    //char logMessage[100];
    int reuseaddr;
    /* Create socket with which to listen for connections from peripherals. */ 
    listen_socket = socket(AF_INET, SOCK_STREAM, 0); 
    if (listen_socket < 0)
    { 
	perror("opening TCP socket for peripheral connections"); 
	return(FALSE); 
    } 
	  
    reuseaddr = 1;
    if(setsockopt(listen_socket,SOL_SOCKET,SO_REUSEADDR,&reuseaddr,sizeof(reuseaddr)) == -1)
    {
	printf("**** setsockopt FAILED *******\n");
    }



 
    /* Create name with wildcards. */ 
    name.sin_family = AF_INET; 
    name.sin_addr.s_addr = INADDR_ANY; 
    name.sin_port = htons(7000); 
    if (bind(listen_socket,(struct sockaddr *) &name, sizeof(name))) { 
	perror(">>>>>>>>>>>>>>>>>>>>>> binding TCP socket <<<<<<<<<<<<<<<<<<<"); 
	return(FALSE); 
    } 
    /* Find assigned port value and print it out. */ 
    length = sizeof(name); 
    if (getsockname(listen_socket,(struct sockaddr *) &name, &length)) { 
	perror("getting socket name"); 
	return(FALSE); 
    } 
    printf("Listening for new network connections on port #%d\n", ntohs(name.sin_port)); 
 
  
    listening_channel = g_io_channel_unix_new(listen_socket);

    g_io_add_watch(listening_channel,G_IO_IN,accept_new_connection,NULL);

    /* listen for new connections ! */
    listen(listen_socket,1);

    return(TRUE);
  


}
