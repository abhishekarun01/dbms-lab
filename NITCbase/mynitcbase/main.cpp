#include <iostream>
#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"

int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  Disk disk_run;
  StaticBuffer buffer;
  OpenRelTable cache;

  

  for(int i = 0; i < 3; i++)
  {
    RelCatEntry relCatBuffer;
    RelCacheTable::getRelCatEntry(i, &relCatBuffer);
    printf("Relation: %s\n", relCatBuffer.relName);

    for(int j = 0; j < relCatBuffer.numAttrs; j++)
    {
        AttrCatEntry attrCatBuffer;
        AttrCacheTable::getAttrCatEntry(i, j, &attrCatBuffer);
        const char *attrType = attrCatBuffer.attrType == NUMBER ? "NUM" : "STR";
        printf(" %s: %s\n", attrCatBuffer.attrName, attrType);
    }

    printf("\n");
  }

  return 0;

  // return FrontendInterface::handleFrontend(argc, argv);

}