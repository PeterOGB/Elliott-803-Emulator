#define G_LOG_USE_STRUCTURED
#include <math.h>
#include <alsa/asoundlib.h>
#include <gtk/gtk.h>

#include "Sound.h"

#define SOUNDDEBUG 0


/* Various ALSA configuration values */
static snd_pcm_format_t soundSampleFormat = SND_PCM_FORMAT_S16_LE;
static unsigned int soundChannelCount = 2;   /* count of channels, stereo */


static const char *soundDevice = "default";
static snd_pcm_t *AlsaHandle = NULL;
int16_t *periodBuffer;                   /* Global buffer for effects samples */
static unsigned int fdCount = 0;
static struct pollfd *ufds = NULL;

static unsigned int FramesPerPeriod;
static unsigned int PeriodBufferSizeInBytes;
static int iFramesPerWordTime;
static int iWordTimesPerPeriod;

// List of sound effects generator functions
static GSList *soundHandlers = NULL;



// Infrastructure to allow ALSA to be used as an event source in the GTK event loop.

gboolean (*AlsaHandler)(GSource *,GIOCondition ,gpointer ) = NULL;

static gboolean AlsaPrepareFunc(__attribute__((unused)) GSource *source, gint *timeout)
{
    *timeout = -1;
    return FALSE;
}


static gboolean AlsaCheckFunc(__attribute__((unused)) GSource *source)
{
    unsigned short revents[2];

    snd_pcm_poll_descriptors_revents(AlsaHandle, ufds, fdCount, &revents[0]);

    if (revents[0] & POLLOUT)
	return TRUE;

    return FALSE;
}


static gboolean AlsaDispatchFunc(GSource *source,
				 __attribute__((unused)) GSourceFunc cb, 
				 __attribute__((unused))gpointer data)
{
    (AlsaHandler)(source,0,(gpointer) AlsaHandle);
    ufds->revents = 0;

    return TRUE;
}


// Data structure to  pass to g_source_new
static GSourceFuncs AlsaSourceFuncs = {
    AlsaPrepareFunc,
    AlsaCheckFunc,
    AlsaDispatchFunc,
    NULL,
    NULL,
    NULL
};



// Alsa configurator !

static int set_hwparamsV3(snd_pcm_t *handle,
			  snd_pcm_hw_params_t *params,
			  snd_pcm_access_t access,
			  unsigned int periods,
			  snd_pcm_uframes_t period_size,
			  unsigned int rate)

{
    unsigned int rrate;
    int err;

    /* choose all parameters */
    if((err = snd_pcm_hw_params_any(handle, params)) < 0) 
    {
	g_error("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
	return err;
    }
    /* set the interleaved read/write format */
    if((err = snd_pcm_hw_params_set_access(handle, params, access)) < 0) 
    {
	g_error("Access type not available for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* set the sample format */
    if((err = snd_pcm_hw_params_set_format(handle, params,  soundSampleFormat)) < 0) 
    {
	g_error("Sample format not available for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* set the count of channels */
    if((err = snd_pcm_hw_params_set_channels(handle, params, soundChannelCount)) < 0) 
    {
	g_error("Channels count (%i) not available for playbacks: %s\n",  soundChannelCount, snd_strerror(err));
	return err;
    }
    /* set the stream rate */
    rrate = rate;
    if((err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0)) < 0) 
    {
	g_error("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
	return err;
    }
    if (rrate != rate) {
	g_error("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
	return -EINVAL;
    }
    /* set the buffer size */

    if((err = snd_pcm_hw_params_set_buffer_size(handle, params, period_size * periods)) < 0) 
    {
	g_error("Unable to set buffer size %zi for playback: %s\n", period_size * periods , snd_strerror(err));
	return err;
    }

//    err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
//    if (err < 0) {
//	g_error("Unable to get buffer size for playback: %s\n", snd_strerror(err));
//	return err;
//    }

    /* set the period size */

    if((err = snd_pcm_hw_params_set_period_size(handle, params, period_size, 0)) < 0) 
    {
	g_error("Unable to set period size %zi for playback: %s\n", period_size, snd_strerror(err));
	return err;
    }


//    err = snd_pcm_hw_params_get_period_size(params, &period_size, &dir);
//    if (err < 0) {
//	g_error("Unable to get period size for playback: %s\n", snd_strerror(err));
//	return err;
//   }
//    g_error("period_size = %d \n",period_size);

    /* write the parameters to device */
    if((err = snd_pcm_hw_params(handle, params)) < 0) 
    {
	g_error("Unable to set hw params for playback: %s\n", snd_strerror(err));
	return err;
    }
    return 0;
}



static int set_swparamsV3(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams, snd_pcm_uframes_t PeriodSizeInFrames,int threshold)
{
    int err;

    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0) {
	g_error("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, PeriodSizeInFrames *  (snd_pcm_uframes_t)threshold);
    if (err < 0) {
	g_error("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
	return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, PeriodSizeInFrames);
    if (err < 0) {
	g_error("Unable to set avail min for playback: %s\n", snd_strerror(err));
	return err;
    }
#if 0
    /* align all transfers to 1 sample */
    err = snd_pcm_sw_params_set_xfer_align(handle, swparams, 1);
    if (err < 0) {
	g_error("Unable to set transfer align for playback: %s\n", snd_strerror(err));
	return err;
    }
#endif
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
	g_error("Unable to set sw params for playback: %s\n", snd_strerror(err));
	return err;
    }
    return 0;
}

/* Always writes the first "period_size" bytes in the snd_buffer.
   Returns the number of "spare" samples */

extern unsigned char *snd_buffer_p;
int snd_buffer_size;

static snd_pcm_sframes_t xrun_recovery(snd_pcm_t *handle, snd_pcm_sframes_t err)
{
    g_info(" xrun_recovery \n");

    if (err == -EPIPE) {    /* under-run */
	err = snd_pcm_prepare(handle);
	if (err < 0)
	    g_error("Can't recovery from underrun, prepare failed: %s\n", snd_strerror((int)err));
	return 0;
    } else if (err == -ESTRPIPE) {
	while ((err = snd_pcm_resume(handle)) == -EAGAIN)
	    sleep(1);       /* wait until the suspend flag is released */
	if (err < 0) {
	    err = snd_pcm_prepare(handle);
	    if (err < 0)
		g_error("Can't recovery from suspend, prepare failed: %s\n", snd_strerror((int)err));
	}
	return 0;
    }
    return err;
}


static int soundWritePeriodBuffer(snd_pcm_t *handle)
{
    gint8 *ptr;
    snd_pcm_sframes_t err; 
    int state;
    unsigned int frameCounter;
    static int init = 1;

    if (!init)
    {
	if (snd_pcm_state(handle) == SND_PCM_STATE_XRUN
	    || snd_pcm_state(handle) == SND_PCM_STATE_SUSPENDED)
	{
	    err = (snd_pcm_state(handle) == SND_PCM_STATE_XRUN ? -EPIPE
		   : -ESTRPIPE);
	    if (xrun_recovery(handle, err) < 0)
	    {
		g_error("Write error: %s\n", snd_strerror((int) err));
		exit(EXIT_FAILURE);
	    }
	    init = 1;
	}
    }

    ptr = (gint8 *) periodBuffer;
    frameCounter = FramesPerPeriod;
    while (frameCounter > 0)
    {
	err = snd_pcm_writei(handle, (void *) ptr, frameCounter);
	//printf("%s %d %d %d\n",__FUNCTION__,frameCounter,err,count++);
	if (err < 0)
	{
	    if (xrun_recovery(handle, err) < 0)
	    {
		g_error("Write error: %s\n", snd_strerror((int) err));
		exit(EXIT_FAILURE);
	    }
	    init = 1;
	    break; /* skip one period */
	}

	state = snd_pcm_state(handle);
	switch (state)
	{
	case SND_PCM_STATE_RUNNING:
	    init = 0;
	    break;
	case SND_PCM_STATE_PREPARED:
	    snd_pcm_start(handle);
	    break;

	default:
	    g_error("Unexpected state %d\n", state);
	}

	/*
	  if ((state = snd_pcm_state(handle)) == SND_PCM_STATE_RUNNING)
	  {
	  init = 0;
	  }
	  else
	  {
	  printf("state = %d\n",state);
	  }
	*/
	ptr += err ; 
	frameCounter -= (unsigned int) err;
	if (frameCounter == 0)
	    break;

    }
    return 0;
}


static int CPU_SampleCount;
static int16_t *CPU_Samples = NULL;
void addSamplesFromCPU(int16_t first,int16_t remainder)
{
    int16_t *samplePtr;
#if SOUNDDEBUG
    g_debug("%s called 0x%x 0x%x %d ",__FUNCTION__,first,remainder,CPU_SampleCount);
#endif
    if(CPU_Samples == NULL)
    {
	CPU_Samples = (int16_t *) malloc((size_t) PeriodBufferSizeInBytes);
	CPU_SampleCount = 0;
    }
    
    samplePtr = &CPU_Samples[CPU_SampleCount];

    *samplePtr++ = first;   // L
    *samplePtr++ = first;   // R

    for(int n = 1; n < iFramesPerWordTime; n++)
    {
	    *samplePtr++ = remainder;   // L
	    *samplePtr++ = remainder;   // R
    }

    CPU_SampleCount += (iFramesPerWordTime *2);

#if SOUNDDEBUG
    g_debug("%d\n",CPU_SampleCount);
#endif
}



// FIX IT 
static void addInCPU_Sound(void)
{
    int spareSamples;
    // FIX 480*2
    if(CPU_Samples != NULL)
    {
	for(int n = 0; n<(480*2); n++)
	{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
	    periodBuffer[n] += CPU_Samples[n];
#pragma GCC diagnostic pop
	}
    }
    spareSamples = (CPU_SampleCount - (480 * 2));

    for(int n = 0; n < spareSamples; n++)
    {
	CPU_Samples[n] = CPU_Samples[(480*2) + n];
    }

#if SOUNDDEBUG
    g_debug("%s CPU_SampleCount = %d spareSamples = %d\n",__FUNCTION__,CPU_SampleCount,spareSamples);
#endif    
    CPU_SampleCount = spareSamples;
}


// Add/remove sound effect generators
void addSoundHandler(void (*soundFunc)(void *buffer, int sampleCount,double time,int wordTimes))
{
        soundHandlers = g_slist_prepend(soundHandlers,(gpointer) soundFunc);
}


void removeSoundHandler(void (*soundFunc)(void *buffer, int sampleCount,double time,int wordtimes))
{
        soundHandlers = g_slist_remove(soundHandlers,(gpointer) soundFunc);
}



static gboolean soundWriteCallback(__attribute__((unused)) GSource *source,
				   __attribute__((unused))GIOCondition condition, 
				   gpointer data)
{
    static int wordTimesAdjustment = 0;

    snd_pcm_t *soundHandle;

    GSList *soundHandler;
    void (*soundFunc)(void *buffer, int sampleCount,double time,int wordtimes);

    bzero(periodBuffer,PeriodBufferSizeInBytes);

    for (soundHandler = soundHandlers; soundHandler != NULL; soundHandler = g_slist_next(soundHandler))
    {
	soundFunc = soundHandler->data;
	if (soundFunc != NULL)
	{
	    (*soundFunc)(periodBuffer,480,0.01,iWordTimesPerPeriod - wordTimesAdjustment);
	}
    }

    soundHandle = (snd_pcm_t *) data;

    addInCPU_Sound();

    wordTimesAdjustment = (CPU_SampleCount/2)  / iFramesPerWordTime;
#if SOUNDDEBUG
    g_debug("%s wordTimesAdjustment = %d\n",__FUNCTION__,wordTimesAdjustment);
#endif

    soundWritePeriodBuffer(soundHandle);

    return TRUE;
}


/*
 * frameRate = sampling frequency
 * periodRate = How many times a second ALSA will ask for the next buffer.
 * periodCount = How many buffers ALSA will use
 */

static snd_pcm_t *soundInitV3(gboolean(*handler)(GSource *,GIOCondition ,gpointer ),
		       snd_pcm_format_t SampleFormat,
		       unsigned int FrameRate,
		       int PeriodRate,
		       unsigned int PeriodCount

    )

{
    int BitsPerSample;
    double WordTime,ModifiedWordTime;
    double PeriodTime;
    double FramesPerWordTime;
    double  WordTimesPerPeriod ;
    
    int BytesPerFrame;
    

    int err;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;

    GSource *AlsaSource = NULL;
    snd_output_t *output = NULL;

    WordTime = 288.0E-6;
    PeriodTime =  1.0 / PeriodRate;
    
  
    BitsPerSample = snd_pcm_format_width(SampleFormat);
    BytesPerFrame = (((BitsPerSample-1)/8)+1) * (int) soundChannelCount;
    g_info("BytesPerFrame=%d\n",BytesPerFrame);


    /*  FramesPerWordTime needs to be nearest integer value.  
	All wordtimes need to produce the same number of samples, 
	otherwise dynamic stops (for example) will sound noisey.
    */
    FramesPerWordTime = rint(WordTime * FrameRate);
    
    g_info("FramesPerWordTime = %f\n",FramesPerWordTime);

    ModifiedWordTime = (FramesPerWordTime / FrameRate);
    g_info("ModifiedWordTime = %f Changed by %f\n",ModifiedWordTime,
	   ModifiedWordTime/WordTime);

    FramesPerPeriod = (unsigned int)  ceil(FramesPerWordTime * (PeriodTime / ModifiedWordTime));
    g_info("FramesPerPeriod = %d\n",FramesPerPeriod);

    WordTimesPerPeriod = ceil(PeriodTime / ModifiedWordTime); 
    iWordTimesPerPeriod = (int) WordTimesPerPeriod;
    g_info("WordTimesPerPeriod = %f\n", WordTimesPerPeriod );


    BytesPerWordTime = (size_t) (FramesPerWordTime * BytesPerFrame);
    iFramesPerWordTime = (int) FramesPerWordTime;

    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);

    err = snd_output_stdio_attach(&output, stdout, 0);
    if (err < 0) {
	g_error("Output failed: %s\n", snd_strerror(err));
	return NULL;
    }


    g_info("Playback device is %s\n", soundDevice);
    g_info("Stream parameters are %iHz, %s, %i channels\n",
	   FrameRate, snd_pcm_format_name(SampleFormat), soundChannelCount);

//    g_info("Using transfer method: %s\n", transfer_methods[method].name);

    if ((err = snd_pcm_open(&AlsaHandle, soundDevice, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
	g_error("Playback open error: %s\n", snd_strerror(err));
	return NULL;
    }

    if ((err = set_hwparamsV3(AlsaHandle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED,
			      PeriodCount, 
			      FramesPerPeriod,
			      FrameRate)) < 0) {
	g_error("Setting of hwparams failed: %s\n", snd_strerror(err));
	exit(EXIT_FAILURE);
    }
    if ((err = set_swparamsV3(AlsaHandle, swparams, FramesPerPeriod,3)) < 0) {
	g_error("Setting of swparams failed: %s\n", snd_strerror(err));
	exit(EXIT_FAILURE);
    }


    PeriodBufferSizeInBytes = ((FramesPerPeriod + (2 * (unsigned int) FramesPerWordTime)) *  (unsigned int)BytesPerFrame)  ;

    g_info("PeriodBufferSizeInBytes=%d\n",PeriodBufferSizeInBytes);

    periodBuffer = (int16_t *) malloc(PeriodBufferSizeInBytes );

    //periodBufferEnd = periodBuffer + PeriodBufferSizeInBytes;
    //periodBufferWrite = periodBuffer;

    bzero(periodBuffer, PeriodBufferSizeInBytes);

    g_info("periodBufferSizeInBytes=%d\n",PeriodBufferSizeInBytes);

/* set up the call back via glib */

    fdCount = (unsigned int) snd_pcm_poll_descriptors_count (AlsaHandle);
	
    g_info("%d descriptor\n",fdCount);
    g_info("snd_pcm_poll_descriptors_count=%d\n",fdCount);

    ufds = malloc(sizeof(struct pollfd) * fdCount);
    if (ufds == NULL) {
	g_error("No enough memory\n");
	return NULL;
    }

    if ((err = snd_pcm_poll_descriptors(AlsaHandle, ufds, fdCount)) < 0) {
	g_error("Unable to obtain poll descriptors for playback: %s\n", snd_strerror(err));
	return NULL;
    }

    AlsaSource = g_source_new(&AlsaSourceFuncs,sizeof(GSource));

    g_source_attach(AlsaSource,NULL);
    g_source_set_priority(AlsaSource,G_PRIORITY_DEFAULT);

    g_source_add_poll(AlsaSource,(GPollFD *)ufds);

    AlsaHandler = handler;

    return AlsaHandle;
}

void SoundInit(__attribute__((unused)) GtkBuilder *builder,
	       __attribute__((unused)) GString *sharedPath,
	       __attribute__((unused)) GString *userPath)



{
    soundInitV3(soundWriteCallback, SND_PCM_FORMAT_S16_LE,
		48000,100,4);

}
