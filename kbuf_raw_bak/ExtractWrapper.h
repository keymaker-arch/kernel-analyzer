#include <llvm/IR/Module.h>
#include <llvm/IR/User.h>

#include "KAMain.h"
#include "BufSizer.h"

using namespace llvm;


class ExtractWrapperPass{
  private:
  BufSizeAnalyzor BufSizer;
  GlobalContext* Ctx;
  unsigned isPassedIn(Function& F, User::op_iterator arg);
  std::string getFormatStr(Value* arg);
  void AddWrapperEntry(std::string FileName, std::string FName, std::string FmtStr, unsigned idx, std::string wrappeeTrace);
  struct WrapperFunc* isWrapper(std::string name);
  void checkBugLocal(Value* buf_v, Value* fmtstr_v);
  Value* getArgAt(CallInst* callinst, unsigned idx);

  public:
  ExtractWrapperPass(GlobalContext* ctx);
  ~ExtractWrapperPass(){};
  void run(Module M);
};