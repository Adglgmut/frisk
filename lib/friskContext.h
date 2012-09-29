#ifndef FRISK_H
#define FRISK_H

// Shamelessly mimicking Microsoft's RGB()
#define FRISKCOLOR(r, g, b) ((unsigned int)(((unsigned char)(r)|((unsigned int)((unsigned char)(g)) << 8))|(((unsigned int)(unsigned char)(b)) << 16)))

typedef enum friskSearchFlag
{
    FSF_RECURSIVE               = (1 << 0),
    FSF_FILESPEC_REGEXES        = (1 << 1),
    FSF_FILESPEC_CASE_SENSITIVE = (1 << 2),
    FSF_MATCH_REGEXES           = (1 << 3),
    FSF_MATCH_CASE_SENSITIVE    = (1 << 4),
    FSF_REPLACE                 = (1 << 5),
    FSF_BACKUP                  = (1 << 6),
    FSF_TRIM_FILENAMES          = (1 << 7),

    FSF_COUNT
} friskSearchFlag;

typedef struct friskSavedSearch
{
    char * name;
    char * match;
    char * path;
    char * filespec;
    char * fileSize;
    char * replace;
    char * backupExtension;
    int flags;
} friskSavedSearch;

friskSavedSearch * friskSavedSearchCreate();
void friskSavedSearchDestroy(friskSavedSearch * savedSearch);

typedef struct friskConfig
{
    int windowX;
    int windowY;
    int windowW;
    int windowH;
    int windowMaximized;
    int flags;
    int textSize;
    int textColor;
    int backgroundColor;
    int highlightColor;

    char *cmdTemplate;

    char ** matches;
    char ** paths;
    char ** filespecs;
    char ** replaces;
    char ** backupExtensions;
    char ** fileSizes;

    friskSavedSearch ** savedSearches;
} friskConfig;

friskConfig * friskConfigCreate();
void friskConfigDestroy(friskConfig * config);
void friskConfigDefaults(friskConfig * config);
int friskConfigLoad(friskConfig * config, const char * filename);
int friskConfigSave(friskConfig * config, const char * filename);

#endif
