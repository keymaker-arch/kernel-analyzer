
#include "Global.h"
#include "BufSizer.h"


class SprintfPass{
private:
  GlobalContext* Ctx;
  BufSizeAnalyzor BufSizer;

  // we track the wrapper trace of a sprintf call with this struct
  struct WrapperRecord{
    llvm::CallInst* ci;  // callinst of this wrapper
    llvm::Value* bufv;  // the buf value we are tracing in this callinst
    unsigned idx;  // the buf was passed in, the index of this value in parameter of the CALLER func
  };
  typedef struct WrapperRecord WrapperT;
  typedef std::set<WrapperT*> TraceT;


  // this struct describes a sprintf invokation
  struct SprintfCallEntry{
    llvm::Value* bufv;
    unsigned bufsize;
    std::string fmtstr;
    std::set<llvm::Value*> argv;
    TraceT trace;
  };
  typedef struct SprintfCallEntry EntryT;

  // this maps a callsite to SprintfCallEntry
  typedef std::map<CallInst*, EntryT*> EntryMap;
  #define Entries (*(EntryMap *)(Ctx->get("SprintfEntries")))

  CallInstSet getCIS(std::string);
  std::string getFmtStr(llvm::CallInst*);
  bool isWrapper(llvm::CallInst*, unsigned);
  unsigned getBufSizeLocal(llvm::Value* arg);
  void itrWrapper(llvm::CallInst*, EntryT*);
  unsigned getCallerIdx(llvm::Function*, llvm::Value*);
  void pushWrapperTrace(llvm::CallInst*, EntryT*);
  WrapperT* getWrapperTrace(EntryT*);
  llvm::Value* getArgAt(llvm::CallInst*, unsigned);
  void replaceall(std::string& str, const std::string& from, const std::string& to);
  unsigned getFinalStrLen(std::string raw);
  
  
public:
  SprintfPass(GlobalContext *Ctx_):Ctx(Ctx_){
    Ctx->add("SprintfEntries", new EntryMap());
  };
  void run();
  void dump();
};