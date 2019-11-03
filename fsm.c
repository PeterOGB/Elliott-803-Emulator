#define G_LOG_USE_STRUCTURED
#include <stdio.h>
#include <glib.h>
#include "fsm.h"

/*********************** FSM support *********************/

void doFSM(struct fsm *fsm,int event,void *data)
{

    int (*handler)(int,int,void *);
    int state;
    struct fsmtable *fsmEntry;
 
nextEvent:
    fsmEntry = fsm->table;
    state = fsm->state;

    if(fsm->debugFlag)
    {
	g_debug("\tFSM %s ",fsm->name);
	if(fsm->stateNames != NULL) 
	{
	    g_debug("state %s ",(fsm->stateNames)[state]);
	}
	else
	{
	    g_debug("state %d ",state);
	}

	if(fsm->eventNames != NULL)
	{
	    if(event < 0)
		g_debug("event = FSMNoEvent ");
	    else
		g_debug("event %s \n",(fsm->eventNames)[event]);
	}
	else
	{
	    g_debug("event %d \n",event);
	}
    }

    while(fsmEntry->state != -1)
    {
	if( (fsmEntry->state == state) && (fsmEntry->event == event))
	{
    	    handler = fsmEntry->handler;

	    if(handler == NULL)
	    {
		state = fsmEntry->nextState; 
	    }
	    else
	    {
		state = (handler)(state,event,data);
		if(state == -1)
		    state = fsmEntry->nextState; 

	    }
	    fsm->state = state;
	    if(fsm->nextEvent != -1)
	    {
		event = fsm->nextEvent;
		fsm->nextEvent = -1;
		goto nextEvent;
	    }
	    return;
	}
	fsmEntry++;
    }

    if(fsm->debugFlag)
    {
    
	if(fsm->stateNames == NULL)
	{
	    g_debug("No entry in FSM table %s for state %d ",fsm->name,state);
	}
	else
	{
	    g_debug("No entry in FSM table %s for state %s",fsm->name,(fsm->stateNames)[state]);

	}
   
	if(fsm->eventNames == NULL)
	{
	    g_debug("event = %d",event);
	}
	else
	{
	    if(event < 0)
		g_debug("event = FSMNoEvent ");
	    else
		g_debug("event = %s",(fsm->eventNames)[event]);
	}
    }
}

