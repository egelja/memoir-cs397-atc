#include <iostream>

#include "object_ir.h"

using namespace objectir;

int main() {
  Type *objTy = getObjectType(3, getUInt64Type(), getUInt64Type(), getUInt64Type());

  std::cerr << objTy->toString() << "\n";
    
  Object *myObj = buildObject(objTy);

  std::cerr << myObj->toString() << "\n";

  Field *field1 = getObjectField(myObj, 0);
  Field *field2 = getObjectField(myObj, 1);
  Field *field3 = getObjectField(myObj, 2);

  writeUInt64(field1, 123);
  writeUInt64(field2, 456);
  writeUInt64(field3, 789);

  uint64_t read1 = readUInt64(field1);
  uint64_t read2 = readUInt64(field2);
  uint64_t read3 = readUInt64(field3);

  std::cerr << "1: " << read1 << "\n";
  std::cerr << "2: " << read2 << "\n";
  std::cerr << "3: " << read3 << "\n\n";

  writeUInt64(field1, read1 + read2);
  writeUInt64(field2, read2 + read3);
  writeUInt64(field3, read3 + read1);
  
  read1 = readUInt64(field1);
  read2 = readUInt64(field2);
  read3 = readUInt64(field3);

  std::cerr << "1: " << read1 << "\n";
  std::cerr << "2: " << read2 << "\n";
  std::cerr << "3: " << read3 << "\n\n";
}
