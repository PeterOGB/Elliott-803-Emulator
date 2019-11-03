#pragma once


// Startup and Shutdown handlers.
void SoundInit(GtkBuilder *builder,
	       GString *sharedPath,
	       GString *userPath);
void SoundTidy(GString *userPath);


// Add/Remove functions that are called periodically to add sound effects.
void addSoundHandler(void (*soundFunc)(void *buffer, int sampleCount,double time,int wordTimes));
void removeSoundHandler(void (*soundFunc)(void *buffer, int sampleCount,double time,int wordTimes));


void addSamplesFromCPU(int16_t first,int16_t remainder);

size_t BytesPerWordTime;



