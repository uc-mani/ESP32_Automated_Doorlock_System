/**
 * The purpose of this file is to maintain lock and unlock list and stores it in memory 
 */

#include <Preferences.h>

#define MAX_UNITS 200 // maximum no of units supported

// Storing building and units number in memory
const char *untBlkNoArr[] = {"1011", "1021", "1031", "1012", "1013"}; //eventually it will be array of 200 (populate it manually)
size_t arrSize = sizeof(untBlkNoArr) / sizeof(untBlkNoArr[0]);
int16_t unlockArr[MAX_UNITS] = {};// {"1011", "1021"};
int16_t lockArr[MAX_UNITS] = {};//{1031, 1012, 1013};// {"1012", "1013"};
size_t unlockArrSize = 0; //sizeof(unlockArr) / sizeof(int16_t);
size_t lockArrSize = 0; //sizeof(unlockArr) / sizeof(int16_t);

Preferences doorList; //Preference namespace init

//***************************************************************
// Saving unlocked and locked unit No and Status in flash storage
//***************************************************************
/**
 * Save the status list of locked or unlocked doors
 */
void SaveDoorList(char *doorStatus)
{
  if(!strcmp(doorStatus, "unlocked")) //saving status list of unlocked doors
  {
    doorList.begin("unlockedDoor", RW_MODE);
    //doorList.clear(); //Delete all keys and values from the currently opened namespace. ::::::: ONLY ONCE :::::: disable it afterward

    //Serial.print("The number of elements in unlockArr is: ");
    //Serial.println(unlockArrSize);

    doorList.putBytes("unlockedDoor", unlockArr, unlockArrSize*2);
    //Serial.print("The size of \"unlockedDoor\" is (in bytes): ");
    //Serial.println(doorList.getBytesLength("unlockedDoor") );
    Serial.println("");

    doorList.end();
  }
  else if(!strcmp(doorStatus, "locked")) //saving status list of locked doors
  {
    doorList.begin("lockedDoor", RW_MODE);
    //doorList.clear(); //Delete all keys and values from the currently opened namespace. ::::::: ONLY ONCE :::::: disable it afterward

    //Serial.print("The number of elements in lockArr is: ");
    //Serial.println(lockArrSize);

    doorList.putBytes("lockedDoor", lockArr, lockArrSize*2);
    //Serial.print("The size of \"lockedDoor\" is (in bytes): ");
    //Serial.println(doorList.getBytesLength("lockedDoor") );
    Serial.println("");

    doorList.end();
  }
}

/**
 * Get the status list of locked or unlocked doors
 */
void GetDoorList(char *doorStatus)
{
  String unlock_message = "";
  String unlock_units = "";
  String lock_message = "";
  String lock_units = "";
  if(!strcmp(doorStatus, "unlocked")) //get status list of unlocked doors
  {
    doorList.begin("unlockedDoor", RO_MODE);
    uint8_t arrByteSize = doorList.getBytesLength("unlockedDoor");
    unlockArrSize = arrByteSize/2;
    //Serial.print("Size of unlock array: ");
    //Serial.println(unlockArrSize);
    doorList.getBytes("unlockedDoor", unlockArr, arrByteSize);
    unlock_message = "All Unlocked Unit and Building No's: ";
    //Serial.print("All Unlocked Unit and Building No's: ");
    if(unlockArrSize)
    {
      for (int i = 0; i< unlockArrSize; i++)
      {
        unlock_units = unlock_units + unlockArr[i];
        unlock_units = unlock_units + ", ";
        //Serial.print(unlockArr[i]);
        //Serial.print(", ");
      }
      unlock_message = unlock_message + unlock_units;
      Serial.println("************************************");
      Serial.print(unlock_message);
      SerialBT.println(unlock_message);
      Serial.println("");
    }
    else
    {
      Serial.println("Empty List");
    }
    doorList.end();
    Serial.println("************************************");
  }
  else if(!strcmp(doorStatus, "locked")) //get status list of locked doors
  {
    doorList.begin("lockedDoor", RO_MODE);
    uint8_t arrByteSize = doorList.getBytesLength("lockedDoor");
    lockArrSize = arrByteSize/2;
    //Serial.print("Size of lock array: ");
    //Serial.println(lockArrSize);
    doorList.getBytes("lockedDoor", lockArr, arrByteSize);
    //Serial.print("All Locked Unit and Building No's: ");
    lock_message = "All Locked Unit and Building No's: ";
    if(lockArrSize)
    {
      for (int i = 0; i< lockArrSize; i++)
      {
        lock_units = lock_units + lockArr[i];
        lock_units = lock_units + ", ";
        //Serial.print(lockArr[i]);
        //Serial.print(", ");
      }
      lock_message = lock_message + lock_units;
      Serial.println("**********************************");
      Serial.print(lock_message);
      SerialBT.println(lock_message);
      Serial.println("");
    }
    else
    {
      Serial.println("Empty List");
    }
    doorList.end();
    Serial.println("**********************************");
  }
}

/*
* Delete entry from locked and unlocked array
*/
void deleteEntry(bool islocked, int16_t unitBuildNo)
{
  int i=0;
  if(islocked) //delete entry from locked array
  {
    //check if unitBuildNo exist in lockArr then delete entry and readjust the array
    if(lockArrSize == 1)
    {
      lockArr[0] = '\0';
      Serial.println("Entry deleted from locked array");
      SaveDoorList("locked");
    }
    else if(lockArrSize > 1)
    {
      for(i=0; i < lockArrSize; i++)
      {
        if(lockArr[i] == unitBuildNo)
        {
          //swap the current index value with last index value
          lockArr[i] = lockArr[lockArrSize-1];
          lockArr[lockArrSize--] = '\0';
          Serial.println("Entry deleted from locked array");
          SaveDoorList("locked");
          break;
        }
      }
    }
  }
  else //delete entry from unlocked array
  {
    //check if unitBuildNo exist in unlockArr then delete entry and readjust the array
    if(unlockArrSize == 1)
    {
      unlockArr[0] = '\0';
      Serial.println("Entry deleted from unlocked array");
      SaveDoorList("unlocked");
    }
    else if(unlockArrSize > 1)
    {
      for(i=0; i < unlockArrSize; i++)
      {
        if(unlockArr[i] == unitBuildNo)
        {
          //swap the current index value with last index value
          unlockArr[i] = unlockArr[unlockArrSize-1];
          unlockArr[unlockArrSize--] = '\0';
          Serial.println("Entry deleted from unlocked array");
          SaveDoorList("unlocked");
          break;
        }
      }
    }
  }
}

/**
 * Populate and process unlockArr from the LoRa receiver message.
 * >if Received unit&Building no already exist do nothing,
 * >otherwise add it in the unlockArr at the end of last entry(at unlockArrSize)
 * >Also delete it from the locked array (lockArr) => may be sort the index::TBC or swap the last entry at lockArrSize with the index of deleted entry and decrement lockArrSize
**/
void processUnlockArr(int16_t unitBuildNo)
{
  bool present = false;
  int i=0;

  //GetDoorList("unlocked");

  //Serial.print("unlockArrSize = "); Serial.println(unlockArrSize);

  //Serial.print("unlockArr : ");
  //check if unitBuildNo exist in unlockArr
  for(; i < unlockArrSize; i++)
  {
    //Serial.print(unlockArr[i]); Serial.print(", ");
    if(unlockArr[i] == unitBuildNo)
    {
      present = true;
      break;
    }
  }
  Serial.println(" ");

  if(present == false)
  {
    unlockArr[unlockArrSize++] = unitBuildNo;
    Serial.print("List Updated with New Unit and Building No: ");
    Serial.println(unlockArr[unlockArrSize-1]);
  }
  else
  {
    Serial.print("List already have Unit and Building No: ");
    Serial.println(unlockArr[i]);
  }

  //delete the same number from locked list
  deleteEntry(true, unitBuildNo); //true means delete entry from locked array

  // Serial.print("Unlocked Array Now: ");
  // for(i=0; i < unlockArrSize; i++)
  // {
  //   Serial.print(unlockArr[i]); Serial.print(", ");
  // }
  // Serial.println(" ");

  SaveDoorList("unlocked"); // stores the unlock array in memory

  GetDoorList("unlocked");
}

/**
 * Populate and process lockArr from the LoRa receiver message.
 * >if Received unit&Building no already exist do nothing,
 * >otherwise add it in the lockArr at the end of last entry(at lockArrSize)
 * >Also delete it from the locked array (unlockArr) => may be sort the index::TBC or swap the last entry at unlockArrSize with the index of deleted entry and decrement unlockArrSize
**/
void processLockArr(int16_t unitBuildNo)
{
  bool present = false;
  int i=0;

  //GetDoorList("locked");

  //Serial.print("lockArrSize = "); Serial.println(lockArrSize);

  //Serial.print("lockArr : ");
  //check if unitBuildNo exist in unlockArr
  for(i=0; i < lockArrSize; i++)
  {
    //Serial.print(lockArr[i]); Serial.print(", ");
    if(lockArr[i] == unitBuildNo)
    {
      present = true;
      break;
    }
  }
  Serial.println(" ");

  if(present == false)
  {
    lockArr[lockArrSize++] = unitBuildNo;
    Serial.print("Lock Array list Updated with New Unit and Building No: ");
    Serial.println(lockArr[lockArrSize-1]);
  }
  else
  {
    Serial.print("List already have Unit and Building No: ");
    Serial.println(lockArr[i]);
  }

  //delete the same number from unlocked list
  deleteEntry(false, unitBuildNo); //false means delete entry from unlocked array

  // Serial.print("Locked Array Now: ");
  // for(i=0; i < lockArrSize; i++)
  // {
  //   Serial.print(lockArr[i]); Serial.print(", ");
  // }
  // Serial.println(" ");

  SaveDoorList("locked"); // stores the lock array in memory

  GetDoorList("locked");
}

//*******************************************************************
// END Saving unlocked and locked unit No and Status in flash storage
//*******************************************************************