#pragma once

/* Finite state machine definitions. 
   (C) P.J. Onion 2005
*/


struct fsmtable {
    int state;
    int event;
    int nextState;
    int (*handler)(int state,int event,void *data);
};

struct fsm {
    const char *name;
    int state;
    struct fsmtable *table;
    const char **stateNames;
    const char **eventNames;
    int debugFlag;
    int nextEvent;
};

extern void doFSM(struct fsm *fsm,int event,void *data);

