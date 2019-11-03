#pragma once


void ChargerInit(__attribute__((unused)) GtkBuilder *builder,
		   GString *sharedPath,
		   GString *userPath);

void ChargerTidy(GString *userPath);
//void updateCharger(gboolean batteryOn,gboolean computerOn,gboolean PtsOn);
