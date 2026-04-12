#include "AttrCacheTable.h"

#include <cstring>

AttrCacheEntry *AttrCacheTable::attrCache[MAX_OPEN];

int AttrCacheTable::getAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry * attrCatBuffer)
{
    if(relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    if(attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        if(strcmp(entry->attrCatEntry.attrName, attrName) == 0)
        {
            *attrCatBuffer = entry->attrCatEntry;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry* attrCatBuffer)
{
    if(relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    if(attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        if(entry->attrCatEntry.offset == attrOffset)
        {
            *attrCatBuffer = entry->attrCatEntry;
            return SUCCESS;
        }
    }
    
    return E_ATTRNOTEXIST;
}

void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS], AttrCatEntry* attrCatEntry)
{
    strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);
    strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);
    attrCatEntry->attrType = (int)record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
    attrCatEntry->offset = (int)record[ATTRCAT_OFFSET_INDEX].nVal;
    attrCatEntry->primaryFlag = (bool)record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
    attrCatEntry->rootBlock = (int)record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
}

int AttrCacheTable::getSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex)
{
    if(relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    if(AttrCacheTable::attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry *attrCacheEntry = AttrCacheTable::attrCache[relId]; attrCacheEntry != nullptr; attrCacheEntry = attrCacheEntry->next)
    {
        if(strcmp(attrCacheEntry->attrCatEntry.attrName, attrName) == 0)
        {
            *searchIndex = attrCacheEntry->searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::getSearchIndex(int relId, int attrOffset, IndexId *searchIndex)
{
    if(relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    if(AttrCacheTable::attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry *attrCacheEntry = AttrCacheTable::attrCache[relId]; attrCacheEntry != nullptr; attrCacheEntry = attrCacheEntry->next)
    {
        if(attrCacheEntry->attrCatEntry.offset == attrOffset)
        {
            *searchIndex = attrCacheEntry->searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex)
{
        if(relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    if(AttrCacheTable::attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry *attrCacheEntry = AttrCacheTable::attrCache[relId]; attrCacheEntry != nullptr; attrCacheEntry = attrCacheEntry->next)
    {
        if(strcmp(attrCacheEntry->attrCatEntry.attrName, attrName) == 0)
        {
            attrCacheEntry->searchIndex = *searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, int attrOffset, IndexId *searchIndex)
{
    if(relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    if(AttrCacheTable::attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry *attrCacheEntry = AttrCacheTable::attrCache[relId]; attrCacheEntry != nullptr; attrCacheEntry = attrCacheEntry->next)
    {
        if(attrCacheEntry->attrCatEntry.offset == attrOffset)
        {
            attrCacheEntry->searchIndex = *searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::resetSearchIndex(int relId, char attrName[ATTR_SIZE])
{
    IndexId searchIndex = {-1, -1};
    return AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
}

int AttrCacheTable::resetSearchIndex(int relId, int attrOffset)
{
    IndexId searchIndex = {-1, -1};
    return AttrCacheTable::setSearchIndex(relId, attrOffset, &searchIndex);
}

int AttrCacheTable::setAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry *attrCatEntry)
{
    if(relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    if(AttrCacheTable::attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry* entry = AttrCacheTable::attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        if(strcmp(attrName, entry->attrCatEntry.attrName) == 0)
        {
            entry->attrCatEntry = *attrCatEntry;
            entry->dirty = true;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::setAttrCatEntry(int relId, int attrOffset, AttrCatEntry *attrCatEntry)
{
    if(relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    if(AttrCacheTable::attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry* entry = AttrCacheTable::attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        if(entry->attrCatEntry.offset == attrOffset)
        {
            entry->attrCatEntry = *attrCatEntry;
            entry->dirty = true;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

void AttrCacheTable::attrCatEntryToRecord(AttrCatEntry *attrCatEntry, union Attribute record[ATTRCAT_NO_ATTRS])
{
    strcpy(record[ATTRCAT_REL_NAME_INDEX].sVal, attrCatEntry->relName);
    strcpy(record[ATTRCAT_ATTR_NAME_INDEX].sVal, attrCatEntry->attrName);
    record[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrCatEntry->attrType;
    record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = attrCatEntry->primaryFlag;
    record[ATTRCAT_ROOT_BLOCK_INDEX].nVal = attrCatEntry->rootBlock;
    record[ATTRCAT_OFFSET_INDEX].nVal = attrCatEntry->offset;
}
