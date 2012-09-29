#include "friskContext.h"

#include "dynArray.h"
#include "dynString.h"
#include <stdlib.h>

friskSavedSearch * friskSavedSearchCreate()
{
    friskSavedSearch * savedSearch = (friskSavedSearch *)calloc(1, sizeof(friskSavedSearch));
    return savedSearch;
}

void friskSavedSearchDestroy(friskSavedSearch * savedSearch)
{
    dsDestroy(&savedSearch->name);
    dsDestroy(&savedSearch->match);
    dsDestroy(&savedSearch->path);
    dsDestroy(&savedSearch->filespec);
    dsDestroy(&savedSearch->fileSize);
    dsDestroy(&savedSearch->replace);
    dsDestroy(&savedSearch->backupExtension);

    free(savedSearch);
}

friskConfig * friskConfigCreate()
{
    friskConfig * config = (friskConfig *)calloc(1, sizeof(friskConfig));
    return config;
}

void friskConfigDestroy(friskConfig * config)
{
    daDestroyStrings(&config->matches);
    daDestroyStrings(&config->paths);
    daDestroyStrings(&config->filespecs);
    daDestroyStrings(&config->replaces);
    daDestroyStrings(&config->backupExtensions);
    daDestroyStrings(&config->fileSizes);

    free(config);
}

void friskConfigDefaults(friskConfig * config)
{
    // Defaults go here; may be overridden by jsonGet*()
    config->windowX = 0;
    config->windowY = 0;
    config->windowW = 0;
    config->windowH = 0;
    config->windowMaximized = 0;
    config->flags = FSF_RECURSIVE | FSF_BACKUP;
    config->textColor = FRISKCOLOR(0, 0, 0);
    config->textSize = 8;
    config->backgroundColor = FRISKCOLOR(224, 224, 224);
    config->highlightColor = FRISKCOLOR(255, 0, 0);
    dsCopy(&config->cmdTemplate, "notepad.exe \"!FILENAME!\"");
    daPush(&config->backupExtensions, dsDup("friskbackup"));
    daPush(&config->fileSizes, dsDup("5000"));
    daPush(&config->paths, dsDup("."));
    daPush(&config->filespecs, dsDup("*.txt;*.ini"));
}

int friskConfigLoad(friskConfig * config, const char * filename)
{
    return 0;
}

int friskConfigSave(friskConfig * config, const char * filename)
{
    return 0;
}
