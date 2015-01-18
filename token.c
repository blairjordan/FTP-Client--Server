#include <string.h>
#include "token.h"

int tokenise (char *inputLine, char *token[]){

      char *tk;
      int i=0;

      tk = strtok(inputLine, DELIMITERS);
      token[i] = tk;

      while (tk != NULL) {

          ++i;
          if (i>=MAX_TOKENS) {
              i = -1;
              break;
          }

          tk = strtok(NULL, DELIMITERS);
          token[i] = tk;
      }
      return i;

}
