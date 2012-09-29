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
    daDestroy(&config->savedSearches, friskSavedSearchDestroy);

    free(config);
}

void friskConfigDefaults(friskConfig * config)
{
#ifdef NOT_NET
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
#endif
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

// ------------------------------------------------------------------------------------------------

friskHighlight * friskHighlightCreate()
{
    friskHighlight *entry = (friskHighlight *)calloc(1, sizeof(friskHighlight));
    return entry;
}

void friskHighlightDestroy(friskHighlight *highlight)
{
    free(highlight);
}

// ------------------------------------------------------------------------------------------------

friskEntry * friskEntryCreate()
{
    friskEntry *entry = (friskEntry *)calloc(1, sizeof(friskEntry));
    return entry;
}

void friskEntryDestroy(friskEntry *entry)
{
    dsDestroy(&entry->filename);
    dsDestroy(&entry->match);
    daDestroy(&entry->highlights, friskHighlightDestroy);
    free(entry);
}

// ------------------------------------------------------------------------------------------------

friskParams * friskParamsCreate()
{
    friskParams *params = (friskParams *)calloc(1, sizeof(friskParams));
    return params;
}

void friskParamsDestroy(friskParams *params)
{
    daDestroyStrings(&params->paths);
    daDestroyStrings(&params->filespecs);
    dsDestroy(&params->match);
    dsDestroy(&params->replace);
    dsDestroy(&params->backupExtension);
    free(params);
}

// ------------------------------------------------------------------------------------------------

friskContext * friskContextCreate()
{
    friskContext *context = (friskContext *)calloc(1, sizeof(friskContext));
    context->params = friskParamsCreate();
    context->config = friskConfigCreate();
    return context;
}

void friskContextDestroy(friskContext *context)
{
    daDestroy(&context->list, friskEntryDestroy);
    friskParamsDestroy(context->params);
    friskConfigDestroy(context->config);
    free(context);
}
