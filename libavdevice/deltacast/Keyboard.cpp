/**********************************************************************************************************************
 
   Module   : Keyboard
   File     : Keyboard.cpp
   Created  : 2007/06/25
   Author   : cs

 **********************************************************************************************************************/

/***** INCLUDES *******************************************************************************************************/

#include "Keyboard.h"
#include <termios.h>
#include <unistd.h>

/***** EXTERNAL VARIABLES *********************************************************************************************/

/***** GLOBAL VARIABLES ***********************************************************************************************/

static struct termios initial_settings, new_settings, current_settings;
static int peek_character = -1;


void init_keyboard()
{
   tcgetattr(0,&initial_settings);
   new_settings = initial_settings;
   new_settings.c_lflag &= ~ICANON;
   new_settings.c_lflag &= ~ECHO;
   new_settings.c_lflag &= ~ISIG;
   new_settings.c_cc[VMIN] = 1;
   new_settings.c_cc[VTIME] = 0;
   tcsetattr(0, TCSANOW, &new_settings);
}

void close_keyboard()
{
   tcsetattr(0, TCSANOW, &initial_settings);
}

int kbhit()
{
   char ch;
   int nread;

   if(peek_character != -1)
      return 1;
   new_settings.c_cc[VMIN]=0;
   tcsetattr(0, TCSANOW, &new_settings);
   nread = read(0,&ch,1);
   new_settings.c_cc[VMIN]=1;
   tcsetattr(0, TCSANOW, &new_settings);

   if(nread == 1) {
      peek_character = ch;
      return 1;
   }
   return 0;
}

int getch()
{
   char ch;

   if(peek_character != -1) {
      ch = peek_character;
      peek_character = -1;
      return ch;
   }
   if(read(0,&ch,1))
      return ch;
   else
      return 0;
}

void userQueryTxtDisplayON_keyboard()
{
   tcgetattr(0,&current_settings);
   new_settings = current_settings;
   new_settings.c_lflag |= ECHO;
   tcsetattr(0, TCSANOW, &new_settings);
}

void userQueryTxtDisplayOFF_keyboard()
{
   tcgetattr(0,&current_settings);
   new_settings = current_settings;
   new_settings.c_lflag &= ~ECHO;
   tcsetattr(0, TCSANOW, &new_settings);
}
