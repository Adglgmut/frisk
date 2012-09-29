#include "friskContext.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    friskConfig *context = friskConfigCreate();
    friskConfigDefaults(context);
    {
        int i;
        for(i = 0; i < daSize(&context->filespecs); ++i)
        {
            printf("filespec: %s\n", context->filespecs[i]);
        }
    }
    friskConfigDestroy(context);
    return 0;
}
