#include "SearchConfig.h"

#include <windows.h>
#include <cJSON.h>

// ------------------------------------------------------------------------------------------------
// Helper functions

static std::string calcConfigFilename()
{
    char buffer[MAX_PATH];
    std::string filename;
    if(GetModuleFileName(GetModuleHandle(NULL), buffer, MAX_PATH))
    {
        char *lastBackslash = strrchr(buffer, '\\');
        if(lastBackslash)
        {
            *lastBackslash = 0;
            filename = buffer;
            filename += "\\config.json";
        }
    }
    return filename;
}

bool readEntireFile(const std::string &filename, std::string &contents)
{
    FILE *f = fopen(filename.c_str(), "rb");
    if(!f)
        return false;

    fseek(f, 0, SEEK_END);
    off_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(size == 0)
    {
        fclose(f);
        return false;
    }

    std::vector<char> temp;
    temp.resize(size);
    size_t bytesRead = fread(&temp[0], sizeof(char), size, f);
    if(bytesRead != size)
    {
        fclose(f);
        return false;
    }

    fclose(f);
    contents.assign(temp.begin(), temp.end());
    return true;
}

static bool writeEntireFile(const std::string &filename, const std::string &contents)
{
    FILE *f = fopen(filename.c_str(), "wb");
    if(!f)
        return false;

    fwrite(contents.c_str(), sizeof(char), contents.length(), f);
    fclose(f);
    return true;
}

static bool jsonGetString(cJSON *json, const char *k, std::string &v)
{
    if(json->type != cJSON_Object)
        return false;

    cJSON *child = cJSON_GetObjectItem(json, k);
    if(!child || (child->type != cJSON_String))
        return false;
    
    v = child->valuestring;
    return true;
}

static bool jsonGetInt(cJSON *json, const char *k, int &v)
{
    if(json->type != cJSON_Object)
        return false;

    cJSON *child = cJSON_GetObjectItem(json, k);
    if(!child || (child->type != cJSON_Number))
        return false;
    
    v = child->valueint;
    return true;
}

static bool jsonSetInt(cJSON *json, const char *k, int v)
{
    if(json->type != cJSON_Object)
        return false;

    cJSON_AddNumberToObject(json, k, v);
    return true;
}

static bool jsonSetString(cJSON *json, const char *k, const std::string &v)
{
    if(json->type != cJSON_Object)
        return false;

    cJSON_AddStringToObject(json, k, v.c_str());
    return true;
}

static bool jsonGetStringList(cJSON *json, const char *k, StringList &v)
{
    if(json->type != cJSON_Object)
        return false;

    cJSON *child = cJSON_GetObjectItem(json, k);
    if(!child || (child->type != cJSON_Array))
        return false;

    child = child->child;

    v.clear();
    for( ; child != NULL; child = child->next)
    {
        if(child->type != cJSON_String)
            continue;

        v.push_back(std::string(child->valuestring));
    }
    return true;
}

static void jsonSetStringList(cJSON *json, const char *k, const StringList &v)
{
    if(v.empty())
    {
        cJSON_AddItemToObject(json, k, cJSON_CreateArray());
        return;
    }

    int count = v.size();
    const char **rawStrings = (const char **)malloc(sizeof(const char **) * count);
    for(int i = 0; i < count; i++)
    {
        rawStrings[i] = v[i].c_str();
    }
    cJSON_AddItemToObject(json, k, cJSON_CreateStringArray(rawStrings, count));
    free(rawStrings);
}

// ------------------------------------------------------------------------------------------------

SearchConfig::SearchConfig()
{
    // Defaults go here; may be overridden by jsonGet*()
    windowX_ = 0;
    windowY_ = 0;
    windowW_ = 0;
    windowH_ = 0;
    windowMaximized_ = 0;
    flags_ = SF_RECURSIVE;
    textColor_ = RGB(0, 0, 0);
    backgroundColor_ = RGB(224, 224, 224);
    cmdTemplate_ = "notepad.exe \"!FILENAME!\"";
}

SearchConfig::~SearchConfig()
{
}

// ------------------------------------------------------------------------------------------------

void SearchConfig::load()
{
    std::string filename = calcConfigFilename();
    if(filename.empty())
        return;

    std::string contents;
    if(!readEntireFile(filename, contents))
        return;

    cJSON *json = cJSON_Parse(contents.c_str());
    if(!json)
        return;

    jsonGetInt(json, "windowX", windowX_);
    jsonGetInt(json, "windowY", windowY_);
    jsonGetInt(json, "windowW", windowW_);
    jsonGetInt(json, "windowH", windowH_);
    jsonGetInt(json, "windowMaximized", windowMaximized_);
    jsonGetInt(json, "flags", flags_);
    jsonGetInt(json, "textColor", textColor_);
    jsonGetInt(json, "backgroundColor", backgroundColor_);
    jsonGetString(json, "cmdTemplate", cmdTemplate_);
    jsonGetStringList(json, "matches", matches_);
    jsonGetStringList(json, "paths", paths_);
    jsonGetStringList(json, "filespecs", filespecs_);
    cJSON_Delete(json);
}

void SearchConfig::save()
{
    std::string filename = calcConfigFilename();
    if(filename.empty())
        return;

    cJSON *json = cJSON_CreateObject();

    jsonSetInt(json, "windowX", windowX_);
    jsonSetInt(json, "windowY", windowY_);
    jsonSetInt(json, "windowW", windowW_);
    jsonSetInt(json, "windowH", windowH_);
    jsonSetInt(json, "windowMaximized", windowMaximized_);
    jsonSetInt(json, "flags", flags_);
    jsonSetInt(json, "textColor", textColor_);
    jsonSetInt(json, "backgroundColor", backgroundColor_);
    jsonSetString(json, "cmdTemplate", cmdTemplate_);
    jsonSetStringList(json, "matches", matches_);
    jsonSetStringList(json, "paths", paths_);
    jsonSetStringList(json, "filespecs", filespecs_);

    char *jsonText = cJSON_Print(json);
    writeEntireFile(filename, jsonText);

    cJSON_Delete(json);
}
