#pragma once
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


GObject *gtk_builder_get_object_checked(GtkBuilder *builder,const gchar *name);
GdkPixbuf *my_gdk_pixbuf_new_from_file (const char *filename);


typedef struct token
{
    const char *string;                 /* The string to match */
    int value;                          /* The value to be returned by #tokenToValue */ 
    gboolean (*handler)(int nextField); /* The function to be called when the token found */
} Token;


int preParse(char *line);
int parse(Token *Tokens, int index);
int parseFile(gchar *filename,Token *Tokens,const char *comment);
char *getField(int index);
int getFieldCount(void);
void updateConfigFile(const char *fileName,GString *userPath,GString *configString);
void writeConfigFile(const char *fileName,GString *userPath,GString *configString);
gboolean readConfigFile(const char *fileName,GString *userPath,Token *savedStateTokens);


typedef struct _switch
{
    gboolean state;
    GdkPixbuf *onPixbuf;
    GdkPixbuf *offPixbuf;
    GtkWidget *drawingArea;     // Obsolete ?
    int width,height;
} Switch;



struct sndEffect 
{
    int effectType;
    gint16 *frames;
    int frameCount;
};
struct sndEffect *readWavData(const char *wavFileName);


/*
#define REDRAW(w) if(!w##Pending){w##Pending=TRUE;gtk_widget_queue_draw(w);}
#define DRAWN(w) {w##Pending=FALSE;}
*/
