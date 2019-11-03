

enum WiringEvent {MAINS_SUPPLY_ON=1,MAINS_SUPPLY_OFF,CHARGER_CONNECTED,CHARGER_DISCONNECTED,
		  BATTERY_ON_PRESSED,BATTERY_OFF_PRESSED,COMPUTER_ON_PRESSED,COMPUTER_OFF_PRESSED,
		  SUPPLIES_ON,SUPPLIES_OFF,
		  F1WIRES,N1WIRES,F2WIRES,N2WIRES,
		  RONWIRES,MDWIRE,RESETWIRE,CSWIRE,SSWIRE,OPERATEWIRE,
		  TIMER100HZ,LAST_WIRING_EVENT};



typedef void (*Connectors)(unsigned int);


// Set a wire to a value
void wiring(enum WiringEvent event,unsigned int values);

// Register a connection (handler) to a type of wire. 
void connectWires(enum WiringEvent event,Connectors handler);


