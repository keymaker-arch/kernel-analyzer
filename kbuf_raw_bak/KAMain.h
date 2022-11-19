#include <llvm/IR/Module.h>


// struct WrappeeEntry{
//   std::string FuncName;
//   // std::string File;
// };
using WrappeeEntry = std::string;

// a wrapper function is a function that has a parameter as the dest buffer of sprintf
// there might be more than one level of wrapper, track it in wrappees
struct WrapperFunc{
  std::string File;  // file containing this wrapper
  std::string Name;
  std::string FmtStr;
  unsigned idx;  // which arg of this wrapper is the buffer
  std::vector<WrappeeEntry> wrappees; // track of function that passes the buffer
};

using WrapperFuncV = std::map<std::string, WrapperFunc*>;

struct GlobalContext{
  WrapperFuncV Wrappers;
};