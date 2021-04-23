/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#ifndef _SKELETON_MODULE_LOADER_H_
#define _SKELETON_MODULE_LOADER_H_

// From SC
void AddMyPlayerScripts();

// Add all
void AddMyModuleScripts()
{
    AddMyPlayerScripts();
}

#endif // _SKELETON_MODULE_LOADER_H_
