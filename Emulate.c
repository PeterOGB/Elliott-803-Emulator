/*
 * Lower level 803 instruction emulation.
 * \author Peter Onion
 * (C) 1990-2019 P.J.Onion
 */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include "E803-types.h"
#include "E803ops.h"
#include "Emulate.h"
#include "wg-definitions.h"
#include "Wiring.h"
//#include "Sound.h"

#define FILM 0


extern void addSamplesFromCPU(int16_t first,int16_t remainder);

// Defined in Cpu.c
extern unsigned int WG_ControlButtons;    // State of the buttons like Reset etc 
//extern unsigned int WG_ControlButtonPresses;
//extern unsigned int WG_ControlButtonReleases;
extern bool WG_operate_pressed;

/* Various 803 registers */
E803word ACC;           // The Accumulator
E803word AR;            // Auxillary register used in double length working
E803word MREG;          // Multipler
E803word STORE_CHAIN;   // Data read from and written back into store
E803word WG;            // value on the Word Generator buttons
E803word QACC,QAR;      // Q register for double length multiply  
E803word MANTREG;       // Mantissa and Exponents in fp maths 
int T;                  // Number of places to shift in Gp 5 
int EXPREG;

/* Short values */
int32_t BREG;       // For B modification 
int32_t IR;         // Instruction register 
int32_t SCR;        // Sequence Control Register 
int32_t STORE_MS;   // Top half on store chain 
int32_t STORE_LS;   // Bottom half on store chain 

/* Some 803 constants */
E803word E803_ONE  = 1;
E803word E803_ZERO = 0;
E803word E803_AR_MSB= 02000000000000; // Rounding bit for Fn 53 


/* Various 803 internal sisnals */
bool S;
bool SS25;
bool WI;         // Something to do with manual data
bool SS2,SS3;    // Timing for operate to clear busy on Fn70
bool R;          //  Beat counter (R=Fetch,!R=Execute)
bool FPO;        // Gp6 / floating point overflow 
bool OFLOW;      // Integer overflow flag
bool PARITY;
bool L;          // Long function, prolongs R-bar (execute) phase
bool LW;         // Last word time in some long functions */
bool B;          // Busy
bool M;          // B Modifier flag
bool N;          // Read Button
bool ALWAYS = true;   /* used for conditional jump decoding */
bool NEGA,Z;
bool GPFOUR;     // surpresses execute phase for jumps 
bool TC;         // Transfer Contol for jumps
bool J;          // Interrrupt request (unused) 


bool *Conditions[] = {&ALWAYS,&NEGA,&Z,&OFLOW};


/* Emulation variables */
int32_t IR_saved;
/* working space */
E803word tmp;

bool *DM160s_bits[] = {&PARITY,&L,&B,&FPO,&S,&OFLOW};
int DM160s_bright[6];
bool CpuRunning = false;

int PTSBusyBright;

int16_t CPUVolume = 0x100;


/* The next two run since the emulator was started */ 
unsigned int CPU_word_time_count = 0;
unsigned int CPU_word_time_stop = 1;
/* The next one refers to the current emulation cycle */
unsigned int word_times_done =  0;

E803word *CoreStore = NULL; 

void fn00(void); void fn01(void); void fn02(void); void fn03(void);
void fn04(void); void fn05(void); void fn06(void); void fn07(void);

void fn10(void); void fn11(void); void fn12(void); void fn13(void);
void fn14(void); void fn15(void); void fn16(void); void fn17(void);

void fn20(void); void fn21(void); void fn22(void); void fn23(void);
void fn24(void); void fn25(void); void fn26(void); void fn27(void);

void fn30(void); void fn31(void); void fn32(void); void fn33(void);
void fn34(void); void fn35(void); void fn36(void); void fn37(void);

void fn40(void); void fn41(void); void fn42(void); void fn43(void);
void fn44(void); void fn45(void); void fn46(void); void fn47(void);

void fn50(void); void fn51(void); void fn52(void); void fn53(void);
void fn54(void); void fn55(void); void fn56(void); void fn57(void);

void fn60(void); void fn61(void); void fn62(void); void fn63(void);
void fn64(void); void fn65(void); void fn66(void); void fn67(void);

void fn70(void); void fn71(void); void fn72(void); void fn73(void);
void fn74(void); void fn75(void); void fn76(void); void fn77(void);


/** Jump table for 803 op codes */
void (*functions[])(void) =
{ fn00,fn01,fn02,fn03,fn04,fn05,fn06,fn07,
  fn10,fn11,fn12,fn13,fn14,fn15,fn16,fn17,
  fn20,fn21,fn22,fn23,fn24,fn25,fn26,fn27,
  fn30,fn31,fn32,fn33,fn34,fn35,fn36,fn37,
  fn40,fn41,fn42,fn43,fn44,fn45,fn46,fn47,
  fn50,fn51,fn52,fn53,fn54,fn55,fn56,fn57,
  fn60,fn61,fn62,fn63,fn64,fn65,fn66,fn67,
  fn70,fn71,fn72,fn73,fn74,fn75,fn76,fn77};


// New version 4/11/19
static void FetchStore(int32_t address,
		       int32_t *MSp, 
		       int32_t *LSp, 
		       E803word *STORE_READ)
{
    E803word readWord;
    
    address &= 8191;
    readWord = CoreStore[address];
  
    *STORE_READ = readWord;
    
    *LSp = readWord &  0xFFFFF;  /* Bottom 20 bits */

    readWord >>= 20;
    
    *MSp = readWord &  0xFFFFF; /* Top 20 bits */
}

static void WriteStore(int address, E803word *datae)
{
    address &= 8191;
    CoreStore[address] = *datae;
}

void setCPUVolume(unsigned int level)
{
    CPUVolume = level * 0x40;
}

// Called before emulate to handle button presses etc.
void PreEmulate(bool updateFlag)
{

    //printf("%s %s\n",__FUNCTION__,updateFlag?"TRUE":"FALSE");

    // Check if the machine is on 
    if(CpuRunning)
    {
	if(WG_operate_pressed)
	{
	    printf("Operate Bar pressed\n");
	    if(S) SS25 = true;
	    if(WI && !R) SS3 = true; /* 17/4/06 added !R */
	}
	if(SS25) PARITY = FPO = false;
    
    }
    if(updateFlag)
    {
	for(int n=0; n<6; n+=1)
	{
	    DM160s_bright[n] = 0;
	    
	}

    }
    
    
    PTSBusyBright = 0;
    word_times_done = 0;

    
}




void PostEmulate(bool updateFlag)
{

    if(updateFlag)
    {
	wiring(UPDATE_DISPLAYS,0);
	

    }
}

void Emulate(int wordTimesToEmulate)
{
    static int ADDRESS,fn;
    
    while(wordTimesToEmulate--)
    {
	if(CpuRunning)
	{
	    /* This is the heart of the emulation.  It fetches instructions and executes them. */
	    if (R)
	    { /* fetch */
		// 23/2/10 There may be moreto do when reset is pressed, but not reseting OFLOW was the
		// visible clue to the bug!
		if (WG_ControlButtons & WG_reset)
		{
		    OFLOW = 0;
		}
		if (!S)
		{ // Not stopped
		    if (WG_ControlButtons & WG_clear_store)
		    {
			STORE_CHAIN = E803_ZERO;
			STORE_LS = STORE_MS = 0;
			M = 0;
			if ((ADDRESS = IR & 8191) >= 4)
			{
			    CoreStore[ADDRESS] = STORE_CHAIN;
			}
		    }
		    else
		    {

			
			
			FetchStore(IR, &STORE_MS, &STORE_LS, &STORE_CHAIN);
			if (SCR & 1) /* H or D */
			{ /* F2 N2 */

			    if (M == 0)
			    { /* No B-mod */
				IR = STORE_LS;
				BREG = 0;
			    }
			    else
			    { /* B-Mod */
				
				IR = STORE_LS + BREG;
				BREG = 0;
				M = false;
			    }
			}
			else
			{ /* F1 N1 */
			    IR = STORE_MS;
			    BREG = STORE_LS; /* Save for B-mod later */
			    M = (STORE_LS & 0x80000) ? true : false;
			}
		    }
		}

		IR_saved = IR;

		/* Need to check RON to see if S should be set */
		S |= (WG_ControlButtons & (WG_read | WG_obey | WG_reset)) ? true
		    : false;


		/* Selected stop */
		if(WG_ControlButtons & WG_selected_stop)
		{
		    //int n;
		    //n = (WG.bytes[0] & 0xFF) + ((WG.bytes[1] << 8) & 0xFF00);
		    //if( n  == ((SCR >> 1) & 8191))

		    if((WG & 017777) == ((SCR >> 1) & 017777))
		    {
			S = true;
		    }
		}
		if(SS25)
		{
		    printf("SS25 UP %x\n",WG_ControlButtons );
		}
		/* Do a single instruction if operate been pressed */
		if (SS25 && (WG_ControlButtons & (WG_normal |WG_obey)))
		{
		    SS25 = S = false;
		}

		if (SS25 && (WG_ControlButtons & WG_read))
		{

		    N = true;
		    M = false;
		    SS25 = false;
		}

		
		if (!S)
		{
		    fn = (IR >> 13) & 077;
#if 0
		    if (traceChannel != NULL)
		    {
			g_string_truncate(traceText,0);	

			g_string_printf(traceText,"\nSCR= %"PRId32"%c IR = %02o %4"PRId32" \nACC=",
					(SCR>>1),SCR & 1 ? '+' : ' ', fn,IR & 8191);
			traceText = dumpWord(traceText,&ACC);

			g_string_append_printf(traceText,"\nAR =");
			traceText = dumpWord(traceText,&AR);

			g_string_append_printf(traceText,"\nSTR=");
			traceText= dumpWord(traceText,&CoreStore[IR & 8191]);

			//g_string_append_c(traceText,');
			g_io_channel_write_chars(traceChannel,traceText->str,-1,NULL,NULL);
			//printf("%s\n",traceText->str);
		    }
#endif
		    if ((fn & 070) == 040)
		    {
			GPFOUR = true;

			if (*Conditions[fn & 3]) /* if(TC)  */
			{
			    SCR = ((IR & 8191) << 1) + ((fn >> 2) & 1);
			    M = false;

			    if ((fn & 3) == 3)
				OFLOW = 0; 
			    TC = true; /* set TC */
			}
			else
			{
			    TC = false;

			    SCR += 1;
			    SCR &= 16383;

			}

			IR = SCR >> 1;
		    }

		    else
		    {
			GPFOUR = false;
			R = !R;
		    }

		    if (fn & 040)
		    {
			addSamplesFromCPU(0x0000, CPUVolume);
		    }
		    else
		    {
			addSamplesFromCPU(0x0000,0x0000);
		    }
		}
		else
		{ /* S == TRUE  --> stopped */

		    if (N)
		    {
			IR = (WG >> 20) & 0x7FFFF;
			N = false;
		    }

		    addSamplesFromCPU(0x0000,0x0000);
		}
		TC = GPFOUR = false;
	    }
	    else
	    { /* execute R-bar*/
		if (fn & 040)
		{
		    addSamplesFromCPU( CPUVolume, CPUVolume);
		}
		else
		{
		    addSamplesFromCPU(0x0000,0x0000);
		}

		ADDRESS = IR & 8191;

		if (ADDRESS >= 4)
		{
		    STORE_CHAIN = CoreStore[ADDRESS];
		}
		else
		{
		    STORE_CHAIN = E803_ZERO;
		}

		/* Call the handler for the current instruction */
		(functions[fn])();

		if (ADDRESS >= 4)
		{
		    //hash(ADDRESS,STORE_CHAIN);
		    CoreStore[ADDRESS] = STORE_CHAIN;
		}

		//Z = (ACC.bytes[0] | ACC.bytes[1] | ACC.bytes[2] | ACC.bytes[3]
		//     | ACC.bytes[4]) ? FALSE : TRUE;

		Z = (ACC & 07777777777777) ? false : true;

		//NEGA = (ACC.bytes[4] & 0x40) ? TRUE : FALSE;

		NEGA = (ACC & 04000000000000) ? true : false;
		
		/* Added PTSBusy to variables to set cleared by reset
		   Fri Aug  8 20:20:31 BST 1997*/
		{

		    if (WG_ControlButtons & (WG_reset | WG_clear_store))
		    {

			OFLOW = B = L = J = 0; /*= F77State*/
			//reader1CharBufferWrite = reader1CharBufferRead = 0;
			// FIX THIS
//			if (WG_ControlButtons & WG_reset)
//			    doExternalDeviceFSM(STOP);
		    }
		}

		if ( !(L | B))
		{
		    R = !R;

		    SCR += 1;
		    SCR &= 16383;

		    /* If M is set, don't replace previous address in IR with SCR */
		    if (!M)
		    {
			IR = SCR >> 1;
		    }
		}


	    }

	    for(int n=0;n<6;n++)
	    {
		if(*DM160s_bits[n]) DM160s_bright[n]+=1;
	    }
	}
	else
	{  // The computer is turned off, but there are still thing to do....

	    addSamplesFromCPU(0x0000,0x0000);

	}
    }
    

}


void fn00(void)
{

}

void fn01(void)
{
    OFLOW |= E803_neg(&ACC,&ACC);
}




void fn02(void)
{
    ACC = STORE_CHAIN;
    OFLOW |= E803_add(&E803_ONE,&ACC);
}



void fn03(void)
{
    E803_and(&STORE_CHAIN,&ACC);
}

void fn04(void)
{
    OFLOW |= E803_add(&STORE_CHAIN,&ACC);
}

void fn05(void)
{
    OFLOW |= E803_sub(&STORE_CHAIN,&ACC);
}

void fn06(void)
{
    ACC = E803_ZERO;
}

void fn07(void)
{
    OFLOW |= E803_neg_add(&STORE_CHAIN,&ACC);
}


void fn10(void)
{
    tmp = ACC;
    ACC = STORE_CHAIN;
    STORE_CHAIN = tmp;
}

void fn11(void)
{
    tmp = ACC;
    OFLOW |= E803_neg(&STORE_CHAIN,&ACC);
    STORE_CHAIN = tmp;
}

void fn12(void)
{
    tmp = ACC;
    ACC = STORE_CHAIN;
    STORE_CHAIN = tmp;
    OFLOW |= E803_add(&E803_ONE,&ACC);
}

void fn13(void)
{
    tmp = ACC;
    ACC = STORE_CHAIN;
    STORE_CHAIN = tmp;
    E803_and(&STORE_CHAIN,&ACC);
}

void fn14(void)
{
    tmp = ACC;
    ACC = STORE_CHAIN;
    STORE_CHAIN = tmp;
    OFLOW |= E803_add(&STORE_CHAIN,&ACC);
}

void fn15(void)
{
    tmp = STORE_CHAIN;
    STORE_CHAIN = ACC;
    E803_sub(&tmp,&ACC);
}

void fn16(void)
{
    STORE_CHAIN = ACC;
    ACC = E803_ZERO;
}

void fn17(void)
{
    tmp = STORE_CHAIN;;
    STORE_CHAIN = ACC;
    OFLOW |= E803_neg_add(&tmp,&ACC);
}


void fn20(void)
{
    STORE_CHAIN = ACC;
}

void fn21(void)
{
    OFLOW |= E803_neg(&ACC,&STORE_CHAIN);  
}

void fn22(void)
{
    OFLOW |= E803_add(&E803_ONE,&STORE_CHAIN); 
}

void fn23(void)
{
    E803_and(&ACC,&STORE_CHAIN); 
}

void fn24(void)
{
    OFLOW |= E803_add(&ACC,&STORE_CHAIN); 
}

void fn25(void)
{
    OFLOW |= E803_neg_add(&ACC,&STORE_CHAIN); 
}

void fn26(void)
{
    STORE_CHAIN = E803_ZERO;
}

void fn27(void)
{
    OFLOW |= E803_sub(&ACC,&STORE_CHAIN); 
}


void fn30(void)
{
    ACC = STORE_CHAIN;
}

void fn31(void)
{
    ACC = STORE_CHAIN;
    OFLOW |= E803_neg(&STORE_CHAIN,&STORE_CHAIN);
}

void fn32(void)
{
    ACC = STORE_CHAIN;
    OFLOW |= E803_add(&E803_ONE,&STORE_CHAIN);
}

void fn33(void)
{
    tmp = STORE_CHAIN;
    E803_and(&ACC,&STORE_CHAIN);
    ACC = tmp;
}

void fn34(void)
{
    tmp = STORE_CHAIN;
    OFLOW |= E803_add(&ACC,&STORE_CHAIN);
    ACC = tmp;
}

void fn35(void)
{
    tmp = STORE_CHAIN;
    OFLOW |= E803_neg_add(&ACC,&STORE_CHAIN);
    ACC = tmp;
}

void fn36(void)
{
    ACC = STORE_CHAIN;
    STORE_CHAIN = E803_ZERO;
}

void fn37(void)
{
    tmp = STORE_CHAIN;
    OFLOW |= E803_sub(&ACC,&STORE_CHAIN);
    ACC = tmp;
}



void fn40(void)
{

}

void fn41(void)
{
  

}

void fn42(void)
{

}

void fn43(void)
{

}

void fn44(void)
{

}

void fn45(void)
{

}

void fn46(void)
{

}

void fn47(void)
{

}


void fn50(void)
{
    if(!L)
    {  /* First word */
	L = true;
	T = (IR & 127) - 1;
    }

    if(T--< 0)
    { /* Last Word */
	L = false;
    }
    else
    {
	E803_signed_shift_right(&ACC,&AR);
    }
}



void fn51(void)
{
    if(!L)
    {  /* First word */
	L = true;
	T = (IR & 127) - 1;
    }

    if(T--< 0)
    { /* Last Word */
	L = false;
	AR = E803_ZERO;
    }
    else
    {
	E803_unsigned_shift_right(&ACC);
    }

}


void fn52(void)
{
    int m,n,action;


    if(!L)
    {  /* First Word time */

	L = true;
	MREG = STORE_CHAIN;
	E803_Double_M(&MREG);    /* shift one bit left so that M.bytes[0] &
				    3 gives the action code */
	E803_Acc_to_Q(&ACC,&QACC,&QAR);
      
	ACC = AR = E803_ZERO;
	return;
    }

    action = MREG & 3;
    switch(action)
    {
	case 0:   break;

	case 1:   /* Add */
	    OFLOW |= E803_dadd(&QACC,&QAR,&ACC,&AR);
	    break;

	case 2:   /* Subtract */
	    OFLOW |= E803_dsub(&QACC,&QAR,&ACC,&AR);
	    break;
		
	case 3:   break;
    }

    E803_shift_left(&QACC,&QAR);
    n = E803_Shift_M_Right(&MREG);
    m = (n >> 8) & 0xFF;
    n = n & 0xFF;

    if( (n == 0xFF) || (m == 0x00) )
    {
	L = false;
    }
}

void fn53(void)
{
    int m,n,action;


    if(!L)
    {  /* First Word time */

	L = true;
	MREG = STORE_CHAIN;
	E803_Double_M(&MREG);    /* shift one bit left so that M.bytes[0] &
				    3 gives the action code */
	E803_Acc_to_Q(&ACC,&QACC,&QAR);
      
	ACC = AR = E803_ZERO;
	return;
    }

    if(LW)
    {
	L = LW = false;
	OFLOW |= E803_dadd(&E803_ZERO,&E803_AR_MSB,&ACC,&AR);
	AR = E803_ZERO;
    }
    else
    {
	action = MREG & 3;
	switch(action)
	{
	    case 0:   break;
		   
	    case 1:   /* Add */
		OFLOW |= E803_dadd(&QACC,&QAR,&ACC,&AR);
		break;

	    case 2:   /* Subtract */
		OFLOW |= E803_dsub(&QACC,&QAR,&ACC,&AR);
		break;
		
	    case 3:   break;
	}

	E803_shift_left(&QACC,&QAR);
	n = E803_Shift_M_Right(&MREG);
	m = (n >> 8) & 0xFF;
	n = n & 0xFF;

	if( (n == 0xFF) || (m == 0x00) )
	{
	    LW = true;
	}
    }
}

void fn54(void)
{
    if(!L)
    {  /* First word */
	L = true;
	T = (IR & 127) - 1;
    }

    /* Note first and last words can be the same word for 0 bit shift */
  
    if(T-- < 0)
    {   /* Last Word */
	L = false;
    }
    else
    {
	OFLOW |= E803_shift_left(&ACC,&AR);
    }
}

void fn55(void)
{
    if(!L)
    {  /* First word */
	L = true;
	T = (IR & 127) - 1;
	AR = E803_ZERO;   /* This is not quite what happens on a real 803.
			     The AR should be cleared during LW, but clearing
			     it here allows the use of the normal double
			     length left shift function */
    }

    /* Note first and last words can be the same word for 0 bit shift */
  
    if(T-- < 0)
    {   /* Last Word */
	L = false;
	AR = E803_ZERO;
    }
    else
    {
	OFLOW |= E803_shift_left(&ACC,&AR);
    }
}



void fn56(void)
{
    static E803word M_sign;
    E803word ACC_sign;

    if(!L)
    {  /* First Word time */
	uint64_t *m,mm,n;

    

	L = true;
	MREG = STORE_CHAIN;

	/* Bug fixed 11/4/2010   The remainder needs an extra bit so
	   MREG and ACC/AR are sign extended to 40 bits and special 
	   versions of add and subtract are used.
	   Oddly the listings of the DOS version from 1992 have 
	   code to shift these values one place right. 
	*/

	m =  &MREG;
	mm = n = *m;
	n  &= 0x4000000000LL;   // Sign bit
	mm &= 0x7FFFFFFFFFLL;   // 39 bits

	n *= 6; // Two duplicate sign bits
	*m =mm | n;
	
	m =  &ACC;
	mm = n = *m;
	n  &= 0x4000000000LL;   // Sign bit
	mm &= 0x7FFFFFFFFFLL;   // 39 bits

	n *= 6; // Two duplicate sign bits
	*m =mm | n;
	
	M_sign = MREG;
       
	T = 40;   /* ? */

	QAR = QACC = E803_ZERO;  /* Clear QAR so I can use the double
				    length left shift on QACC */
	/*printf("  ");       
	  dumpACCAR();
	  printf("\n");
	*/
	return;
    }

    ACC_sign = 0;
   
    if(T--)
    {
#if 0
	if(traceChannel != NULL)
	{
	    g_string_truncate(traceText,0);	
	    g_string_printf(traceText,"\n%s T=%d\nMREG=",__FUNCTION__,T);
	    dumpWord(traceText,&MREG);
	    g_string_append_printf(traceText,"\nACC =");
	    dumpWord(traceText,&ACC);
	    g_string_append_printf(traceText,"\nAR  =");
	    dumpWord(traceText,&AR);
	    g_string_append_printf(traceText,"\nQACC=");
	    dumpWord(traceText,&QACC);
      }
#endif
	if(T == 39)
	{  /* Second word */
	    ACC_sign = ACC;
	}
       
	E803_shift_left(&QACC,&QAR);
	if((ACC ^ M_sign) & 0x8000000000)
	{  /* Signs different , so add */
	    E803_add56(&MREG,&ACC);
	}
	else
	{  /* Signs same, so subtract */
	    E803_sub56(&MREG,&ACC);
	    if(T != 39) QACC |= 1;
	}
#if 0
	if(traceChannel != NULL)
	{
	  g_string_append_printf(traceText,"\nACC2=");
	  dumpWord(traceText,&ACC);
	  g_string_append_printf(traceText,"\nAR2 =");
	  dumpWord(traceText,&AR);
	  g_string_append_c(traceText, '\n');
	  g_io_channel_write_chars(traceChannel,traceText->str,-1,NULL,NULL);	  
	}
#endif

	if(T == 39)
	{
	    if(( (ACC ^ ACC_sign) & 0x8000000000) == 0)
	    {
		OFLOW |= true;
	    }
	}
	E803_shift_left56(&ACC,&AR);
    }
    else
    {   /* Last word */
	ACC = QACC;
	AR = E803_ZERO;
	L = false;
    }
}

// 6/3/10  BUG FIXED.  Fn 57 does NOT clear the AR as it is not 
// a long instruction so LW is not up.
void fn57(void)
{
    E803_AR_to_ACC(&ACC,&AR);
    //    AR = E803_ZERO;
}



/* NOTE on Gp 6 timings.
   Although on the real machine Gp 6 functions carry on into the the
   next instructions R & RBAR times (to complete standardising shift),
   for the emulation everything has to be completed before L is reset.
*/


/*

803 floating point format numbers

E803word.bytes[]       4	3	 2	 1	  0
SMMMMMMM MMMMMMMM MMMMMMMM MMMMMMmE EEEEEEEE
	           
S = replicated Sign bit
M = Mantissa    m = 2^-29  LSB of Mantisa
E = Exponent

Separated Mantissae are held as...
E803word.bytes[]       4	 3	 2	   1	    0
SSMMMMMM MMMMMMMM MMMMMMMM MMMMMMMm nn000000

n = extra mantisa bits held during calculations
*/





static void fn6X(int FN)
{
    int ACCexp,STOREexp,diff,negdiff,RightShift,LeftShift,NED,NED16,NEDBAR16,K;
    int oflw,round;
    E803word ACCmant,STOREmant,VDin,TMPmant;
  
    if(!L)
    {  /* First Word time */
	L = true;
	round = 0;

	if(FN != 062)
	{
	    ACCexp   = E803_fp_split(&ACC,&ACCmant);
	    STOREexp = E803_fp_split(&STORE_CHAIN,&STOREmant);
	}
	else
	{
	    /* Just reverse the input parameters */
	    ACCexp   = E803_fp_split(&STORE_CHAIN,&ACCmant);
	    STOREexp = E803_fp_split(&ACC,&STOREmant);
	}
/*
	if(trace != NULL)
	{
	    fprintf(trace,"ACCexp=%d ACCmant=",ACCexp);
	    dumpWordFile(trace,&ACCmant);
	    fprintf(trace," STOREexp=%d STOREmant=",STOREexp);
	    dumpWordFile(trace,&STOREmant);
	    fprintf(trace,"\n");
	}
*/
	K = NED = NED16 = NEDBAR16 = false;

	diff    = ACCexp - STOREexp;
	negdiff = STOREexp - ACCexp;

	if(diff < 0) NED = true;
       
	RightShift = 0;
       
	if(diff & 0x1E0)
	{
	    NED16 = true;
	    RightShift = 32 - ( diff & 0x1F );
	}

	if(negdiff & 0x1E0)
	{
	    NEDBAR16 = true;
	    RightShift = 32 - ( negdiff & 0x1F );
	}

	if(NED16 && NEDBAR16) RightShift = 32;

	if(NED)
	{
	    VDin = ACCmant;
	    ACCexp = STOREexp;
	}
	else
	{
	    VDin = STOREmant;
	}
	/* Since Add only keeps one extra significant bit, test bottom
	   7 for rounding bits */
	if(RightShift) round |= E803_mant_shift_right(&VDin,RightShift,7);

	if(NED)
	{
	    ACCmant = VDin;
	}
	else
	{
	    STOREmant = VDin;
	}

	if(FN == 060)
	{
/*	    if(trace != NULL)
	    {
		fprintf(trace,"FN60 before STOREmant=");
		dumpWordFile(trace,&STOREmant);
		fprintf(trace," ACCmant=");
		dumpWordFile(trace,&ACCmant);
		fprintf(trace,"\n");
	    }
*/
	    oflw = E803_mant_add(&STOREmant,&ACCmant);
/*
	    if(trace != NULL)
	    {
		fprintf(trace,"FN60 after  STOREmant=");
		dumpWordFile(trace,&STOREmant);
		fprintf(trace," ACCmant=");
		dumpWordFile(trace,&ACCmant);
		fprintf(trace," oflw=%d\n",oflw);
	    }
*/
	}
	else
	{
	    oflw = E803_mant_sub(&STOREmant,&ACCmant);
	}

	LeftShift = 0;
	if( oflw )
	{   /* The mant overflowed */
	    K = true;
	}
	else
	{   /* No overflow, so we may need to standardise */
	    /* May need to check for zero here too */

	    TMPmant = ACCmant;
	    if(E803_mant_shift_right(&TMPmant,31,7) == 0)
	    {
		ACCexp = 0;
		ACCmant = E803_ZERO;
	    }
	    else
	    {
		while( !oflw )
		{
		    TMPmant = ACCmant;
		    oflw = E803_mant_add(&TMPmant,&ACCmant);
		    LeftShift += 1;
		}
		LeftShift -= 1;
		ACCmant = TMPmant;
	    }
	}

	if(K)
	{
	    RightShift = 1;
	    round |= E803_mant_shift_right(&ACCmant,RightShift,7);
	    ACCexp += 1;
	}

	ACCexp -= LeftShift;
       
	if(ACCexp < 0)
	{
	    /* Exp underflow !! */
	    ACCexp = 0;
	    ACCmant = E803_ZERO;
	}

	if(ACCexp > 511)
	{
	    printf("********** FPO fn6X Exp Ovfl ***********\n");
	    S = true;
	    FPO = true;
	}

	if( ACCmant & 0x80) round = 1;
       
	if(round)
	{   /* Force the rounding bit */
	    ACCmant |= 0x100;
	}
/*
	if(trace != NULL)
	{
	    fprintf(trace,"PreJoin  ACCmant=");
	    dumpWordFile(trace,&ACCmant);
	    fprintf(trace," ACCexp%d\n",ACCexp);
	}
*/
	E803_fp_join(&ACCmant,&ACCexp,&ACC);
/*
	if(trace != NULL)
	{
	    fprintf(trace,"PostJoin ACC    =");
	    dumpWordFile(trace,&ACC);
	    fprintf(trace,"\n");
	}
*/
    }
    else
    {  /* Second word time */
	L = false;
	AR = E803_ZERO;
    }
    return;
}

void fn60(void)
{
    fn6X(060);
}

void fn61(void)
{
    fn6X(061);
}

void fn62(void)
{
    fn6X(062);
}


void fn63(void)
{
    int ACCexp,STOREexp,op,oflw,LeftShift;
    E803word TMPmant;
    static E803word ACCmant;
    static int round; 
 
    
    if(!L)
    {  /* First Word time */
	L = true;
	T = 16 ; /* ? */
	round = 0;

	ACCexp = E803_fp_split(&ACC,&ACCmant);

	/* Multiplier mantissa to MREG */
	STOREexp = E803_fp_split(&STORE_CHAIN,&MREG);

	EXPREG = ACCexp + STOREexp;
      
	MANTREG = E803_ZERO;



    }

    op = (*((int *) &MREG) >> 7) & 0x7;
  
    round += E803_mant_shift_right(&MANTREG,2,6);  /* Note 2 extra bits */  
    switch(op)
    {

	case 0:
	    break;
	case 1:
	case 2:
	    E803_mant_add(&ACCmant,&MANTREG);
	    break;
	case 3:
	    E803_mant_add(&ACCmant,&MANTREG);
	    E803_mant_add(&ACCmant,&MANTREG);
	    break;
	    
	case 4:
	    E803_mant_sub(&ACCmant,&MANTREG);
	    E803_mant_sub(&ACCmant,&MANTREG);
	    break;
	case 5:
	case 6:
	    E803_mant_sub(&ACCmant,&MANTREG);
	    break;
	case 7:
	    break;
    }

    E803_mant_shift_right(&MREG,2,1);

    T -= 1;
    if( T == 1)
    {  /* This is the word time when "end" is set */
	MREG = E803_ZERO;
    }

    if(T == 0)
    {  /* This is the AS & FR & SD word times combined */
      
	L = 0;
	oflw = 0;
	LeftShift = 0;
	TMPmant = MANTREG;
	if(E803_mant_shift_right(&TMPmant,31,7) == 0)
	{
	    ACCexp = 0;
	    ACCmant = E803_ZERO; 	
	    /* Bug fixed 27/3/05   Multiply by zero was NOT giving zero result!
	       The next to lines were needed */
	    EXPREG = 0;
	    MANTREG = E803_ZERO;
	}
	else
	{
	    while( !oflw )
	    {
		TMPmant = MANTREG;
		oflw = E803_mant_add(&TMPmant,&MANTREG);
		LeftShift += 1;
	    }
	    LeftShift -= 1;
	    MANTREG = TMPmant;
  
	    EXPREG -= (255 + LeftShift);

	    if(EXPREG < 0)
	    {
		/* Exp underflow !! */
		EXPREG = 0;
		MANTREG = E803_ZERO;
		round = 0;
	    }
       
	    if(EXPREG > 511)
	    {
		printf("**********FPO  Fn63 Exp Ovfw ***********\n");
	    }
      
	    /* Do I need to check the MANTREG for bits which should set
	       round ?   I think I do.... */

	    TMPmant = MANTREG;
	    round |= E803_mant_shift_right(&TMPmant,2,6);
      
	    if(round)
	    {   /* Force the rounding bit */
		MANTREG |= 0x100;
	    }
	}

	E803_fp_join(&MANTREG,&EXPREG,&ACC);
	AR = E803_ZERO;
    }
    return;
}

  
//#define DIV_CNT 31

void fn64(void)
{
    static E803word TBIT =  0x1000000000; //   { .bytes={ 0,0,0,0,0x10,0,0,0}};  /* This may be wrong */
    static E803word TsignBit = 0xE000000000; //{ .bytes={ 0,0,0,0,0xE0,0,0,0}};
    static E803word TshiftBit,TBit;
    static E803word ACCmant;
    int MantZ,Same,oflw,LeftShift,STOREexp,ACCexp;
    static int firstbit,exact;
    E803word TMPmant;
    bool DivByZero;

    if(!L)
    {
	/* First word time */
	TBit = E803_ZERO;     /* To ignore the first bit in the answer */
	TshiftBit = TBIT;
	firstbit = true;
	T = 0;   /* 31 bits of quotient to form */
	L = true;
	
	exact = false;
       
	/* divisor  mantissa from store  to MREG */
	STOREexp = E803_fp_split(&STORE_CHAIN,&MREG);

	/* Split the dividend */
	ACCexp = E803_fp_split(&ACC,&ACCmant);

	EXPREG = ACCexp - STOREexp;
      
	MANTREG = E803_ZERO;   /* Form result in here */
	QACC = E803_ZERO;
      
	E803_mant_shift_right(&ACCmant,1,1); 

	//DivByZero = (STORE_CHAIN.bytes[0] | STORE_CHAIN.bytes[1] | 
	//	     STORE_CHAIN.bytes[2] | STORE_CHAIN.bytes[3] |
	//	     STORE_CHAIN.bytes[4]) ? false : true;
	DivByZero = (STORE_CHAIN != 0)  ? false : true;
	
	if(DivByZero)
	{
	    //printf("********** FPO fn64 DivByZero ***********\n");
	    S = true;
	    FPO = true;
	}

    }
    else
    {
	Same = !((ACCmant ^ MREG) & 0x8000000000);
	MantZ = (ACCmant != 0) ? false : true;

	if(MantZ) exact = true;

	if(MantZ || Same)
	{
	    E803_mant_sub(&MREG,&ACCmant);
	    E803_mant_add(&TBit,&MANTREG);
	}
	else
	{
	    E803_mant_add(&MREG,&ACCmant);
	}
       
	E803_mant_add(&ACCmant,&ACCmant);

	if(firstbit)
	{
	    TBit = TsignBit;
	    firstbit = false;
	}
	else
	{
	    TBit = TshiftBit;
	    E803_mant_shift_right(&TshiftBit,1,1);
	}

	T += 1;

	if( (T == 32) || (MantZ))
	{

	    L = 0;

	    oflw = 0;
	    LeftShift = 0;

	    MantZ = (MANTREG != 0) ? false : true;

	    if(MantZ)
	    {
		EXPREG = 0;
		exact = true;
	    }
	    else
	    {
	   
		TMPmant = MANTREG;
		while( !oflw )
		{
		    TMPmant = MANTREG;
		    oflw = E803_mant_add(&TMPmant,&MANTREG);

		    if(!oflw) LeftShift += 1;
		}
		MANTREG = TMPmant;
		EXPREG += (257 - LeftShift);
	    }

	    if(!exact)
	    {   /* Force the rounding bit */
		MANTREG |= 0x100;
	    }
       
	    if(EXPREG < 0)
	    {
		/* Exp underflow !! */
		EXPREG = 0;
		MANTREG = E803_ZERO;
	    }
     	    if(EXPREG > 511)
	    {
		printf("********** FPO Fn64 Exp Oflw ***********\n");
		S = true;
		FPO = true;
	    }  
	    E803_fp_join(&MANTREG,&EXPREG,&ACC);
	    AR = E803_ZERO;
	}
    }
}


void fn65(void)
{
    int count,EXP,oflw,*ip;
    E803word temp;

    if((IR & 8191) < 4096)
    {   /* Shift */
      count = IR & 63;
      //count = IR & 0x2F;   // Fault on TNMOC's 803 on 4/4/2010

	if(count == 0) return;
      
	if(count <= 39)
	{
	    E803_rotate_left(&ACC,&count);
	}
	else
	{
	    count -= 39;
	    E803_shift_left_F65(&ACC,&count);
	}
    }
    else
    {   /* Fp standardisation */
	AR = E803_ZERO;

	EXP = (ACC != 0) ? false : true;
	if(EXP) return;
      
	oflw = false;
      
	EXP = 256+38;
      
	while(1)
	{
	    temp = ACC;
	    oflw = E803_shift_left(&ACC,&AR);
	    if(oflw) break;
	
	    EXP -= 1;
	}

	ACC = temp;
	ip = (int *) &ACC;
      
	if(*ip & 511) *ip |= 512;
      
	*ip &= ~511;
	*ip |= (EXP & 511);
    }
}

void fn66(void)
{
  
}

void fn67(void)
{

}


// Some static stubs until the PTS is implemented
static int ptsTestReady(__attribute__((unused)) int fn,__attribute__((unused)) int address)
{
    return(0);
}

static void ptsSetACT(__attribute__((unused)) int Fn,__attribute__((unused)) int address)
{

}

static E803word readTRlines(void)
{
  return(0);
}



void fn70(void)
{

    WI = B = (WG_ControlButtons & WG_manual_data) ? true : false;
  
    if(B) 
    {
	if(SS3)
	{
	    B = false;
	    WI = false;
	    SS3 = false;
	    ACC = WG;
#if 0
	    g_string_truncate(traceText,0);	
	    g_string_append_printf(traceText,"\nWG =");
	    traceText = dumpWord(traceText,&WG);
	    printf("%s\n",traceText->str);
	    printf("%p %p %p\n",&ACC,&ACC.word,&ACC.bytes);
#endif
	}
    }
    else
    {
	B = false;
	WI = false;
	SS3 = false;
	ACC = WG;
    }
}

/*
void reader1CharBufferSave(char ch)
{
    reader1CharBuffer[reader1CharBufferWrite++] = ch;
    reader1CharBufferWrite &= 0xFF;
}
*/

//extern int ptsTestReady(int fn,int address);
//extern int plotterTestReady(int fn,int address);
//extern int readTRlines(void);
//extern void setClines(int n); 
//extern void ptsSetACT(int fn,int address);
//extern void plotterSetACT(int fn,int address);
static int Blines;
static uint8_t Clines;
// TODO add F75
//enum IOINSTR {F71,F72,F74,F75,F76,F77};


static uint8_t getClines(void)
{
    return Clines;
}


static int testReady(int fn,int address)
{
  int ready = false;

  switch(fn)
  {
  case F72:
    if(address == 7168)
//      ready |= plotterTestReady(fn,address);
    if(address == 512)
      ready |= true;
   if(address == 1024)
      ready |= true;
   if(address == 1536)
      ready |= true;


    break;

  case F71:
  case F74:
      ready |= ptsTestReady(fn,address);
    break;

  case F75:

    if(address == 1027)
    {
#if FILM
      ready |= filmControllerReady(fn,address);
#endif
    }
    else if(address & 2048) {
	ready = true;
    }
    else
    {
      ready = false;
      printf("%s FN75 %d (0x%02x)\n",__FUNCTION__,address,address);
    }
    break;
  case F76:
#if FILM
    ready |= filmControllerReady(fn,address);
#endif
    break;

  default:
    printf("%s instruction not implelemnted\n",__FUNCTION__);
    break;
  }
  return(ready);
}

static void setACT(int fn,int address)
{
    switch(fn)
    {
    case F72:
	if(address == 7168)
//	    plotterSetACT(fn,address);
	if(address == 512)
	    printf("FN72 512 + %d\n",IR & 63);
	if(address == 1024)
	    printf("FN72 1024 + %d\n",IR & 63);
	if(address == 1536)
	    printf("FN72 1536 + %d\n",IR & 63);
	break;

    case F71:
    case F74:
	ptsSetACT(fn,address);
	break;

    case F75:
	if(address == 1027)
	{
#if FILM
	    filmControllerSetACT(fn,address);
#endif
	}
	else if(address & 2048)
	{
#if 0
	    struct tm *now;
	    time_t seconds;
	    static unsigned long ulseconds;
	    static int h,m,s;
	    
	    switch(address & 0x7)
	    {
	    case 0:

		seconds = time(NULL);
		ulseconds = (unsigned long) seconds;
		now = gmtime(&seconds);
		h = now->tm_hour;
		m = now->tm_min;
		s = now->tm_sec;
		Blines = 0;
		break;
	    case 1:
		Blines = h;
		break;
	    case 2:
		Blines = m;
		break;
	    case 3:
		Blines = s;
		break;
	    case 4:
		Blines = ulseconds & 0x1FFF;
		break;
	    case 5:
		Blines = (ulseconds >> 13) & 0x1FFF;
		break;
	    case 6:
		Blines = (ulseconds >> 26) & 0x1FFF;
		break;
	    }
	
#endif	    
	}
	else
	{
	    printf("F75 with address = %d\n",address);
	}
	break;

    case F76:
#if FILM
	filmControllerSetACT(fn,address);
#endif
	break;

    default:
	printf("%s instruction not implelemnted\n",__FUNCTION__);
	break;
    }

    return;
}


void fn71(void)
{
  if(testReady(F71,IR&2048))
  {
    setACT(F71,IR&2048);
    ACC |= readTRlines();
    if(B)
    {
      B = false;
    }

  }
  else
  {
    B = true;
  }

  return;
}    



#if 0
	if(!reader1Mode)
	{
		/* Normal GUI reader */
		if( reader1Act && (reader_tape_motion(1,0) == 1))
		{   
			/* char in the buffer so ACT.  Now use the
	       tape_micro_position to find the character to read */
			B = false;
			PTS_Busy = false;
			ACC.bytes[0] |= (ch = Reader_tape_buffer[0][Reader_tape_micro_position[0] / 7] & 0x1F);

			/* make the reader go busy near end of tape */
			if(((Reader_tape_micro_position[0] / 7) + 7) >  Reader_tape_buffer_size[0])
			{
				reader1Act = false;
				reader1EOT = true;
			}
		}
		else
		{  /* No char available */
			B = true;
			PTS_Busy = true;
		}
	}
	else
	{
		/* Use the real reader.  Still check reader1Act to allow virtual 
	   PTS manual button to work as expected */	
		if(!reader1Act)
		{
			B = true;
			PTS_Busy = true;
		}
		else
		{
			if(reader1CharBufferRead != reader1CharBufferWrite)
			{
				B = false;
				PTS_Busy = false;
				//printf("F71 read %02X\n",reader1CharBuffer[reader1CharBufferRead] & 0x1F);
				ACC.bytes[0] |= reader1CharBuffer[reader1CharBufferRead++] & 0x1F;
				reader1CharBufferRead &= 0xFF;
				write(reader_fd,"R",1);
				tcdrain(reader_fd);
			}
			else
			{
				doExternalDeviceFSM(FUNC71);
				B = true;
				PTS_Busy = true;
			}
		}
	}
#endif
  



int plotterAct = 0;

void fn72(void)
{

  if(testReady(F72,IR&7680))
  {
//    Clines = (IR&0x3F);
    setACT(F72,IR&7680);
    if(B)
    {
      B = false;
    }

  }
  else
  {
    B = true;
  }
}


#if 0
    unsigned char  bits;

    if(CalcompBusy || !plotterAct)
    {
	B = true;
    }
    else
    {
	CalcompBusy = true;
	bits = IR  & 0x3F;
	if( (bits & 0x30) == 0)
	{
	    start_timer(&CalcompBusy,0.003);
	}
	else
	{
	    start_timer(&CalcompBusy,0.1);
	}
	B = false;
	   
	bits = IR  & 0x3F;
	plotterMessage[plotter_moves++] = '@' +  bits;
    }
}
#endif
void fn73(void)
{
    E803_SCR_to_STORE(&SCR,&STORE_CHAIN);
}

/* Speed is now controlled using the polling techniques developed 
   for the film controler.
*/

int F74punchAt;

void fn74(void)
{

  if(testReady(F74,IR&6144))
  {
    Clines = (IR&0x1F);
    setACT(F74,IR&6144);
    if(B)
    {
      B = false;
    }

  }
  else
  {
    B = true;
  }
}




#if 0
    char line[100];

    if( (F74Ready) && (CPU_word_time_count >= F74punchAt))
    {  /* We got a reply.  Note this is the Ready & Act rolled into one ! */
	F74Ready = false;
	B = false;
	PTS_Busy = false;
    }
    else
    {
	if(!B)
	{
	    /* Go busy and send the F74 message */
	    B = true;
	    PTS_Busy = true;

//	    sprintf(line,"F74 %ld %d\n",IR & 8191,CPU_word_time_count);
//	    notifyV2(PTS,line);
	    // TODO printfMessageToClients(eventClients[PTS],"F74 %ld %d\n",IR & 8191,CPU_word_time_count);
	  
	    doExternalDeviceFSM(FUNC71);  
	}
    }
#endif
#if 0
    switch(device)
    {
	case 0:// Channel 1
	    if(channel1Act && !channel1timer)
	    {
		B = false;
		PTS_Busy = false;
		channelBuffer[channelCharsPunched++] = 0x40 | (IR & 0x1F);
		channel1timer = true;
		start_timer(&channel1timer,channel1charTime);
		return;
	    }
	    break;
	case 1:
	     if(channel2Act && !channel2timer)
	    {
		B = false;
		PTS_Busy = false;
		channelBuffer[channelCharsPunched++] = 0x80 | (IR & 0x1F);
		channel2timer = true;
		start_timer(&channel2timer,channel2charTime);
		return;
	    }
	    break;
	case 2: 
	    if(channel3Act && !channel3timer)
	    {
		B = false;
		PTS_Busy = false;
		channelBuffer[channelCharsPunched++] = 0xC0 | (IR & 0x1F);
		channel3timer = true;
		start_timer(&channel3timer,channel3charTime);
		return;
	    }
	    break;
	case 3:
	    break;
    }

    if(!B)
    {
	switch(device)
	{
	    case 0:
		if(!channel1timer) channel1GoneBusy = true;
		break;
	    case 1:
		if(!channel2timer) channel2GoneBusy = true;
		break;
	    case 2:
		if(!channel3timer) channel3GoneBusy = true;
		break;
	}
	PTS_Busy = true;
	B = true;
    }
#endif




/* 
07/10/05 Started to work on film instructions 
22/10/05 Trying to use messages to the film controler for F76 rather than 
having copy of handler control words held in the CPU.  This seems to work 
OK and now I'll be using the same technique for PTS as well  
*/
 
/*
F75 address decode
1027 FILM read last block address read or written
2048 TIMER read timer
*/



int FILM_POWER_ON = false;
int filmControllerLastBlock = 0;

void fn75(void)
{
#if 0
    time_t now;
    time(&now);

    ACC.bytes[0] = (uint8_t) now & 0xFF;
    ACC.bytes[1] = (uint8_t) (now >> 8) & 0xFF;
    ACC.bytes[2] = (uint8_t) (now >> 16) & 0xFF;
    ACC.bytes[3] = (uint8_t) (now >> 24) & 0xFF;
    ACC.bytes[4] = (uint8_t) (now >> 32) & 0xFF;
#endif
}

#if 0
  if(testReady(F75,IR&8191))
  {
    setACT(F75,IR&8191);
    //readBlines();
    ACC = E803_ZERO;
    ACC.bytes[0] = Blines & 0xFF;
    ACC.bytes[1] = (Blines >> 8) & 0x1F;

    if(B)
    {
      B = false;
    }
  }
  else
  {
    B = true;
  }
}
#endif
#if 0
void fn75old(void)
{
    char line[100];
    int address;
    struct tm *now;
    time_t seconds;
    static int h,m,s;

    if(F75Ready)
    {  /* We got a reply.  Note this is the Ready & Act rolled into one ! */
	ACC = E803_ZERO;
	ACC.bytes[0] = ACC_INPUTS & 0xFF;
	ACC.bytes[1] = (ACC_INPUTS >> 8) & 0x1F;

	F75Ready = false;
	B = false;
    }
    else
    {
	if(!B)
	{

	  address = IR & 8191;

	  

	  if(address & 2048)
	  {
	      struct tm *now;
	      time_t seconds;
	      static int h,m,s;
	      switch(address & 0x3)
	      {
	      case 0:

		  seconds = time(NULL);
	      now = gmtime(&seconds);
	      h = now->tm_hour;
	      m = now->tm_min;
	      s = now->tm_sec;


	      ACC_INPUTS = 0;
	      break;
	    case 1:
	      ACC_INPUTS = h;
	      break;
	    case 2:
	      ACC_INPUTS = m;
	      break;
	    case 3:
	      ACC_INPUTS = s;
	      break;
	    }
	    B = true;
	    F75Ready = true;
	  }
	  else
	  {

	    /* Go busy and send the F75 message */
	    B = true;
	    
	    sprintf(line,"F75 %ld\n",IR & 8191);
	    notifyV2(F75,line);
	  
	    doExternalDeviceFSM(FUNC71);
	  }  
	}
    }
}
#endif

/*
extern int filmHandlerControlWords[5];
#define HANDLER_MANUAL 1
#define HANDLER_WRITE_PERMIT 4
#define HANDLER_SEARCHING 8
*/



void fn76(void)
{
  if(testReady(F76,IR&8191))
  {
    setACT(F76,IR&8191);
    //readBlines();
    ACC = E803_ZERO;
    ACC = Blines & 0x1FFF;
    if(B)
    {
      B = false;
    }
  }
  else
  {
    B = true;
  }
}




/* New version that uses the externalDeviceFSM.
   Since this works OK it opens up all sorts of possibilities.  
   For ALL peripheral READY/ACT can be done this way ! 
   F76 can poll the film controller at full speed ! 
*/
#if 0
void fn76old(void)
{
    char line[100];
    /* I would normally avoid sending messages from within the emulate loop.... but it 
       seems to work OK !... */

//    printf("F76 %d\n",F76Ready);

    if(F76Ready)
    {  /* We got a reply.  Note this is the Ready & Act rolled into one ! */
	ACC = E803_ZERO;
	ACC.bytes[0] = ACC_INPUTS & 255;
	ACC.bytes[1] = (ACC_INPUTS >> 8) & 255;

	F76Ready = false;
	B = false;
    }
    else
    {
	if(!B)
	{
	    /* Go busy and send the F76 message */
	    B = true;
	    
	    sprintf(line,"F76 %ld\n",IR & 8191);
	    notifyV2(F76,line);

	    doExternalDeviceFSM(FUNC71);  
	}
    }
}
#endif


uint8_t storeBuffer[64*5];

static uint8_t *TransferAndFinish(__attribute__((unused)) bool Transfer,
				  __attribute__((unused))bool Finish,
				  __attribute__((unused))int count,
				  __attribute__((unused))uint8_t *data)
{
    // Needs serious reworking
    return(0);
#if 0    
  uint8_t *dataOut;
  uint8_t *pointer;
  int wordNo,byteNo,F77Address;

  dataOut = NULL;
  if(Transfer && Finish && (count == 0 ) && (data == NULL))
  {
    // Search 
    F77Finish = true;

  }


  if(Transfer &&  (count != 0) && (data != NULL))
  {
    // Read  (data from Film Controler)

    pointer = data;

    	for(wordNo = 0; wordNo < 8; wordNo += 1)
	{
	    for(byteNo = 0; byteNo < 5; byteNo += 1)
	    {
		STORE_CHAIN.bytes[byteNo] = (uint8_t) (*pointer++ & 0xFF);
	    }

	    F77Address = IR & 8191;
	    /* You can't overwrite the initial instructions !*/
	    if(F77Address > 3)
	      {
		  //hash(F77Address,STORE_CHAIN);
#if 0
		guchar b1,b2,b3,*ptr,*ptr1;
		b1 =  STORE_CHAIN.bytes[0];
		b2 =  STORE_CHAIN.bytes[1];
		b3 =  STORE_CHAIN.bytes[2];

		ptr = ptr1 = &rgbbuf[((F77Address % 64) * 6) + ((F77Address / 64) * 6 * IMAGE_WIDTH)];

		*ptr++ = b1; *ptr++ = b2; *ptr++ = b3;
		*ptr++ = b1; *ptr++ = b2; *ptr++ = b3;

		ptr1 += IMAGE_WIDTH * 3;

		*ptr1++ = b1; *ptr1++ = b2; *ptr1++ = b3;
		*ptr1++ = b1; *ptr1++ = b2; *ptr1++ = b3;
#endif

		CoreStore[F77Address] = STORE_CHAIN;
	    }
	    IR += 1;
	}


	if(Finish)
	{
	  F77Finish = true;
	  IR -= 1;
	}
  }

  if(Transfer  && (count != 0) && (data == NULL))
  {
    pointer = storeBuffer;
    // Write  (data to Film Controler)
    for(wordNo = 0; wordNo < 8; wordNo += 1)
    {


      F77Address = IR & 8191;
      /* The initial instructions read as zeros !*/
      if(F77Address > 3)
      {	    
	STORE_CHAIN = CoreStore[F77Address];
      }
      else
      {
	STORE_CHAIN = E803_ZERO;
      }
      for(byteNo = 0; byteNo < 5; byteNo += 1)
      {
	*pointer++ = STORE_CHAIN.bytes[byteNo] & 0xFF;
      }
	
      IR += 1;
    }
    if(Finish)
    {
      F77Finish = true;
      IR -= 1;
    }
    dataOut = storeBuffer;
	
  }


  return(dataOut);
  #endif
}

void fn77(void)
{
    // Needs serious reworking
    L = true;
#if 0
    int FW;

    ST = true;
    FW = ST && !L;

    if(FW) 
    {
	L = true;
	ADDRESS_COUNT = 0;
    }

    if(ST && FW) 
    {   /* First Word Time */
	LP = true;

#if FILM	  
	FChandleF77(IR&8191);
#endif  
	//	sprintf(line,"F77 %ld\n",IR & 8191);
	//notifyV2(F77,line);

	//doExternalDeviceFSM(FUNC71);  
    }

    if(LP && F77Finish)
    {
      //if(ADDRESS_COUNT != 0) IR -= 1;
	LP = false;
	F77Finish = false;
    }

    if(!LP && !FW) L = false;

    if(ST) FW = false;
    #endif
}



static void cpuPowerOn(__attribute__((unused)) unsigned int dummy)
{

    printf("%s called\n",__FUNCTION__);
    CpuRunning = true;
    S = PARITY = true;
    wiring(UPDATE_DISPLAYS,0);

}

static void cpuPowerOff(__attribute__((unused)) unsigned int dummy)
{
    CpuRunning = false;
    PARITY = false;
    wiring(UPDATE_DISPLAYS,0);

}

#if 1
static
void dumpWord(E803word word)
{
    unsigned int f1,n1,b,f2,n2;
    
    f1 = (word >> 33) & 077;
    n1 = (word >> 20) & 017777;
    b =  (word >> 19) & 01;
    f2 = (word >> 13) & 077;
    n2 = word & 017777;

    printf("%02o %4d%c%02o %4d\n",f1,n1,b?'/':':',f2,n2);
    
}
#endif

const char *T1[4] = {"264:060","224/163","555:710","431:402"};


// Called from CpuInit
void StartEmulate(char *coreImage)
{
    connectWires(SUPPLIES_ON,cpuPowerOn);
    connectWires(SUPPLIES_OFF,cpuPowerOff);
    uint64_t f1,n1,f2,n2,bbit;
    char b;
    E803word II;
    int n;

    if(coreImage == NULL)
    {
	CoreStore = (E803word *) calloc(8194,sizeof(E803word));  //8194 ???
    }
    else
    {
	CoreStore = (E803word *) coreImage;
    }

    for(n=0;n<4;n++)
    {
    
	sscanf(T1[n],"%2" SCNo64 "%" SCNu64 "%c %2" SCNo64 "%" SCNu64,&f1,&n1,&b,&f2,&n2);
	bbit = (b == '/')?1:0 ;
	//printf("%02o %4d%c%02o %4d\n",F1,N1,Bbit?'/':':',F2,N2);
	II = (f1 << 33) | (n1 << 20) | (bbit << 19) | (f2 << 13) | n2;
	dumpWord(II);
	CoreStore[n] = II;
    }

    
#if 0
   // Make sure T1 is in place.
    texttoword("264:060",0);
    texttoword("224/163",1);
    texttoword("555:710",2);
    texttoword("431:402",3);
#endif    
}



/*******************************************************************************************************/



//  Ignore every thing !
#if 0

#define FILM 0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
//#include <stdbool.h>
#include <inttypes.h>

#include <sys/time.h>
#include <asm/types.h>

#include "E803-types.h"
#include "wg-definitions.h"
#include "pts-definitions.h"
#include "externalFsm.h"
#include "Emulate.h"
#include "CPU.h"
#include "Sound.h"
#include "PTS.h"
#include "Keyboard.h"
#include "Charger.h"
#include "Reader.h"
#include "Punch.h"


#include "E803ops.h"


#include "fsm.h"

//#define IMAGE_WIDTH     128

static int debugging = 0;
//static FILE *trace=NULL;
GIOChannel *traceChannel = NULL;
static GString *traceText = NULL;


E803word *CoreStore;  /**<  Char array to hold the Core store */
extern void updateCoreDisplay(void);
extern guchar rgbbuf[];


/* Various 803 registers */
E803word ACC;           /**< The Accumulator */
E803word STORE_CHAIN;   /**< Data read from and written back into store */
E803word AR;            /**< Auxillary register used in double length working */
E803word MREG;          /**< Multipler or Multiplicand ? */
E803word QACC,QAR;      /**< Q register for double length multiply */ 
E803word MANTREG;       /**< Mantissa and Exponents in fp maths */
E803word WG;            /**< value on the Word Generator buttons */
int EXPREG;


// REMOVE unsigned long IR;       /**< Instruction register (why is this a long ?) */
extern unsigned int WG_ControlButtons;    /**< State of the buttons like Reset etc */
extern unsigned int WG_ControlButtonPresses;
extern unsigned int WG_ControlButtonReleases;

unsigned int SCRbits[39];
unsigned int ACCbits[39];
unsigned int IRbits[39];

/* Emulation variables */
int32_t IR_saved;

/* The next two run since the emulator was started */ 
unsigned int CPU_word_time_count = 0;
unsigned int CPU_word_time_stop = 1;

/* The next one refers to the current emulation cycle */
unsigned int word_times_done =  0;

unsigned int E803state;

int display_bright[6];
int PTSBusyBright;

int16_t CPUVolume = 0x100;


/* Definitions for bits in Machine state */
#define Computer_on_state  (1U<<0)  /**< Computer ON bit in #E803state */
#define Battery_on_state   (1U<<1)  /**< Battery ON bit in #E803state */
#define Step_by_step_state (1U<<2)  /**< Step by step bit in #E803state */

/* working space */
E803word tmp;
 
/* Some 803 constants */
E803word E803_ONE  = { .bytes={1,0,0,0,0,0,0,0}};     /**< 1 in 803 format */ 
E803word E803_ZERO = { .bytes={0,0,0,0,0,0,0,0}};     /**< 0 in 803 format */

/* The change to the way the aux register is stored on 4 bit machines
   menas this is dependent on the architecture now */


E803word E803_AR_MSB={ .bytes={0,0,0,0,0x20,0,0,0}};  /**< Rounding bit for Fn 53 */


/* Various 803 internal sisnals */
// TO DO : Make thee booleans.

int L;            /**< Long function, prolongs R-bar (execute) phase */
int OFLOW;        /**< Integer overflow flag */
int T;            /**< Number of places to shift in Gp 5 */
int LW;           /**< Last word time in some long functions */
int WI;           /**< Something to do with manual data */
int SS2,SS3;      /**< Timing for operate to clear busy on Fn70 */
int B;            /**< Busy */
int SS25;
int S;
int R;            /**< Beat counter (R=Fetch,!R=Execute)  */
int M;            /**< Multiply and Divide control */
int GPFOUR;       /**< surpresses execute phase for jumps */
int N;
int FPO;          /**< Gp6 / floating point overflow */
int PARITY;
int TC;           /**< True Condition for jumps */
int J;            /**< Interrrupt request (unused) */


int FN;           /* Function number from instruction register */

int32_t BREG;  /**< For B modification */
int32_t IR;    /**< Instruction register */
int32_t SCR;   /**< Sequence Control Register */
int32_t STORE_MS;  /**< Top half on store chain */
int32_t STORE_LS;  /**< Bottom half on store chain */



int ALWAYS = true;   /* used for conditional jump decoding */
int NEGA,Z;
int *Conditions[] = {&ALWAYS,&NEGA,&Z,&OFLOW};

int *display_bits[] = {&PARITY,&L,&B,&FPO,&S,&OFLOW};
//int *display_bits[] = {&R,&L,&B,&FPO,&S,&N};


int ST = FALSE;          /**< Store Transfer */
int LP = FALSE;          /**< Long Peripheral transfer */
int ADDRESS_COUNT = 0;   /**< Words transfered during F77 */
int A0 = FALSE;          /**< Controls direction for F75 */
int F77Finish = FALSE;   /**< Set by F77 finish from a peripheral */
int ACC_INPUTS = 0;


/* Some PTS stuff */

int F74Ready = FALSE;    /**< Set by F74 reply from a peripheral */

char reader1CharBuffer[256];    /**< Circular buffer for chars from a real reader */
int reader1CharBufferWrite = 0; /**< Circular buffer write index */
int reader1CharBufferRead = 0;  /**< Circular buffer read  index */

//static int reader1Mode = 0;
static int PTS_GR = FALSE;

unsigned long PTS_buttons;   /**< State of the buttons on the PTS. */
unsigned char *Reader_tape_buffer[2] = {NULL,NULL};  /**< Malloced buffers to hold tape images. */ 
unsigned int Reader_tape_buffer_size[2] = { 0,0};    /**< Size of malloced buffers. */
int readerBufferValid[2] = {FALSE,FALSE};            /**< Used to show when a tape has been read */ 
unsigned int Reader_tape_micro_position[2] = {0,0};  /**< Used to simulate tape motion in the reader */
unsigned int tapeKey[2] = {0,0};
int reader_moved[2] = {0,0};



unsigned char channelBuffer[50];    /**< Holds message to PTS for all chars punched in last period */
unsigned int channelCharsPunched = 0;

float channel1charTime = 0.01F;    /**< Timer for Channel 1 output  */ 
unsigned int channel1timer = 0;   /**< Channel 1 timer flag */
int channel1GoneBusy = 0;         /**< Used in PostEmulate to inform PTS that channel 1 is busy */

float channel2charTime = 0.01F;    /**< Timer for Channel 2 output  */ 
unsigned int channel2timer = 0;   /**< Channel 2 timer flag */
int channel2GoneBusy = 0;         /**< Used in PostEmulate to inform PTS that channel 2 is busy */

float channel3charTime = 0.1F;     /**< Timer for Channel 3 output  */ 
unsigned int channel3timer = 0;   /**< Channel 3 timer flag */
int channel3GoneBusy = 0;         /**< Used in PostEmulate to inform PTS that channel 3 is busy */


//unsigned int Reader_tape_micro_step_start[2] = {0,0};

unsigned char plotterMessage[1024];/**< Holds message to Plotter for all moves in last period */
int plotter_moves = 0;
//unsigned int CalcompBusy = FALSE; 

//void dumpWordFile(FILE *fp,E803word *wp);
static GString *dumpWord(GString *text,E803word *wp);

//void E803_SCR_to_STORE(unsigned long *, E803word *);

void fn00(void); void fn01(void); void fn02(void); void fn03(void);
void fn04(void); void fn05(void); void fn06(void); void fn07(void);

void fn10(void); void fn11(void); void fn12(void); void fn13(void);
void fn14(void); void fn15(void); void fn16(void); void fn17(void);

void fn20(void); void fn21(void); void fn22(void); void fn23(void);
void fn24(void); void fn25(void); void fn26(void); void fn27(void);

void fn30(void); void fn31(void); void fn32(void); void fn33(void);
void fn34(void); void fn35(void); void fn36(void); void fn37(void);

void fn40(void); void fn41(void); void fn42(void); void fn43(void);
void fn44(void); void fn45(void); void fn46(void); void fn47(void);

void fn50(void); void fn51(void); void fn52(void); void fn53(void);
void fn54(void); void fn55(void); void fn56(void); void fn57(void);

void fn60(void); void fn61(void); void fn62(void); void fn63(void);
void fn64(void); void fn65(void); void fn66(void); void fn67(void);

void fn70(void); void fn71(void); void fn72(void); void fn73(void);
void fn74(void); void fn75(void); void fn76(void); void fn77(void);


/** Jump table for 803 op codes */
void (*functions[])(void) =
{ fn00,fn01,fn02,fn03,fn04,fn05,fn06,fn07,
  fn10,fn11,fn12,fn13,fn14,fn15,fn16,fn17,
  fn20,fn21,fn22,fn23,fn24,fn25,fn26,fn27,
  fn30,fn31,fn32,fn33,fn34,fn35,fn36,fn37,
  fn40,fn41,fn42,fn43,fn44,fn45,fn46,fn47,
  fn50,fn51,fn52,fn53,fn54,fn55,fn56,fn57,
  fn60,fn61,fn62,fn63,fn64,fn65,fn66,fn67,
  fn70,fn71,fn72,fn73,fn74,fn75,fn76,fn77};


/** The symbols for use with notifiers */
enum events {WG_EVENTS=0,POWER,LIGHTS,CPULIGHT,READER,HAND,PTS,PLOTTER,GENRESET,F75a,F76a,F77a,TIMECHECK,SCOPE};
// Note RESET changed to GENRESET

// Note these are for 16 bit samples, and only one channel. They are expanded to stereo later.

/** Samples for IR top bit down */
gint16 *snd_00 = NULL; //[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
/** Samples for IR top bit staying up in Rbar */
gint16 *snd_11 = NULL;  //[] = {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10};
/** Samples for IR top bit being up in R */ 
gint16 *snd_01 = NULL;  //[] = {0x00,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10};

/** Mix samples into the snd_buffer 
 * @param samples Source of samples
 * @param length Number of samples to append
 * Mixes (adds) samples into snd_buffer at snd_buffer_p.
 */  

/* This assumes samples are U8 */



extern gint16 CPU_periodBuffer[];
extern int CPU_sampleCount;
//extern int framesPerWordTime;
//extern int bytesPerFrame;

#if 0
static void add_samples(int16_t *samples,int length)
{
#if 0
    int16_t *ucp;
    
    ucp = &CPU_periodBuffer[CPU_sampleCount];
    
    CPU_sampleCount += length ;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    while(length--)   *ucp++ +=  *samples++ ;
#pragma GCC diagnostic pop
#endif
}
#endif

 



#if 0
#define UP 40

void makeSamples(int percent)
{
    int n;
    int16_t up;
    
    printf("%s %d\n",__FUNCTION__,percent);

    up = (int16_t) (UP * percent);

    // Allocate buffers on first use.
    if(snd_00 == NULL)
	snd_00 = (int16_t *) malloc(BytesPerWordTime);
    if(snd_11 == NULL)
	snd_11 = (int16_t *) malloc(BytesPerWordTime);
    if(snd_01 == NULL)
	snd_01 = (int16_t *) malloc(BytesPerWordTime);


    snd_00[0] = 0x0000;
    snd_00[1] = 0x0000;

    snd_11[0] = up;
    snd_11[1] = up;
   
    snd_01[0] = 0x0000;
    snd_01[1] = up;

    for(n=2;n<iFramesPerWordTime;n++)
    {
	snd_00[n] = 0x0000;	
	snd_11[n] = up;
	snd_01[n] = up;
    }


}
#endif
#if 0
void notifyV2(int device_number,char *text)
{
	
	//printf("%s called %d %s\n",__FUNCTION__,device_number,text);
	
	
}
#endif

/** \typedef TIMER 
 * A generic timer structure 
 * \bug This shoud use the glib list handing code */
typedef struct timer
{
    unsigned int *flag;          /**< Pointer to the int to reset when timeout occures. n */
    unsigned int counter;        /**< Down counter */
    struct timer *next;
    struct timer *prev;
} TIMER;

TIMER *timer_list_head = NULL;    /**< Timer list management */ 
TIMER *timer_list_tail = NULL;    /**< Timer list management */ 


#if 0
/** 
 * Create a #TIMER and start it running 
 * @param ip pointer to the flag to reset when timer expires.
 * @param f timeout period in seconds */
 
static void start_timer(unsigned int *ip,float f)
{
    TIMER *tp;

    tp = (TIMER *) malloc(sizeof(TIMER));

    tp->flag = ip;
    tp->counter = f / 288.0E-6;
    tp->next = NULL;
    tp->prev = NULL; 
     
    if(timer_list_head == NULL)
    {
	timer_list_head = tp;
	timer_list_tail = tp;
    }
    else
    {
	timer_list_tail->next = tp;
	tp->prev = timer_list_tail;
      
	timer_list_tail = tp;
    }
}
#endif
#if 0
/** 
 * Restart a #TIMER with a new timeout. 
 * @param ip pointer to the flag to reset when timer expires.
 * @param f timeout period in seconds .
 * If there is no existing #TIMER for ip, then make a new one.*/
static void restart_timer(unsigned int *ip,float f)
{
    TIMER *tp;

    /* first check if a timer already running */

    tp = timer_list_head;
   
    while(tp)
    {
	if(tp->flag == ip)
	{
	    tp->counter = f / 288.0E-6;
	    return;
	}
	tp = tp->next;
    }

    start_timer(ip,f);
}
#endif



#if 0
#define FIRST 1
#define LAST 2
#define ONLY 3


/**
 * Run along the timer list, decrementing counters and reseting the flags
 * where counters reach zero.  It is called every 288uS = 1 word time 
 * (I.e. each time round the main emulation loop.  
 */
static void do_timers(void)
{
    TIMER *tp,*tpt;
    int flag;

    tp = timer_list_head;
   
    while(tp)
    {
	if( tp->counter-- == 0)
	{
	    *tp->flag = 0;

	    flag = 0;
	    if(tp->prev == NULL ) flag |= FIRST;
	    if(tp->next == NULL ) flag |= LAST;

	    switch(flag)
	    {
		case FIRST:
		    timer_list_head = tp->next;
		    tp->next->prev = NULL;
		    break;

		case LAST:
		    timer_list_tail = tp->prev;
		    tp->prev->next = NULL;
		    break;

		case ONLY:
		    timer_list_head = NULL;
		    timer_list_tail = NULL;
		    break;

		default:
		    tp->next->prev = tp->prev;
		    tp->prev->next = tp->next;
		    break;
	    }
	    tpt = tp;
	    tp = tp->next;
	    free(tpt);
	}
	else
	{
	    tp = tp->next;
	}
    }
}

#endif


static int doStop(__attribute__((unused)) int state,
		  __attribute__((unused)) int event,
		  __attribute__((unused)) void *data)
{

  //printf("*********** doStop %d left\n",CPU_word_time_stop - CPU_word_time_count);

     while(CPU_word_time_count < CPU_word_time_stop)
     {
	 CPU_word_time_count+=1;
	 addSamplesFromCPU(0x0000,0x0000);

     }

    return -1;
}



static int unexpected(int state,int event,__attribute__((unused)) void *data)
{
    printf("externalDeviceFSM: UNEXPECTED event %d in state %d\n",
	   event,state);
    return(-1);   /* Take next state from the table */ 
}

const char *externalDeviceStates[] =
{ "IDLE","RUNNING","WAITING","RUNTOEND","STOPPING"};
const char *externalDeviceEvents[] =
{ "SOUNDTICK","EMUDONE","FUNC71","RXCHAR","STOP"};


struct fsmtable externalDeviceTable[] = {
    {IDLE,    SOUNDTICK,   RUNNING,   NULL},
    {IDLE,    EMUDONE,     IDLE,      unexpected},
    {IDLE,    FUNC71,      IDLE,      unexpected},
    {IDLE,    RXCHAR,      IDLE,      unexpected},
    {IDLE,    STOP,        IDLE,      NULL},   

    {RUNNING, SOUNDTICK,   RUNNING,   unexpected},
    {RUNNING, EMUDONE,     IDLE,      NULL},
    {RUNNING, FUNC71,      WAITING,   NULL},
    {RUNNING, RXCHAR,      RUNNING,   unexpected},
    {RUNNING, STOP,        STOPPING,      doStop},

    {WAITING, SOUNDTICK,   RUNTOEND,  NULL},
    {WAITING, EMUDONE,     WAITING,   NULL},
    {WAITING, FUNC71,      WAITING,   unexpected},
    {WAITING, RXCHAR,      RUNNING,   NULL},
    {WAITING, STOP,        STOPPING,      doStop},

    {RUNTOEND,SOUNDTICK,   RUNTOEND,  unexpected},
    {RUNTOEND,EMUDONE,     WAITING,   NULL},
    {RUNTOEND,FUNC71,      RUNTOEND,  NULL},
    {RUNTOEND,RXCHAR,      RUNTOEND,  unexpected},
    {RUNTOEND,STOP,        STOPPING,      doStop},

    {STOPPING,SOUNDTICK,   IDLE,      doStop},
    


    {-1,-1,-1,NULL}
};

struct fsm externalDeviceFSM = { "External Device FSM",0,
				 externalDeviceTable,
				 externalDeviceStates,
				 externalDeviceEvents,
				 0,0};

/**
 * This FSM controls the way the emulation syncs to external devices.
 * It was originally implemented to support a real paper tape reader on
 * the end of a serial interface.  Now it is also used for FN75 and FN76 to
 * the Film Controller.  I should be used for PTS and Plotter devices as well.
 * 26/12/05 Started to change F74 to use this as well.
 * @param event an event from #externalDeviceFSMevents
 */
void doExternalDeviceFSM(int event)
{
    doFSM(&externalDeviceFSM,event,NULL);
}


/**
 * 
 * @param address Location in 803 store (0..8191)
 * @param MSp pointer to buffer for top half of word read from store.
 * @param LSP pointer to buffer for bottom hlaf of word read from store.
 * @param STORE_READ pointer to E803Word to recieve word read from store.  
 */
// TO DO : User the .word version
static void FetchStore(int32_t address,
		       int32_t *MSp, 
		       int32_t *LSp, 
		       E803word *STORE_READ)
{
    uint8_t *cp;

    address &= 8191;
    cp = (uint8_t *) &CoreStore[address];
  
    *STORE_READ = *(E803word *) cp;
    *LSp = *(int32_t *) cp ;
    *LSp &= 0xFFFFF;  /* Bottom 20 bits */

    *MSp = *(int32_t *) (cp + 2);
    *MSp >>= 4;
    *MSp &= 0xFFFFF; /* Top 20 bits */
}

#if 0
static void FetchStoreOld(unsigned short address,
		       unsigned long *MSp, 
		       unsigned long *LSp, 
		       E803word *STORE_READ)
{
    unsigned char *cp;

    address &= 8191;
    cp = (unsigned char *) &CoreStore[address];
  
    *STORE_READ = *(E803word *) cp;
    *LSp = *(unsigned long *) cp ;
    *LSp &= 0xFFFFF;  /* Bottom 20 bits */

    *MSp = *(unsigned long *) (cp + 2);
    *MSp >>= 4;
    *MSp &= 0xFFFFF; /* Top 20 bits */
}
#endif
/**
 * Write an E803word into store
 * @param address Destination location in core store.
 * @param data Pointer to the 803 word to be written.
 */
static void WriteStore(int address, E803word *datae)
{
    address &= 8191;
    CoreStore[address] = *datae;
}

/** Accessor function for Computer_on_state */ 
int getComputer_on_state(void)
{
    return(E803state & Computer_on_state);
}

/** Accessor function for Battery_on_state */ 
int getBattery_on_state(void)
{
    return(E803state & Battery_on_state);
}



void setCPUVolume(int16_t level)
{
    CPUVolume = level * 0x40;
}





/**
 * PreEmulate is called before Emulate !  
 */

extern GList *eventClients[];
enum power {BATTERY=1,CPU=2,ON=4,OFF=8};
void PreEmulate(gboolean updateFlag)
{
	int n;


//	 printf("PreEmulate  %ld\n",WG_ControlButtonPresses);

	if(WG_ControlButtonPresses)
	{
	  printf("%s WG_ControlButtonPresses = 0x%x\n",
		 __FUNCTION__,
		 WG_ControlButtonPresses);

		if(WG_ControlButtonPresses & WG_operate)
		{
			if(S) SS25 = true;
			if(WI && !R) SS3 = true; /* 17/4/06 added !R */
		}
		if(SS25)
		{
		    FPO = false;
		    PARITY = false;
		}
		

		if(WG_ControlButtonPresses & WG_battery_on)
		{
			if( !(E803state & Battery_on_state))
			{
				E803state |= Battery_on_state;
				
				PTS_Power_state_FSM(PPF_Battery_on);
			}

		}
		if((WG_ControlButtonPresses & WG_battery_off) &&
				(E803state & Battery_on_state))
		{
			if(E803state & Computer_on_state)
			{
				E803state &= ~Computer_on_state;
				WG_Lamp(0);
				PTS_Power_state_FSM(PPF_Computer_off);
			}
			E803state &= ~Battery_on_state;
			PTS_Power_state_FSM(PPF_Battery_off);
		}

		if(WG_ControlButtonPresses & WG_803_on)
		{
			if( (E803state & Battery_on_state) && !(E803state & Computer_on_state))
			{
				E803state |= Computer_on_state;
				if(WG_ControlButtons & (WG_read | WG_obey)) E803state |= Step_by_step_state;
				WG_Lamp(1);
				PTS_Power_state_FSM(PPF_Computer_on);	
			}
		}
		if(WG_ControlButtonPresses & WG_803_off)
		{
			if(E803state & Computer_on_state)
			{
				E803state &= Battery_on_state;  /* Reset all but Bat_on */
				WG_Lamp(0);
				PTS_Power_state_FSM(PPF_Computer_off);
			}

		}



	}
	if(WG_ControlButtonReleases)
	{
		//printf("WG_ControlButtonReleases = 0x%lx\n",WG_ControlButtonReleases);
	}

	if(updateFlag)
	{
		n = 6;
		do
		{
			display_bright[--n] = 0;
		} while(n);

		PTSBusyBright = 0;
/*
		for(reader = 0;reader < 2; reader+=1)
		{
			Reader_tape_micro_step_start[reader] = Reader_tape_micro_position[reader];
		}
*/
		

	}
	// Call 100 times a second for maths to work.
	updateCharger((gboolean) (E803state & Battery_on_state),
		      (gboolean) (E803state & Computer_on_state),
	    PTS_POWER_ON);
	word_times_done = 0;

}



extern void updateScopeDisplay(void);
extern unsigned int channelBits[2][48];
/**
 * PostEmulate is called after Emulate !
 * It mostly informs peripherals of the state of things after the 
 * emulation loop has completed.
 */

void PostEmulate(gboolean updateFlag)
{
    int n;
    int br[6];
    GString *text = NULL;

    if(debugging) printf("PostEmlate\n");

    text = g_string_new("");

    if(updateFlag)
    {
	/* Send the PTS busy lamp brightness */ 
	{
	    static int last = 0;
	    if(last != PTSBusyBright)
	    {
		last = PTSBusyBright;
	    }
	}

	/* Send the indicator states to the WG 
	   0 <= br[n] <= 4
	   divide by 8 if updating every wordtime.
	*/ 

	for(n=0;n<6;n++) br[n] = display_bright[n]; // / UPDATE_RATE; // / (8 * UPDATE_RATE); // / 25;

	updateWG(br);


/* Send scope info */

#if 0
	for(n=0;n<39;n++)
	{
	    channelBits[0][n] = ACCbits[n];
		    
	    ACCbits[n] = 0;
	}


	for(n=0;n<19;n++)
	{
	    channelBits[1][n] = IRbits[n];
	    IRbits[n] = 0;
	    channelBits[1][n+25] = SCRbits[n];
	    SCRbits[n] = 0;
	}
		
	updateScopeDisplay();
#endif

/*
  if(plotter_moves != 0)
  {
  plotterMessage[plotter_moves] = '\0';
  sprintf(line,"STEPS %s \n",plotterMessage);

  }
*/
	plotter_moves = 0;   /* ALWAYS reset this */


	/* This will need work when reader 2 is added */

	if(reader_moved[0])
	{
	    reader_moved[0] = 0;
	}

	reader1Update();

    }

    if(PTS_GR)
    {
	PTS_GR = FALSE;
	reader1CharBufferWrite = reader1CharBufferRead = 0;
    }
    g_string_free(text,TRUE);

}


extern int wordTimesLeft;

/**
 * Calls doExternalDeviceFSM(RXCHAR) and then Emulate() to continue emulation
 * after a peripheral reply is received.
 */

void externalRestartEmulation(void)
{
    if(debugging) 
	printf("externalRestartEmulation %d\n",externalDeviceFSM.state);

    if(externalDeviceFSM.state == WAITING)
    {
	doExternalDeviceFSM(RXCHAR);
	//printf("ERE at %d ",CPU_word_time_count);
	// FIX THIS
	Emulate(1);
	//printf("Until %d\n",CPU_word_time_count);
	WG_ControlButtonPresses = WG_ControlButtonReleases = 0;
    }
    else
    {
	printf("serialDoEmlation not in state 2\n");
    }
    return;

}

/**
 * This is called from the ALSA event handler #soundHandler.
 * Depending on the #e xternalDeviceFSM state it controlls
 * the main emulation loop.
 */

int last = 0;





unsigned int word_times;
//extern void reader2WordTimer(void);
//extern void plotterWordTimer(void);

/**
 * This is the main part of the emulation.
 */

void Emulate(int wordTimesToEmulate)
{
    static int ADDRESS;
    static int fn;

//    printf("%s %d\n",__FUNCTION__,wordTimesToEmulate);

#if 1
    if(traceChannel == NULL)
    {
	//trace = fopen("trace","w");
	traceChannel =  g_io_channel_new_file("trace","w",NULL);
	traceText = g_string_sized_new(150);
	    
    }
#endif


    while(wordTimesToEmulate--)
    {
	reader1WordTimer();
	ptsWordTimer();
	punchesWordTimer();
	if (E803state & Computer_on_state)
	{
	    /* This is the heart of the emulation.  It fetches instructions and executes them. */
	    if (R)
	    { /* fetch */
		// 23/2/10 There may be moreto do when reset is pressed, but not reseting OFLOW was the
		// visible clue to the bug!
		if (WG_ControlButtons & WG_reset)
		{
		    OFLOW = 0;
		}
		if (!S)
		{
		    if (WG_ControlButtons & WG_clear_store)
		    {
			STORE_CHAIN = E803_ZERO;
			STORE_LS = STORE_MS = 0;
			M = 0;
			if ((ADDRESS = IR & 8191) >= 4)
			{
			    CoreStore[ADDRESS] = STORE_CHAIN;
			}
		    }
		    else
		    {

			
			
			FetchStore(IR, &STORE_MS, &STORE_LS, &STORE_CHAIN);
			if (SCR & 1) /* H or D */
			{ /* F2 N2 */

			    if (M == 0)
			    { /* No B-mod */
				IR = STORE_LS;
				BREG = 0;
			    }
			    else
			    { /* B-Mod */
				
				IR = STORE_LS + BREG;
				BREG = 0;
				M = FALSE;
			    }
			}
			else
			{ /* F1 N1 */
			    IR = STORE_MS;
			    BREG = STORE_LS; /* Save for B-mod later */
			    M = (STORE_LS & 0x80000) ? TRUE : FALSE;
			}
		    }
		}

		IR_saved = IR;

		/* Need to check RON to see if S should be set */
		S |= (WG_ControlButtons & (WG_read | WG_obey | WG_reset)) ? TRUE
		    : FALSE;


		/* Selected stop */
		if(WG_ControlButtons & WG_selected_stop)
		{
		    int n;
		    n = (WG.bytes[0] & 0xFF) + ((WG.bytes[1] << 8) & 0xFF00);
		    if( n  == ((SCR >> 1) & 8191))
		    {
			S = TRUE;
		    }
		}
		if(SS25)
		{
		    printf("SS25 UP %x\n",WG_ControlButtons );
		}
		/* Do a single instruction if operate been pressed */
		if (SS25 && (WG_ControlButtons & (WG_normal |WG_obey)))
		{
		    SS25 = S = FALSE;
		}

		if (SS25 && (WG_ControlButtons & WG_read))
		{

		    N = TRUE;
		    M = FALSE;
		    SS25 = FALSE;
		}

		if (!S)
		{
		    fn = (IR >> 13) & 077;

		    if (traceChannel != NULL)
		    {
			g_string_truncate(traceText,0);	

			g_string_printf(traceText,"\nSCR= %"PRId32"%c IR = %02o %4"PRId32" \nACC=",
					(SCR>>1),SCR & 1 ? '+' : ' ', fn,IR & 8191);
			traceText = dumpWord(traceText,&ACC);

			g_string_append_printf(traceText,"\nAR =");
			traceText = dumpWord(traceText,&AR);

			g_string_append_printf(traceText,"\nSTR=");
			traceText= dumpWord(traceText,&CoreStore[IR & 8191]);

			//g_string_append_c(traceText,');
			g_io_channel_write_chars(traceChannel,traceText->str,-1,NULL,NULL);
			//printf("%s\n",traceText->str);
		    }

		    if ((fn & 070) == 040)
		    {
			GPFOUR = TRUE;

			if (*Conditions[fn & 3]) /* if(TC)  */
			{
			    SCR = ((IR & 8191) << 1) + ((fn >> 2) & 1);
			    M = FALSE;

			    if ((fn & 3) == 3)
				OFLOW = 0; /* Kludge */
			    TC = TRUE; /* set TC */
			}
			else
			{
			    TC = FALSE;

			    SCR += 1;
			    SCR &= 16383;

			}

			IR = SCR >> 1;
		    }

		    else
		    {
			GPFOUR = FALSE;
			R = !R;

		    }

		    if (fn & 040)
		    {
			addSamplesFromCPU(0x0000, CPUVolume);
		    }
		    else
		    {
			addSamplesFromCPU(0x0000,0x0000);
		    }
		}
		else
		{ /* S == TRUE  --> stopped */

		    if (N)
		    {
#if 1
			IR = (WG.word >> 20) & 0x7FFFF;
#else			
			IR = ((WG.bytes[4] << 12) & 0x7F000) | 
			    ((WG.bytes[3] << 4) & 0xFF0) | ((WG.bytes[2] >> 4) & 0xF);
#endif
#if 0
			printf("  %s N UP %02o %4d &WG=%p\n",__FUNCTION__,(IR >> 13) & 0x3F, IR & 8191,&WG );
#endif
			N = FALSE;
		    }

		    addSamplesFromCPU(0x0000,0x0000);
		}
		TC = GPFOUR = FALSE;
	    }
	    else
	    { /* execute  R-bar */

		if (fn & 040)
		{
		    addSamplesFromCPU( CPUVolume, CPUVolume);
		}
		else
		{
		    addSamplesFromCPU(0x0000,0x0000);
		}

		ADDRESS = IR & 8191;

		if (ADDRESS >= 4)
		{
		    STORE_CHAIN = CoreStore[ADDRESS];
		}
		else
		{
		    STORE_CHAIN = E803_ZERO;
		}

		FN = fn;

		/* Call the handler for the current instruction */
		(functions[fn])();

		if (ADDRESS >= 4)
		{
		    //hash(ADDRESS,STORE_CHAIN);
		    CoreStore[ADDRESS] = STORE_CHAIN;
		}

		Z = (ACC.bytes[0] | ACC.bytes[1] | ACC.bytes[2] | ACC.bytes[3]
		     | ACC.bytes[4]) ? FALSE : TRUE;

		NEGA = (ACC.bytes[4] & 0x40) ? TRUE : FALSE;

		/* Added PTSBusy to variables to set cleared by reset
		   Fri Aug  8 20:20:31 BST 1997*/
		{

		    if (WG_ControlButtons & (WG_reset | WG_clear_store))
		    {

			OFLOW = B = L = J = 0; /*= F77State*/
			reader1CharBufferWrite = reader1CharBufferRead = 0;
			// FIX THIS
//			if (WG_ControlButtons & WG_reset)
//			    doExternalDeviceFSM(STOP);
		    }
		}

		if ( !(L | B))
		{
		    R = !R;

		    SCR += 1;
		    SCR &= 16383;

		    /* If M is set, don't replace previous address in IR with SCR */
		    if (!M)
		    {
			IR = SCR >> 1;
		    }
		}
	    }

	    for(int n=0; n<6; n++)
	    {
		display_bright[n] += *display_bits[n];
	    }
	}
	else
	{   // The comuter is turned off, but there are still thing to do....
	    addSamplesFromCPU(0x0000,0x0000);
	}


	CPU_word_time_count += 1;
    }
}




void fn00(void)
{

}

void fn01(void)
{
    OFLOW |= E803_neg(&ACC,&ACC);
}




void fn02(void)
{
    ACC = STORE_CHAIN;
    OFLOW |= E803_add(&E803_ONE,&ACC);
}



void fn03(void)
{
    E803_and(&STORE_CHAIN,&ACC);
}

void fn04(void)
{
    OFLOW |= E803_add(&STORE_CHAIN,&ACC);
}

void fn05(void)
{
    OFLOW |= E803_sub(&STORE_CHAIN,&ACC);
}

void fn06(void)
{
    ACC = E803_ZERO;
}

void fn07(void)
{
    OFLOW |= E803_neg_add(&STORE_CHAIN,&ACC);
}



void fn10(void)
{
    tmp = ACC;
    ACC = STORE_CHAIN;
    STORE_CHAIN = tmp;
}

void fn11(void)
{
    tmp = ACC;
    OFLOW |= E803_neg(&STORE_CHAIN,&ACC);
    STORE_CHAIN = tmp;
}

void fn12(void)
{
    tmp = ACC;
    ACC = STORE_CHAIN;
    STORE_CHAIN = tmp;
    OFLOW |= E803_add(&E803_ONE,&ACC);
}

void fn13(void)
{
    tmp = ACC;
    ACC = STORE_CHAIN;
    STORE_CHAIN = tmp;
    E803_and(&STORE_CHAIN,&ACC);
}

void fn14(void)
{
    tmp = ACC;
    ACC = STORE_CHAIN;
    STORE_CHAIN = tmp;
    OFLOW |= E803_add(&STORE_CHAIN,&ACC);
}

void fn15(void)
{
    tmp = STORE_CHAIN;
    STORE_CHAIN = ACC;
    E803_sub(&tmp,&ACC);
}

void fn16(void)
{
    STORE_CHAIN = ACC;
    ACC = E803_ZERO;
}

void fn17(void)
{
    tmp = STORE_CHAIN;;
    STORE_CHAIN = ACC;
    OFLOW |= E803_neg_add(&tmp,&ACC);
}


void fn20(void)
{
    STORE_CHAIN = ACC;
}

void fn21(void)
{
    OFLOW |= E803_neg(&ACC,&STORE_CHAIN);  
}

void fn22(void)
{
    OFLOW |= E803_add(&E803_ONE,&STORE_CHAIN); 
}

void fn23(void)
{
    E803_and(&ACC,&STORE_CHAIN); 
}

void fn24(void)
{
    OFLOW |= E803_add(&ACC,&STORE_CHAIN); 
}

void fn25(void)
{
    OFLOW |= E803_neg_add(&ACC,&STORE_CHAIN); 
}

void fn26(void)
{
    STORE_CHAIN = E803_ZERO;
}

void fn27(void)
{
    OFLOW |= E803_sub(&ACC,&STORE_CHAIN); 
}


void fn30(void)
{
    ACC = STORE_CHAIN;
}

void fn31(void)
{
    ACC = STORE_CHAIN;
    OFLOW |= E803_neg(&STORE_CHAIN,&STORE_CHAIN);
}

void fn32(void)
{
    ACC = STORE_CHAIN;
    OFLOW |= E803_add(&E803_ONE,&STORE_CHAIN);
}

void fn33(void)
{
    tmp = STORE_CHAIN;
    E803_and(&ACC,&STORE_CHAIN);
    ACC = tmp;
}

void fn34(void)
{
    tmp = STORE_CHAIN;
    OFLOW |= E803_add(&ACC,&STORE_CHAIN);
    ACC = tmp;
}

void fn35(void)
{
    tmp = STORE_CHAIN;
    OFLOW |= E803_neg_add(&ACC,&STORE_CHAIN);
    ACC = tmp;
}

void fn36(void)
{
    ACC = STORE_CHAIN;
    STORE_CHAIN = E803_ZERO;
}

void fn37(void)
{
    tmp = STORE_CHAIN;
    OFLOW |= E803_sub(&ACC,&STORE_CHAIN);
    ACC = tmp;
}



void fn40(void)
{

}

void fn41(void)
{
  

}

void fn42(void)
{

}

void fn43(void)
{

}

void fn44(void)
{

}

void fn45(void)
{

}

void fn46(void)
{

}

void fn47(void)
{

}



void fn50(void)
{
    if(!L)
    {  /* First word */
	L = TRUE;
	T = (IR & 127) - 1;
    }

    if(T--< 0)
    { /* Last Word */
	L = FALSE;
    }
    else
    {
	E803_signed_shift_right(&ACC,&AR);
    }
}



void fn51(void)
{
    if(!L)
    {  /* First word */
	L = TRUE;
	T = (IR & 127) - 1;
    }

    if(T--< 0)
    { /* Last Word */
	L = FALSE;
	AR = E803_ZERO;
    }
    else
    {
	E803_unsigned_shift_right(&ACC);
    }

}


void fn52(void)
{
    int m,n,action;


    if(!L)
    {  /* First Word time */

	L = TRUE;
	MREG = STORE_CHAIN;
	E803_Double_M(&MREG);    /* shift one bit left so that M.bytes[0] &
				    3 gives the action code */
	E803_Acc_to_Q(&ACC,&QACC,&QAR);
      
	ACC = AR = E803_ZERO;
	return;
    }

    action = MREG.bytes[0] & 3;
    switch(action)
    {
	case 0:   break;

	case 1:   /* Add */
	    OFLOW |= E803_dadd(&QACC,&QAR,&ACC,&AR);
	    break;

	case 2:   /* Subtract */
	    OFLOW |= E803_dsub(&QACC,&QAR,&ACC,&AR);
	    break;
		
	case 3:   break;
    }

    E803_shift_left(&QACC,&QAR);
    n = E803_Shift_M_Right(&MREG);
    m = (n >> 8) & 0xFF;
    n = n & 0xFF;

    if( (n == 0xFF) || (m == 0x00) )
    {
	L = FALSE;
    }
}

void fn53(void)
{
    int m,n,action;


    if(!L)
    {  /* First Word time */

	L = TRUE;
	MREG = STORE_CHAIN;
	E803_Double_M(&MREG);    /* shift one bit left so that M.bytes[0] &
				    3 gives the action code */
	E803_Acc_to_Q(&ACC,&QACC,&QAR);
      
	ACC = AR = E803_ZERO;
	return;
    }

    if(LW)
    {
	L = LW = FALSE;
	OFLOW |= E803_dadd(&E803_ZERO,&E803_AR_MSB,&ACC,&AR);
	AR = E803_ZERO;
    }
    else
    {
	action = MREG.bytes[0] & 3;
	switch(action)
	{
	    case 0:   break;
		   
	    case 1:   /* Add */
		OFLOW |= E803_dadd(&QACC,&QAR,&ACC,&AR);
		break;

	    case 2:   /* Subtract */
		OFLOW |= E803_dsub(&QACC,&QAR,&ACC,&AR);
		break;
		
	    case 3:   break;
	}

	E803_shift_left(&QACC,&QAR);
	n = E803_Shift_M_Right(&MREG);
	m = (n >> 8) & 0xFF;
	n = n & 0xFF;

	if( (n == 0xFF) || (m == 0x00) )
	{
	    LW = TRUE;
	}
    }
}

void fn54(void)
{
    if(!L)
    {  /* First word */
	L = TRUE;
	T = (IR & 127) - 1;
    }

    /* Note first and last words can be the same word for 0 bit shift */
  
    if(T-- < 0)
    {   /* Last Word */
	L = FALSE;
    }
    else
    {
	OFLOW |= E803_shift_left(&ACC,&AR);
    }
}

void fn55(void)
{
    if(!L)
    {  /* First word */
	L = TRUE;
	T = (IR & 127) - 1;
	AR = E803_ZERO;   /* This is not quite what happens on a real 803.
			     The AR should be cleared during LW, but clearing
			     it here allows the use of the normal double
			     length left shift function */
    }

    /* Note first and last words can be the same word for 0 bit shift */
  
    if(T-- < 0)
    {   /* Last Word */
	L = FALSE;
	AR = E803_ZERO;
    }
    else
    {
	OFLOW |= E803_shift_left(&ACC,&AR);
    }
}



void fn56(void)
{
    static unsigned char M_sign;
    unsigned char ACC_sign;

    if(!L)
    {  /* First Word time */
      __u64 *m,mm,n;

    

	L = TRUE;
	MREG = STORE_CHAIN;

	/* Bug fixed 11/4/2010   The remainder needs an extra bit so
	   MREG and ACC/AR are sign extended to 40 bits and special 
	   versions of add and subtract are used.
	   Oddly the listings of the DOS version from 1992 have 
	   code to shift these values one place right. 
	*/

	m = (__u64 *) &MREG;
	mm = n = *m;
	n  &= 0x4000000000LL;   // Sign bit
	mm &= 0x7FFFFFFFFFLL;   // 39 bits

	n *= 6; // Two duplicate sign bits
	*m =mm | n;
	
	m = (__u64 *) &ACC;
	mm = n = *m;
	n  &= 0x4000000000LL;   // Sign bit
	mm &= 0x7FFFFFFFFFLL;   // 39 bits

	n *= 6; // Two duplicate sign bits
	*m =mm | n;
	
	M_sign = MREG.bytes[4];
       
	T = 40;   /* ? */

	QAR = QACC = E803_ZERO;  /* Clear QAR so I can use the double
				    length left shift on QACC */
	/*printf("  ");       
	  dumpACCAR();
	  printf("\n");
	*/
	return;
    }

    ACC_sign = 0;
   
    if(T--)
    {
#if 1
	if(traceChannel != NULL)
	{
	    g_string_truncate(traceText,0);	
	    g_string_printf(traceText,"\n%s T=%d\nMREG=",__FUNCTION__,T);
	    dumpWord(traceText,&MREG);
	    g_string_append_printf(traceText,"\nACC =");
	    dumpWord(traceText,&ACC);
	    g_string_append_printf(traceText,"\nAR  =");
	    dumpWord(traceText,&AR);
	    g_string_append_printf(traceText,"\nQACC=");
	    dumpWord(traceText,&QACC);
      }
#endif
	if(T == 39)
	{  /* Second word */
	    ACC_sign = ACC.bytes[4];
	}
       
	E803_shift_left(&QACC,&QAR);
	if((ACC.bytes[4] ^ M_sign) & 0x80)
	{  /* Signs different , so add */
	    E803_add56(&MREG,&ACC);
	}
	else
	{  /* Signs same, so subtract */
	    E803_sub56(&MREG,&ACC);
	    if(T != 39) QACC.bytes[0] |= 1;
	}
#if 1
	if(traceChannel != NULL)
	{
	  g_string_append_printf(traceText,"\nACC2=");
	  dumpWord(traceText,&ACC);
	  g_string_append_printf(traceText,"\nAR2 =");
	  dumpWord(traceText,&AR);
	  g_string_append_c(traceText, '\n');
	  g_io_channel_write_chars(traceChannel,traceText->str,-1,NULL,NULL);	  
	}
#endif

	if(T == 39)
	{
	    if(( (ACC.bytes[4] ^ ACC_sign) & 0x80) == 0)
	    {
		OFLOW |= TRUE;
	    }
	}
	E803_shift_left56(&ACC,&AR);
    }
    else
    {   /* Last word */
	ACC = QACC;
	AR = E803_ZERO;
	L = FALSE;
    }
}

#if 0
void fn56old(void)
{
    static unsigned char M_sign;
    unsigned char ACC_sign;

    if(!L)
    {  /* First Word time */

	L = TRUE;
	MREG = STORE_CHAIN;
	M_sign = MREG.bytes[4];
       
	T = 40;   /* ? */

	QAR = QACC = E803_ZERO;  /* Clear QAR so I can use the double
				    length left shift on QACC */
	/*printf("  ");       
	  dumpACCAR();
	  printf("\n");
	*/
	return;
    }

    ACC_sign = 0;
   
    if(T--)
    {
#if 0
      if(trace != NULL)
      {
	fprintf(trace,"\n%s T=%d\nMREG=",__FUNCTION__,T);
	dumpWordFile(trace,&MREG);
	fprintf(trace,"\nACC =");
	dumpWordFile(trace,&ACC);
	fprintf(trace,"\nAR  =");
	dumpWordFile(trace,&AR);
	fprintf(trace,"\nQACC=");
	dumpWordFile(trace,&QACC);
      }
#endif
	if(T == 39)
	{  /* Second word */
	    ACC_sign = ACC.bytes[4];
	}
       
	E803_shift_left(&QACC,&QAR);
	if((ACC.bytes[4] ^ M_sign) & 0x40)
	{  /* Signs different , so add */
	    E803_add(&MREG,&ACC);
	}
	else
	{  /* Signs same, so subtract */
	    E803_sub(&MREG,&ACC);
	    if(T != 39) QACC.bytes[0] |= 1;
	}
#if 0
	if(trace != NULL)
	{
	  fprintf(trace,"\nACC2=");
	  dumpWordFile(trace,&ACC);
	  fprintf(trace,"\nAR2 =");
	  dumpWordFile(trace,&AR);
	}
#endif

	if(T == 39)
	{
	    if(( (ACC.bytes[4] ^ ACC_sign) & 0x40) == 0)
	    {
		OFLOW |= TRUE;
	    }
	}
	E803_shift_left(&ACC,&AR);
    }
    else
    {   /* Last word */
	ACC = QACC;
	AR = E803_ZERO;
	L = FALSE;
    }
}
#endif

// 6/3/10  BUG FIXED.  Fn 57 does NOT clear the AR as it is not 
// a long instruction so LW is not up.
void fn57(void)
{
    E803_AR_to_ACC(&ACC,&AR);
    //    AR = E803_ZERO;
}


/* NOTE on Gp 6 timings.
   Although on the real machine Gp 6 functions carry on into the the
   next instructions R & RBAR times (to complete standardising shift),
   for the emulation everything has to be completed before L is reset.
*/


/*

803 floating point format numbers

E803word.bytes[]       4	3	 2	 1	  0
SMMMMMMM MMMMMMMM MMMMMMMM MMMMMMmE EEEEEEEE
	           
S = replicated Sign bit
M = Mantissa    m = 2^-29  LSB of Mantisa
E = Exponent

Separated Mantissae are held as...
E803word.bytes[]       4	 3	 2	   1	    0
SSMMMMMM MMMMMMMM MMMMMMMM MMMMMMMm nn000000

n = extra mantisa bits held during calculations
*/

void fn60(void)
{
    fn6X(060);
}

void fn61(void)
{
    fn6X(061);
}

void fn62(void)
{
    fn6X(062);
}



void fn6X(int FN)
{
    int ACCexp,STOREexp,diff,negdiff,RightShift,LeftShift,NED,NED16,NEDBAR16,K;
    int oflw,round;
    E803word ACCmant,STOREmant,VDin,TMPmant;
  
    if(!L)
    {  /* First Word time */
	L = TRUE;
	round = 0;

	if(FN != 062)
	{
	    ACCexp   = E803_fp_split(&ACC,&ACCmant);
	    STOREexp = E803_fp_split(&STORE_CHAIN,&STOREmant);
	}
	else
	{
	    /* Just reverse the input parameters */
	    ACCexp   = E803_fp_split(&STORE_CHAIN,&ACCmant);
	    STOREexp = E803_fp_split(&ACC,&STOREmant);
	}
/*
	if(trace != NULL)
	{
	    fprintf(trace,"ACCexp=%d ACCmant=",ACCexp);
	    dumpWordFile(trace,&ACCmant);
	    fprintf(trace," STOREexp=%d STOREmant=",STOREexp);
	    dumpWordFile(trace,&STOREmant);
	    fprintf(trace,"\n");
	}
*/
	K = NED = NED16 = NEDBAR16 = FALSE;

	diff    = ACCexp - STOREexp;
	negdiff = STOREexp - ACCexp;

	if(diff < 0) NED = TRUE;
       
	RightShift = 0;
       
	if(diff & 0x1E0)
	{
	    NED16 = TRUE;
	    RightShift = 32 - ( diff & 0x1F );
	}

	if(negdiff & 0x1E0)
	{
	    NEDBAR16 = TRUE;
	    RightShift = 32 - ( negdiff & 0x1F );
	}

	if(NED16 && NEDBAR16) RightShift = 32;

	if(NED)
	{
	    VDin = ACCmant;
	    ACCexp = STOREexp;
	}
	else
	{
	    VDin = STOREmant;
	}
	/* Since Add only keeps one extra significant bit, test bottom
	   7 for rounding bits */
	if(RightShift) round |= E803_mant_shift_right(&VDin,RightShift,7);

	if(NED)
	{
	    ACCmant = VDin;
	}
	else
	{
	    STOREmant = VDin;
	}

	if(FN == 060)
	{
/*	    if(trace != NULL)
	    {
		fprintf(trace,"FN60 before STOREmant=");
		dumpWordFile(trace,&STOREmant);
		fprintf(trace," ACCmant=");
		dumpWordFile(trace,&ACCmant);
		fprintf(trace,"\n");
	    }
*/
	    oflw = E803_mant_add(&STOREmant,&ACCmant);
/*
	    if(trace != NULL)
	    {
		fprintf(trace,"FN60 after  STOREmant=");
		dumpWordFile(trace,&STOREmant);
		fprintf(trace," ACCmant=");
		dumpWordFile(trace,&ACCmant);
		fprintf(trace," oflw=%d\n",oflw);
	    }
*/
	}
	else
	{
	    oflw = E803_mant_sub(&STOREmant,&ACCmant);
	}

	LeftShift = 0;
	if( oflw )
	{   /* The mant overflowed */
	    K = TRUE;
	}
	else
	{   /* No overflow, so we may need to standardise */
	    /* May need to check for zero here too */

	    TMPmant = ACCmant;
	    if(E803_mant_shift_right(&TMPmant,31,7) == 0)
	    {
		ACCexp = 0;
		ACCmant = E803_ZERO;
	    }
	    else
	    {
		while( !oflw )
		{
		    TMPmant = ACCmant;
		    oflw = E803_mant_add(&TMPmant,&ACCmant);
		    LeftShift += 1;
		}
		LeftShift -= 1;
		ACCmant = TMPmant;
	    }
	}

	if(K)
	{
	    RightShift = 1;
	    round |= E803_mant_shift_right(&ACCmant,RightShift,7);
	    ACCexp += 1;
	}

	ACCexp -= LeftShift;
       
	if(ACCexp < 0)
	{
	    /* Exp underflow !! */
	    ACCexp = 0;
	    ACCmant = E803_ZERO;
	}

	if(ACCexp > 511)
	{
	    printf("********** FPO fn6X Exp Ovfl ***********\n");
	    S = TRUE;
	    FPO = TRUE;
	}

	if( ACCmant.bytes[0] & 0x80) round = 1;
       
	if(round)
	{   /* Force the rounding bit */
	    ACCmant.bytes[1] |= 0x1;
	}
/*
	if(trace != NULL)
	{
	    fprintf(trace,"PreJoin  ACCmant=");
	    dumpWordFile(trace,&ACCmant);
	    fprintf(trace," ACCexp%d\n",ACCexp);
	}
*/
	E803_fp_join(&ACCmant,&ACCexp,&ACC);
/*
	if(trace != NULL)
	{
	    fprintf(trace,"PostJoin ACC    =");
	    dumpWordFile(trace,&ACC);
	    fprintf(trace,"\n");
	}
*/
    }
    else
    {  /* Second word time */
	L = FALSE;
	AR = E803_ZERO;
    }
    return;
}


void fn63(void)
{
    int ACCexp,STOREexp,op,oflw,LeftShift;
    E803word TMPmant;
    static E803word ACCmant;
    static int round; 
 
    
    if(!L)
    {  /* First Word time */
	L = TRUE;
	T = 16 ; /* ? */
	round = 0;

	ACCexp = E803_fp_split(&ACC,&ACCmant);

	/* Multiplier mantissa to MREG */
	STOREexp = E803_fp_split(&STORE_CHAIN,&MREG);

	EXPREG = ACCexp + STOREexp;
      
	MANTREG = E803_ZERO;



    }

    op = (*((int *) &MREG) >> 7) & 0x7;
  
    round += E803_mant_shift_right(&MANTREG,2,6);  /* Note 2 extra bits */  
    switch(op)
    {

	case 0:
	    break;
	case 1:
	case 2:
	    E803_mant_add(&ACCmant,&MANTREG);
	    break;
	case 3:
	    E803_mant_add(&ACCmant,&MANTREG);
	    E803_mant_add(&ACCmant,&MANTREG);
	    break;
	    
	case 4:
	    E803_mant_sub(&ACCmant,&MANTREG);
	    E803_mant_sub(&ACCmant,&MANTREG);
	    break;
	case 5:
	case 6:
	    E803_mant_sub(&ACCmant,&MANTREG);
	    break;
	case 7:
	    break;
    }

    E803_mant_shift_right(&MREG,2,1);

    T -= 1;
    if( T == 1)
    {  /* This is the word time when "end" is set */
	MREG = E803_ZERO;
    }

    if(T == 0)
    {  /* This is the AS & FR & SD word times combined */
      
	L = 0;
	oflw = 0;
	LeftShift = 0;
	TMPmant = MANTREG;
	if(E803_mant_shift_right(&TMPmant,31,7) == 0)
	{
	    ACCexp = 0;
	    ACCmant = E803_ZERO; 	
	    /* Bug fixed 27/3/05   Multiply by zero was NOT giving zero result!
	       The next to lines were needed */
	    EXPREG = 0;
	    MANTREG = E803_ZERO;
	}
	else
	{
	    while( !oflw )
	    {
		TMPmant = MANTREG;
		oflw = E803_mant_add(&TMPmant,&MANTREG);
		LeftShift += 1;
	    }
	    LeftShift -= 1;
	    MANTREG = TMPmant;
  
	    EXPREG -= (255 + LeftShift);

	    if(EXPREG < 0)
	    {
		/* Exp underflow !! */
		EXPREG = 0;
		MANTREG = E803_ZERO;
		round = 0;
	    }
       
	    if(EXPREG > 511)
	    {
		printf("**********FPO  Fn63 Exp Ovfw ***********\n");
	    }
      
	    /* Do I need to check the MANTREG for bits which should set
	       round ?   I think I do.... */

	    TMPmant = MANTREG;
	    round |= E803_mant_shift_right(&TMPmant,2,6);
      
	    if(round)
	    {   /* Force the rounding bit */
		MANTREG.bytes[1] |= 0x1;
	    }
	}

	E803_fp_join(&MANTREG,&EXPREG,&ACC);
	AR = E803_ZERO;
    }
    return;
}

  
//#define DIV_CNT 31

void fn64(void)
{
    static E803word TBIT =     { .bytes={ 0,0,0,0,0x10,0,0,0}};  /* This may be wrong */
    static E803word TsignBit = { .bytes={ 0,0,0,0,0xE0,0,0,0}};
    static E803word TshiftBit,TBit;
    static E803word ACCmant;
    int MantZ,Same,oflw,LeftShift,STOREexp,ACCexp;
    static int firstbit,exact;
    E803word TMPmant;
    gboolean DivByZero;

    if(!L)
    {
	/* First word time */
	TBit = E803_ZERO;     /* To ignore the first bit in the answer */
	TshiftBit = TBIT;
	firstbit = TRUE;
	T = 0;   /* 31 bits of quotient to form */
	L = TRUE;
	
	exact = FALSE;
       
	/* divisor  mantissa from store  to MREG */
	STOREexp = E803_fp_split(&STORE_CHAIN,&MREG);

	/* Split the dividend */
	ACCexp = E803_fp_split(&ACC,&ACCmant);

	EXPREG = ACCexp - STOREexp;
      
	MANTREG = E803_ZERO;   /* Form result in here */
	QACC = E803_ZERO;
      
	E803_mant_shift_right(&ACCmant,1,1); 

	DivByZero = (STORE_CHAIN.bytes[0] | STORE_CHAIN.bytes[1] | 
		     STORE_CHAIN.bytes[2] | STORE_CHAIN.bytes[3] |
		     STORE_CHAIN.bytes[4]) ? FALSE : TRUE;

	if(DivByZero)
	{
	    printf("********** FPO fn64 DivByZero ***********\n");
	    S = TRUE;
	    FPO = TRUE;

	}

    }
    else
    {
	Same = !((ACCmant.bytes[4] ^ MREG.bytes[4]) & 0x80);
	MantZ = (ACCmant.bytes[0] | ACCmant.bytes[1]  | ACCmant.bytes[2] | ACCmant.bytes[3]  | ACCmant.bytes[4]) ? FALSE : TRUE;

	if(MantZ) exact = TRUE;

	if(MantZ || Same)
	{
	    E803_mant_sub(&MREG,&ACCmant);
	    E803_mant_add(&TBit,&MANTREG);
	}
	else
	{
	    E803_mant_add(&MREG,&ACCmant);
	}
       
	E803_mant_add(&ACCmant,&ACCmant);

	if(firstbit)
	{
	    TBit = TsignBit;
	    firstbit = FALSE;
	}
	else
	{
	    TBit = TshiftBit;
	    E803_mant_shift_right(&TshiftBit,1,1);
	}

	T += 1;

	if( (T == 32) || (MantZ))
	{

	    L = 0;

	    oflw = 0;
	    LeftShift = 0;

	    MantZ = (MANTREG.bytes[0] | MANTREG.bytes[1] |  
		     MANTREG.bytes[2] | MANTREG.bytes[3] | 
		     MANTREG.bytes[4]) ? FALSE : TRUE;

	    if(MantZ)
	    {
		EXPREG = 0;
		exact = TRUE;
	    }
	    else
	    {
	   
		TMPmant = MANTREG;
		while( !oflw )
		{
		    TMPmant = MANTREG;
		    oflw = E803_mant_add(&TMPmant,&MANTREG);

		    if(!oflw) LeftShift += 1;
		}
		MANTREG = TMPmant;
		EXPREG += (257 - LeftShift);
	    }

	    if(!exact)
	    {   /* Force the rounding bit */
		MANTREG.bytes[1] |= 0x1;
	    }
       
	    if(EXPREG < 0)
	    {
		/* Exp underflow !! */
		EXPREG = 0;
		MANTREG = E803_ZERO;
	    }
     	    if(EXPREG > 511)
	    {
		printf("********** FPO Fn64 Exp Oflw ***********\n");
		S = TRUE;
		FPO = TRUE;
	    }  
	    E803_fp_join(&MANTREG,&EXPREG,&ACC);
	    AR = E803_ZERO;
	}
    }
}


void fn65(void)
{
    int count,EXP,oflw,*ip;
    E803word temp;

    if((IR & 8191) < 4096)
    {   /* Shift */
      count = IR & 63;
      //count = IR & 0x2F;   // Fault on TNMOC's 803 on 4/4/2010

	if(count == 0) return;
      
	if(count <= 39)
	{
	    E803_rotate_left(&ACC,&count);
	}
	else
	{
	    count -= 39;
	    E803_shift_left_F65(&ACC,&count);
	}
    }
    else
    {   /* Fp standardisation */
	AR = E803_ZERO;

	EXP = (ACC.bytes[0] | ACC.bytes[1]  | 
	       ACC.bytes[2] | ACC.bytes[3]  | 
	       ACC.bytes[4]) ? FALSE : TRUE;
	if(EXP) return;
      
	oflw = FALSE;
      
	EXP = 256+38;
      
	while(1)
	{
	    temp = ACC;
	    oflw = E803_shift_left(&ACC,&AR);
	    if(oflw) break;
	
	    EXP -= 1;
	}

	ACC = temp;
	ip = (int *) &ACC;
      
	if(*ip & 511) *ip |= 512;
      
	*ip &= ~511;
	*ip |= (EXP & 511);
    }
}

void fn66(void)
{
  
}

void fn67(void)
{

}



void fn70(void)
{

    WI = B = (WG_ControlButtons & WG_manual_data) ? TRUE : FALSE;
  
    if(B) 
    {
	if(SS3)
	{
	    B = FALSE;
	    WI = FALSE;
	    SS3 = FALSE;
	    ACC = WG;
#if 0
	    g_string_truncate(traceText,0);	
	    g_string_append_printf(traceText,"\nWG =");
	    traceText = dumpWord(traceText,&WG);
	    printf("%s\n",traceText->str);
	    printf("%p %p %p\n",&ACC,&ACC.word,&ACC.bytes);
#endif
	}
    }
    else
    {
	B = FALSE;
	WI = FALSE;
	SS3 = FALSE;
	ACC = WG;
    }
}

/*
void reader1CharBufferSave(char ch)
{
    reader1CharBuffer[reader1CharBufferWrite++] = ch;
    reader1CharBufferWrite &= 0xFF;
}
*/

//extern int ptsTestReady(int fn,int address);
//extern int plotterTestReady(int fn,int address);
//extern int readTRlines(void);
//extern void setClines(int n); 
//extern void ptsSetACT(int fn,int address);
//extern void plotterSetACT(int fn,int address);
static int Blines;
static uint8_t Clines;
// TODO add F75
//enum IOINSTR {F71,F72,F74,F75,F76,F77};

uint8_t getClines(void)
{
    return Clines;
}


static int testReady(int fn,int address)
{
  int ready = FALSE;

  switch(fn)
  {
  case F72:
    if(address == 7168)
//      ready |= plotterTestReady(fn,address);
    if(address == 512)
      ready |= TRUE;
   if(address == 1024)
      ready |= TRUE;
   if(address == 1536)
      ready |= TRUE;


    break;

  case F71:
  case F74:
      ready = false; // TEMPREMOVE|= ptsTestReady(fn,address);
    break;

  case F75:
/*
    if(address == 1027)
    {
#if FILM
      ready |= filmControllerReady(fn,address);
#endif
    }
    else if(address & 2048) {
	ready = TRUE;
    }
    else
    {
      ready = FALSE;
      printf("%s FN75 %d (0x%02x)\n",__FUNCTION__,address,address);
    }
*/
      ready = false;
    break;
  case F76:
      /*
#if FILM
    ready |= filmControllerReady(fn,address);
#endif
    break;

  default:
    printf("%s instruction not implelemnted\n",__FUNCTION__);
    break;
      */
      ready = false;
  }
  return(ready);
}

static void setACT(int fn,int address)
{
    return();
}
#if 0	
    switch(fn)
    {
    case F72:
	if(address == 7168)
//	    plotterSetACT(fn,address);
	if(address == 512)
	    printf("FN72 512 + %d\n",IR & 63);
	if(address == 1024)
	    printf("FN72 1024 + %d\n",IR & 63);
	if(address == 1536)
	    printf("FN72 1536 + %d\n",IR & 63);
	break;

    case F71:
    case F74:
	TEMPREMOVE ptsSetACT(fn,address);
	break;

    case F75:
	if(address == 1027)
	{
#if FILM
	    filmControllerSetACT(fn,address);
#endif
	}
	else if(address & 2048)
	{
	    struct tm *now;
	    time_t seconds;
	    static unsigned long ulseconds;
	    static int h,m,s;
	    
	    switch(address & 0x7)
	    {
	    case 0:

		seconds = time(NULL);
		ulseconds = (unsigned long) seconds;
		now = gmtime(&seconds);
		h = now->tm_hour;
		m = now->tm_min;
		s = now->tm_sec;
		Blines = 0;
		break;
	    case 1:
		Blines = h;
		break;
	    case 2:
		Blines = m;
		break;
	    case 3:
		Blines = s;
		break;
	    case 4:
		Blines = ulseconds & 0x1FFF;
		break;
	    case 5:
		Blines = (ulseconds >> 13) & 0x1FFF;
		break;
	    case 6:
		Blines = (ulseconds >> 26) & 0x1FFF;
		break;
	    }
	
	    
	}
	else
	{
	    printf("F75 with address = %d\n",address);
	}
	break;

    case F76:
#if FILM
	filmControllerSetACT(fn,address);
#endif
	break;

    default:
	printf("%s instruction not implelemnted\n",__FUNCTION__);
	break;
    }

    return;
}
#endif

void fn71(void)
{
  if(testReady(F71,IR&2048))
  {
    setACT(F71,IR&2048);
    ACC |= readTRlines();
    if(B)
    {
      B = FALSE;
    }

  }
  else
  {
    B = TRUE;
  }

  return;
}    



#if 0
	if(!reader1Mode)
	{
		/* Normal GUI reader */
		if( reader1Act && (reader_tape_motion(1,0) == 1))
		{   
			/* char in the buffer so ACT.  Now use the
	       tape_micro_position to find the character to read */
			B = FALSE;
			PTS_Busy = FALSE;
			ACC.bytes[0] |= (ch = Reader_tape_buffer[0][Reader_tape_micro_position[0] / 7] & 0x1F);

			/* make the reader go busy near end of tape */
			if(((Reader_tape_micro_position[0] / 7) + 7) >  Reader_tape_buffer_size[0])
			{
				reader1Act = FALSE;
				reader1EOT = TRUE;
			}
		}
		else
		{  /* No char available */
			B = TRUE;
			PTS_Busy = TRUE;
		}
	}
	else
	{
		/* Use the real reader.  Still check reader1Act to allow virtual 
	   PTS manual button to work as expected */	
		if(!reader1Act)
		{
			B = TRUE;
			PTS_Busy = TRUE;
		}
		else
		{
			if(reader1CharBufferRead != reader1CharBufferWrite)
			{
				B = FALSE;
				PTS_Busy = FALSE;
				//printf("F71 read %02X\n",reader1CharBuffer[reader1CharBufferRead] & 0x1F);
				ACC.bytes[0] |= reader1CharBuffer[reader1CharBufferRead++] & 0x1F;
				reader1CharBufferRead &= 0xFF;
				write(reader_fd,"R",1);
				tcdrain(reader_fd);
			}
			else
			{
				doExternalDeviceFSM(FUNC71);
				B = TRUE;
				PTS_Busy = TRUE;
			}
		}
	}
#endif
  



int plotterAct = 0;

void fn72(void)
{

  if(testReady(F72,IR&7680))
  {
//    Clines = (IR&0x3F);
    setACT(F72,IR&7680);
    if(B)
    {
      B = FALSE;
    }

  }
  else
  {
    B = TRUE;
  }
}


#if 0
    unsigned char  bits;

    if(CalcompBusy || !plotterAct)
    {
	B = TRUE;
    }
    else
    {
	CalcompBusy = TRUE;
	bits = IR  & 0x3F;
	if( (bits & 0x30) == 0)
	{
	    start_timer(&CalcompBusy,0.003);
	}
	else
	{
	    start_timer(&CalcompBusy,0.1);
	}
	B = FALSE;
	   
	bits = IR  & 0x3F;
	plotterMessage[plotter_moves++] = '@' +  bits;
    }
}
#endif
void fn73(void)
{
    E803_SCR_to_STORE(&SCR,&STORE_CHAIN);
}

/* Speed is now controlled using the polling techniques developed 
   for the film controler.
*/

int F74punchAt;

void fn74(void)
{

  if(testReady(F74,IR&6144))
  {
    Clines = (IR&0x1F);
    setACT(F74,IR&6144);
    if(B)
    {
      B = FALSE;
    }

  }
  else
  {
    B = TRUE;
  }
}




#if 0
    char line[100];

    if( (F74Ready) && (CPU_word_time_count >= F74punchAt))
    {  /* We got a reply.  Note this is the Ready & Act rolled into one ! */
	F74Ready = FALSE;
	B = FALSE;
	PTS_Busy = FALSE;
    }
    else
    {
	if(!B)
	{
	    /* Go busy and send the F74 message */
	    B = TRUE;
	    PTS_Busy = TRUE;

//	    sprintf(line,"F74 %ld %d\n",IR & 8191,CPU_word_time_count);
//	    notifyV2(PTS,line);
	    // TODO printfMessageToClients(eventClients[PTS],"F74 %ld %d\n",IR & 8191,CPU_word_time_count);
	  
	    doExternalDeviceFSM(FUNC71);  
	}
    }
#endif
#if 0
    switch(device)
    {
	case 0:// Channel 1
	    if(channel1Act && !channel1timer)
	    {
		B = FALSE;
		PTS_Busy = FALSE;
		channelBuffer[channelCharsPunched++] = 0x40 | (IR & 0x1F);
		channel1timer = TRUE;
		start_timer(&channel1timer,channel1charTime);
		return;
	    }
	    break;
	case 1:
	     if(channel2Act && !channel2timer)
	    {
		B = FALSE;
		PTS_Busy = FALSE;
		channelBuffer[channelCharsPunched++] = 0x80 | (IR & 0x1F);
		channel2timer = TRUE;
		start_timer(&channel2timer,channel2charTime);
		return;
	    }
	    break;
	case 2: 
	    if(channel3Act && !channel3timer)
	    {
		B = FALSE;
		PTS_Busy = FALSE;
		channelBuffer[channelCharsPunched++] = 0xC0 | (IR & 0x1F);
		channel3timer = TRUE;
		start_timer(&channel3timer,channel3charTime);
		return;
	    }
	    break;
	case 3:
	    break;
    }

    if(!B)
    {
	switch(device)
	{
	    case 0:
		if(!channel1timer) channel1GoneBusy = TRUE;
		break;
	    case 1:
		if(!channel2timer) channel2GoneBusy = TRUE;
		break;
	    case 2:
		if(!channel3timer) channel3GoneBusy = TRUE;
		break;
	}
	PTS_Busy = TRUE;
	B = TRUE;
    }
#endif




/* 
07/10/05 Started to work on film instructions 
22/10/05 Trying to use messages to the film controler for F76 rather than 
having copy of handler control words held in the CPU.  This seems to work 
OK and now I'll be using the same technique for PTS as well  
*/
 
/*
F75 address decode
1027 FILM read last block address read or written
2048 TIMER read timer
*/



int FILM_POWER_ON = FALSE;
int filmControllerLastBlock = 0;

void fn75(void)
{

    time_t now;
    time(&now);

    ACC.bytes[0] = (uint8_t) now & 0xFF;
    ACC.bytes[1] = (uint8_t) (now >> 8) & 0xFF;
    ACC.bytes[2] = (uint8_t) (now >> 16) & 0xFF;
    ACC.bytes[3] = (uint8_t) (now >> 24) & 0xFF;
    ACC.bytes[4] = (uint8_t) (now >> 32) & 0xFF;
}
#if 0
  if(testReady(F75,IR&8191))
  {
    setACT(F75,IR&8191);
    //readBlines();
    ACC = E803_ZERO;
    ACC.bytes[0] = Blines & 0xFF;
    ACC.bytes[1] = (Blines >> 8) & 0x1F;

    if(B)
    {
      B = FALSE;
    }
  }
  else
  {
    B = TRUE;
  }
}
#endif
#if 0
void fn75old(void)
{
    char line[100];
    int address;
    struct tm *now;
    time_t seconds;
    static int h,m,s;

    if(F75Ready)
    {  /* We got a reply.  Note this is the Ready & Act rolled into one ! */
	ACC = E803_ZERO;
	ACC.bytes[0] = ACC_INPUTS & 0xFF;
	ACC.bytes[1] = (ACC_INPUTS >> 8) & 0x1F;

	F75Ready = FALSE;
	B = FALSE;
    }
    else
    {
	if(!B)
	{

	  address = IR & 8191;

	  

	  if(address & 2048)
	  {
	      struct tm *now;
	      time_t seconds;
	      static int h,m,s;
	      switch(address & 0x3)
	      {
	      case 0:

		  seconds = time(NULL);
	      now = gmtime(&seconds);
	      h = now->tm_hour;
	      m = now->tm_min;
	      s = now->tm_sec;


	      ACC_INPUTS = 0;
	      break;
	    case 1:
	      ACC_INPUTS = h;
	      break;
	    case 2:
	      ACC_INPUTS = m;
	      break;
	    case 3:
	      ACC_INPUTS = s;
	      break;
	    }
	    B = TRUE;
	    F75Ready = TRUE;
	  }
	  else
	  {

	    /* Go busy and send the F75 message */
	    B = TRUE;
	    
	    sprintf(line,"F75 %ld\n",IR & 8191);
	    notifyV2(F75,line);
	  
	    doExternalDeviceFSM(FUNC71);
	  }  
	}
    }
}
#endif

/*
extern int filmHandlerControlWords[5];
#define HANDLER_MANUAL 1
#define HANDLER_WRITE_PERMIT 4
#define HANDLER_SEARCHING 8
*/



void fn76(void)
{
  if(testReady(F76,IR&8191))
  {
    setACT(F76,IR&8191);
    //readBlines();
    ACC = E803_ZERO;
    ACC.bytes[0] = (uint8_t) Blines & 0xFF;
    ACC.bytes[1] = (uint8_t) (Blines >> 8) & 0xFF;
    if(B)
    {
      B = FALSE;
    }
  }
  else
  {
    B = TRUE;
  }
}




/* New version that uses the externalDeviceFSM.
   Since this works OK it opens up all sorts of possibilities.  
   For ALL peripheral READY/ACT can be done this way ! 
   F76 can poll the film controller at full speed ! 
*/
#if 0
void fn76old(void)
{
    char line[100];
    /* I would normally avoid sending messages from within the emulate loop.... but it 
       seems to work OK !... */

//    printf("F76 %d\n",F76Ready);

    if(F76Ready)
    {  /* We got a reply.  Note this is the Ready & Act rolled into one ! */
	ACC = E803_ZERO;
	ACC.bytes[0] = ACC_INPUTS & 255;
	ACC.bytes[1] = (ACC_INPUTS >> 8) & 255;

	F76Ready = FALSE;
	B = FALSE;
    }
    else
    {
	if(!B)
	{
	    /* Go busy and send the F76 message */
	    B = TRUE;
	    
	    sprintf(line,"F76 %ld\n",IR & 8191);
	    notifyV2(F76,line);

	    doExternalDeviceFSM(FUNC71);  
	}
    }
}
#endif


uint8_t storeBuffer[64*5];

uint8_t *TransferAndFinish(gboolean Transfer,gboolean Finish,int count,uint8_t *data)
{
  uint8_t *dataOut;
  uint8_t *pointer;
  int wordNo,byteNo,F77Address;

  dataOut = NULL;
  if(Transfer && Finish && (count == 0 ) && (data == NULL))
  {
    // Search 
    F77Finish = TRUE;

  }


  if(Transfer &&  (count != 0) && (data != NULL))
  {
    // Read  (data from Film Controler)

    pointer = data;

    	for(wordNo = 0; wordNo < 8; wordNo += 1)
	{
	    for(byteNo = 0; byteNo < 5; byteNo += 1)
	    {
		STORE_CHAIN.bytes[byteNo] = (uint8_t) (*pointer++ & 0xFF);
	    }

	    F77Address = IR & 8191;
	    /* You can't overwrite the initial instructions !*/
	    if(F77Address > 3)
	      {
		  //hash(F77Address,STORE_CHAIN);
#if 0
		guchar b1,b2,b3,*ptr,*ptr1;
		b1 =  STORE_CHAIN.bytes[0];
		b2 =  STORE_CHAIN.bytes[1];
		b3 =  STORE_CHAIN.bytes[2];

		ptr = ptr1 = &rgbbuf[((F77Address % 64) * 6) + ((F77Address / 64) * 6 * IMAGE_WIDTH)];

		*ptr++ = b1; *ptr++ = b2; *ptr++ = b3;
		*ptr++ = b1; *ptr++ = b2; *ptr++ = b3;

		ptr1 += IMAGE_WIDTH * 3;

		*ptr1++ = b1; *ptr1++ = b2; *ptr1++ = b3;
		*ptr1++ = b1; *ptr1++ = b2; *ptr1++ = b3;
#endif

		CoreStore[F77Address] = STORE_CHAIN;
	    }
	    IR += 1;
	}


	if(Finish)
	{
	  F77Finish = TRUE;
	  IR -= 1;
	}
  }

  if(Transfer  && (count != 0) && (data == NULL))
  {
    pointer = storeBuffer;
    // Write  (data to Film Controler)
    for(wordNo = 0; wordNo < 8; wordNo += 1)
    {


      F77Address = IR & 8191;
      /* The initial instructions read as zeros !*/
      if(F77Address > 3)
      {	    
	STORE_CHAIN = CoreStore[F77Address];
      }
      else
      {
	STORE_CHAIN = E803_ZERO;
      }
      for(byteNo = 0; byteNo < 5; byteNo += 1)
      {
	*pointer++ = STORE_CHAIN.bytes[byteNo] & 0xFF;
      }
	
      IR += 1;
    }
    if(Finish)
    {
      F77Finish = TRUE;
      IR -= 1;
    }
    dataOut = storeBuffer;
	
  }


  return(dataOut);
}

void fn77(void)
{
    int FW;

    ST = TRUE;
    FW = ST && !L;

    if(FW) 
    {
	L = TRUE;
	ADDRESS_COUNT = 0;
    }

    if(ST && FW) 
    {   /* First Word Time */
	LP = TRUE;

#if FILM	  
	FChandleF77(IR&8191);
#endif  
	//	sprintf(line,"F77 %ld\n",IR & 8191);
	//notifyV2(F77,line);

	//doExternalDeviceFSM(FUNC71);  
    }

    if(LP && F77Finish)
    {
      //if(ADDRESS_COUNT != 0) IR -= 1;
	LP = FALSE;
	F77Finish = FALSE;
    }

    if(!LP && !FW) L = FALSE;

    if(ST) FW = FALSE;
}

/**
 * Load a word into store.
 * @param text A sting in the normal 803 T2 format for a word.
 * @param address The location to write the word. 
 * This is used to load T1.
 */

int texttoword(const char *text,int address)
{
    const char *cp;
    int n1,n2;
    char f1h,f1l,f2h,f2l,b;
    uint8_t  *ucp;
    E803word data;

    f1h = f1l = f2h = f2l = '0';
    n1 = n2 = 0;

    cp = text;
    if((*cp <= '7') && (*cp >= '0'))
    {
	f1h = *cp++;
    }
    else
    {
	return(0);
    }

    if((*cp <= '7') && (*cp >= '0'))
    {
	f1l = *cp++;
    }
    else
    {
	return(0);
    }

    n1 = 0;
    while(isdigit(*cp))
    {
	n1 *= 10;
	n1 += *cp++ - '0';
    }

    if(n1 > 8191)
    {
	return(0);
    }

    if((*cp == '.') || (*cp == ':') || (*cp == '/'))
    {
	b = *cp++;
    }
    else
    {
	b = ' ';
	if(*cp == '\0') goto OKK;
	return(0);
    }

    if((*cp <= '7') && (*cp >= '0'))
    {
	f2h = *cp++;
    }
    else
    {
	return(0);
    }

    if((*cp <= '7') && (*cp >= '0'))
    {
	f2l = *cp++;
    }
    else
    {
	return(0);
    }

    n2 = 0;
    while(isdigit(*cp))
    {
	n2 *= 10;
	n2 += *cp++ - '0';
    }

    if(n2 > 8191)
    {
	return(0);
    }

 OKK:
    ucp = &data.bytes[0];

    *ucp++  = (uint8_t) n2 & 0xFF;

    *ucp++  = (uint8_t) (((f2l << 5) & 0xE0) | ((n2 >> 8) & 0x1F));

    *ucp++  = (uint8_t) (((n1 << 4) & 0xF0) | ((b == '/')? 8 : 0) | ((f2h) & 0x07));

    *ucp++  = (uint8_t) ((n1 >> 4) & 0xFF);

    *ucp    = (uint8_t) (((f1h - '0') << 4) | ((f1l - '0') << 1) | ((n1 >> 12) & 0x1) | ((*ucp << 1) & 0x80));

    WriteStore(address,&data);
	
    return(1);
}


/* The following functions are all "debugging tools" which were used during
the development of the emulator.  I've left them here as you never know when 
a bug may come to light that needs these tools to help track it down.
*/ 

#if 1

/*
#define F1(x) ((x[4]>>1)&0x3F)
#define N1(x) (int)(((((long  *)&x[2])[0])>>4)&0x1FFF)
#define B(x)  ((x[2]>>3)&0x1)
#define F2(x) (((((int  *)&x[1])[0])>>5)&0x3F)
#define N2(x) (((((int  *)x)[0])&0x1FFF))
*/
/*
#define _F1(x) (int)(((x->bytes[4])>>1)&0x3F)
#define _N1(x) (int)(((((long  *)&x->bytes[2])[0])>>4)&0x1FFF)
#define _B(x)  ((x->bytes[2]>>3)&0x1)
#define _F2(x) (((((int  *)&x->bytes[1])[0])>>5)&0x3F)
#define _N2(x) (((((int  *)&x->bytes[0])[0])&0x1FFF))
*/

#define _F1(x) ((x->word>>33)&0x3F)
#define _N1(x) ((x->word>>20)&0x1FFF)
#define _B(x)  ((x->word>>19)&0x1)
#define _F2(x) ((x->word>>13)&0x3F)
#define _N2(x) ((x->word&0x1FFF))


/*
static char *ADDRprint(int address,char *text)
{
    char temp[20],*cpa,*cpb;
    int lsp;

    if((address < 0) || (address > 8191))
    {
	strcpy(text,"****");
	return(text+4);
    }
	
    sprintf(temp,"%d",address);
    lsp =  4 - strlen(temp);
    cpa = text;
    cpb = temp;
    while(lsp--) *cpa++ = ' ';
    while((*cpa++ = *cpb++) != 0);

    return(--cpa);
}
*/
/*
void WORD_decode(int address,char *text)
{
    int f1,n1,b,f2,n2;
    char *cp;
    char *textcp;


    textcp = text;
    cp = &CoreStore[address].bytes[0];

    f1=F1(cp);
    n1=N1(cp);
    b=B(cp);
    f2=F2(cp);
    n2=N2(cp);

    *textcp++ = '0' + (f1>>3);
    *textcp++ = '0' + (f1 &7);
    *textcp++ = ' ';

    textcp = ADDRprint(n1,textcp);

    *textcp++ = (b == 0)?':':'/';

    *textcp++ = '0' + (f2>>3);
    *textcp++ = '0' + (f2 &7);
    *textcp++ = ' ';

    textcp = ADDRprint(n2,textcp);

}
*/
/*
void REG_decode(char *reg,char *text)
{
    int f1,n1,b,f2,n2;
    char *cp;
    char *textcp;


    textcp = text;
    cp = reg;

    f1=F1(cp);
    n1=N1(cp);
    b=B(cp);
    f2=F2(cp);
    n2=N2(cp);

    *textcp++ = '0' + (f1>>3);
    *textcp++ = '0' + (f1 &7);
    *textcp++ = ' ';

    textcp = ADDRprint(n1,textcp);

    *textcp++ = (b == 0)?':':'/';

    *textcp++ = '0' + (f2>>3);
    *textcp++ = '0' + (f2 &7);
    *textcp++ = ' ';

    textcp = ADDRprint(n2,textcp);

}
*/
/*
void dumpcore(void)
{
    int address;
    char string[100];
    long *lp;
    FILE *dump;

    dump = fopen("core","w");

    for(address = 0; address < 8192; address++)
    {
	WORD_decode(address,string);
	lp = (long *)  &CoreStore[address].bytes[0];
	fprintf(dump,"%d %s\n",address,string);
    }

    fclose(dump);
}
*/
/*
void dumpWord(E803word *wp)
{
    char *cp;
    int bit;
    unsigned char byte;
    int n;


    cp = &wp->bytes[4];

    n = 5;
    while(n--)
    {
	bit = 8;
	byte = *cp;

  
    
	while(bit--)
	{
	    printf("%d",(byte & 0x80) ? 1 : 0 );
	    byte <<= 1;
	}
	printf(" ");
	cp--;
    }

    printf("\n");
}
*/

static GString *dumpWord(GString *text,E803word *wp)
{
    int f1,n1,b,f2,n2;
    uint8_t byte;
    E803word signExtended;

    for(int n = 7; n >= 0; n -= 1)
    {
	byte = wp->bytes[n];
	for(int bit = 0; bit < 8; bit++)
	{
	    g_string_append_c(text,(byte & 0x80) ? '1' : '0' );
	    byte = (uint8_t) (byte << 1);
	}
	g_string_append_c(text,' ');
    }

    f1=_F1(wp);
    n1=_N1(wp);
    b=_B(wp);
    f2=_F2(wp);
    n2=_N2(wp);
    
    g_string_append_printf(text,"%02o %4d %c %02o %4d ",
			   f1,n1,(b == 0)? ':' : '/',f2,n2);
    
    signExtended = *wp;
    
    if(signExtended.bytes[4] & 0x40)
    {
	signExtended.bytes[4] |= 0x80;
	signExtended.bytes[5] = 0xFF;
	signExtended.bytes[6] = 0xFF;
	signExtended.bytes[7] = 0xFF;
    }
    g_string_append_printf(text,"%" PRId64,*(long *)&signExtended);
    return(text);
}



/*
void dumpWordFile(FILE *fp,E803word *wp)
{
    char *cp;
    int bit;
    unsigned char byte;
    int n;
    E803word w;
    long *lp;
    double total,fbit;
    char text[32];

    cp = &wp->bytes[7];

    n = 8;
    while(n--)
    {
	bit = 8;
	byte = *cp;

  
    
	while(bit--)
	{
	    fprintf(fp,"%d",(byte & 0x80) ? 1 : 0 );
	    byte <<= 1;
	}
	fprintf(fp," ");
	cp--;
    }

    REG_decode(&wp->bytes[0],text);

    fprintf(fp," %s ",text);
    w = *wp;

    if(w.bytes[4] & 0x40)
    {
      w.bytes[5] = 0xFF;
      w.bytes[6] = 0xFF;
      w.bytes[7] = 0xFF;
    }

    lp = (long *) &w.bytes[0];

    fprintf(fp," %ld ",*lp);




#if 0
   if(w.bytes[4] & 0x40)
   {
       *lp = - (long) *lp;
      fprintf(fp,"-");
   }



    total = 0.0;
    fbit = 0.5;

    for(n=0;n<39;n++)
    {
      if(w.bytes[4] & 0x20)
      {
	total += fbit;
      }
      fbit *= 0.5;
      
      *lp *= 2;
    }

    fprintf(fp,"%f",total);
#endif
}
*/

/*
void dumpMant(E803word *wp)
{
    char *cp;
    int bit;
    unsigned char byte;
    int n;


    cp = &wp->bytes[4];

    n = 5;
    while(n--)
    {
	bit = 8;
	byte = *cp;

  
    
	while(bit--)
	{
	    printf("%d",(byte & 0x80) ? 1 : 0 );
	    byte <<= 1;
	}
	printf(" ");
	cp--;
    }

    printf("\n");
}
*/
/*
void dumpACCAR(void)
{
    char *cp;
    int bit;
    unsigned char byte;
    int n;


    cp = &ACC.bytes[4];

    n = 5;
    while(n--)
    {
	bit = 8;
	byte = *cp;

  
    
	while(bit--)
	{
	    printf("%d",(byte & 0x80) ? 1 : 0 );
	    byte <<= 1;
	}
	
	cp--;
    }

    printf(" ");
    cp = &AR.bytes[7];

    n = 5;
    while(n--)
    {
	bit = 8;
	byte = *cp;

  
    
	while(bit--)
	{
	    printf("%d",(byte & 0x80) ? 1 : 0 );
	    byte <<= 1;
	}
	
	cp--;
    }


    printf("    ");
    cp = &QACC.bytes[4];

    n = 5;
    while(n--)
    {
	bit = 8;
	byte = *cp;

  
    
	while(bit--)
	{
	    printf("%d",(byte & 0x80) ? 1 : 0 );
	    byte <<= 1;
	}
	
	cp--;
    }

    printf(" ");
    cp = &QAR.bytes[7];

    n = 5;
    while(n--)
    {
	bit = 8;
	byte = *cp;

  
    
	while(bit--)
	{
	    printf("%d",(byte & 0x80) ? 1 : 0 );
	    byte <<= 1;
	}
	
	cp--;
    }
    
}
*/
#endif

#endif   // GLobal ignore
