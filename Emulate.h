#pragma once
/* variables in the emulator that are used by network event handlers */

#define UPDATE_RATE 5
//int SS25,SS3,WI,S;
//int reader1Mode;
float channel1charTime,channel2charTime,channel3charTime;

void Emulate(int wordTimesToEmulate);
void PreEmulate(bool updateFlag);
void PostEmulate(bool updateFlag);
void StartEmulate(char *coreFileName);

//void makeSamples(int percent);

void ReadFileToBuffer(char *filename);
int getComputer_on_state(void);
int getBattery_on_state(void);
void setPTSpower(int n);
int getPTSpower(void);

//void externalRestartEmulation(void);
int texttoword(const char *text,int address);
//uint8_t *TransferAndFinish(gboolean Transfer,gboolean Finish,int count,uint8_t *data);

enum IOINSTR {F71,F72,F74,F75,F76,F77};

//uint8_t getClines(void);
//void setCPUVolume(int16_t level);
