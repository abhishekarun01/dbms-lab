#include <iostream>
#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"

int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  Disk disk_run;
  
  unsigned char buffer[BLOCK_SIZE]; //BLOCK_SIZE constant with value = 2048
  Disk::readBlock(buffer, 6000);

  char message[] = "hello";
  memcpy(buffer + 20, message, 6);
  Disk::writeBlock(buffer, 6000);

  unsigned char buffer2[BLOCK_SIZE];
  char message2[6];
  Disk::readBlock(buffer2, 6000);
  memcpy(message2, buffer2 + 20, 6);
  std::cout << message2 << '\n';

  char message3[20];

  unsigned char buffer3[BLOCK_SIZE];
  Disk::readBlock(buffer3, 0);
  memcpy(message3, buffer3, 20);
  for(int i = 0; i < 20; i++)
  {
    std::cout << (int)(message3[i]);
    if(i < 19)
    {
      std::cout << ", ";
    }
  }
  std::cout << '\n';

  // StaticBuffer buffer;
  // OpenRelTable cache;

  // return FrontendInterface::handleFrontend(argc, argv);

}