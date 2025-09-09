#include <string>
#include <iostream>

#include "readckt.h"
#include "logicsim.h"
#include "podem.h"

#include "readckt.cpp"
#include "logicsim.cpp"
#include "dalg.cpp"
#include "podem.cpp"

int main()
{
   int com;
   std::string cline;
   char wstr[MAXLINE];

   while(!Done) {
      printf("\nCommand>");
      std::getline(std::cin, cline);
      if(sscanf(cline.c_str(), "%s", wstr) != 1) continue;
      char *ptr;
      ptr = wstr;

      while(*ptr){
         *ptr = Upcase(*ptr);
         ptr++;
      }

      cp = cline.substr(strlen(wstr));
      com = READ;
      while(com < NUMFUNCS && strcmp(wstr, command[com].name)) 
      com++;
      
      if(com < NUMFUNCS) {
         if(command[com].state <= Gstate) (*command[com].fptr)();
         else printf("Execution out of sequence!\n");
      }
      else system(cline.c_str());
   }
}