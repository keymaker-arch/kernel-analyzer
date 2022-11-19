#include <llvm/IR/Module.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Instructions.h>

#include <regex>

#include "Sprintf.h"

using namespace llvm;

/*
  a simple wrapper to help expand excape charactors 
*/
void SprintfPass::replaceall(std::string& str, const std::string& from, const std::string& to){
  if(from.empty())
    return;
  size_t start_pos = 0;
  while((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
}



/*
  expand all escape charactor to its longest form
*/
unsigned SprintfPass::getFinalStrLen(std::string raw){
  if(raw.empty()) return 0;
  // ignore the %s case
  if(std::regex_match(raw, std::regex("%s"))) return 0;

  std::string final_string = raw;
  replaceall(final_string, "\n", "NL");
  replaceall(final_string, "\t", "TB");
  llvm::errs() << "[INFO] format string: " << final_string << "\n";

  final_string = std::regex_replace(final_string, std::regex("%(\\.)*[0-9]*[du]"), "aaaaaaaaaa");
  final_string = std::regex_replace(final_string, std::regex("%(\\.)*[0-9]*h[du]"), "aaaaa");
  final_string = std::regex_replace(final_string, std::regex("%(\\.)*[0-9]*hh[du]"), "aaa");
  final_string = std::regex_replace(final_string, std::regex("%(\\.)*[0-9]*ld"), "aaaaaaaaaaaaaaaaaaa");
  final_string = std::regex_replace(final_string, std::regex("%(\\.)*[0-9]*lld"), "aaaaaaaaaaaaaaaaaaa");
  final_string = std::regex_replace(final_string, std::regex("%(\\.)*[0-9]*lu"), "aaaaaaaaaaaaaaaaaaaa");
  final_string = std::regex_replace(final_string, std::regex("%(\\.)*[0-9]*llu"), "aaaaaaaaaaaaaaaaaaaa");
  final_string = std::regex_replace(final_string, std::regex("%(\\.)*[0-9]*x"), "aaaaaaaa");
  final_string = std::regex_replace(final_string, std::regex("%(\\.)*[0-9]*hx"), "aaaa");
  final_string = std::regex_replace(final_string, std::regex("%(\\.)*[0-9]*hhx"), "aa");
  final_string = std::regex_replace(final_string, std::regex("%(\\.)*[0-9]*llx"), "aaaaaaaaaaaaaaaa");
  final_string = std::regex_replace(final_string, std::regex("%(\\.)*[0-9]*lx"), "aaaaaaaaaaaaaaaa");
  final_string = std::regex_replace(final_string, std::regex("%c"), "a");

  return final_string.length();
}

/*
  get the value at given index of a callinst
*/
llvm::Value* SprintfPass::getArgAt(llvm::CallInst* CI, unsigned idx){
  assert(idx<CI->arg_size()-1);
  unsigned cnt = 0;
  for(auto* arg = CI->arg_begin();arg != CI->arg_end();arg++){
    if(idx == cnt){
      return arg->get();
    }
    cnt++;
  }
  return NULL;
}


/*
  check if the caller of this callinst is a wrapper
  if the argument at given index in this callinst is passed in from its caller, we take the caller as a wrapper
*/
bool SprintfPass::isWrapper(CallInst* CI, unsigned idx=0){
  // auto* v = CI->arg_begin()->get();
  auto* v = getArgAt(CI, idx);
  auto* F = CI->getParent()->getParent();
  errs() << *F << "\n";
  errs() << *CI << "\n";
  for(auto* param = F->arg_begin();param != F->arg_end();param++){
    Value* v_ = cast<Value>(param);
    if(v_ == v){
      return true;
    }
  }
  return false;
}


/*
  get the format string from sprintf() callsite
*/
std::string SprintfPass::getFmtStr(CallInst* CI){
  // auto* arg = CI->arg_begin();
  // auto* v = (arg++)->get();
  auto* v = getArgAt(CI, 1);
  if(auto* gep = dyn_cast<GEPOperator>(v)){
    if(auto* glb = dyn_cast<GlobalVariable>(gep->getPointerOperand())){
      if(dyn_cast<ConstantDataArray>(glb->getInitializer())){
        std::string format_string = cast<ConstantDataArray>(glb->getInitializer())->getAsString().str();
        if(gep->hasAllZeroIndices() && gep->getNumIndices() == 2){
          return format_string;
        }else{
          errs() << "OOPs, SprintfPass::getFmtStr: more than 2 indices or(and) non-zero indices\n";
        }
      }else{
        errs() << "OOPs, SprintfPass::getFmtStr: the initial value of the global var is not constantdataarray\n";
      }
    }else{
      errs() << "OOPs, SprintfPass::getFmtStr: the pointer operand of GEP is not global var\n";
    }
  }else{
    errs() << "OOPs, SprintfPass::getFmtStr: the 2nd argument of sprintf is not a GEP\n";
  }
  return std::string();
}


/*
  return the call site set of a function
*/
CallInstSet SprintfPass::getCIS(std::string funcname){
  for(auto M : Ctx->Callers){
    Function* F = M.first;
    CallInstSet CIS = M.second;
    if(F->getName().str() == funcname)
      return CIS;
  }
  return CallInstSet();
}


// FIXME: expand the BufSizer to this class, we don't need a this function in other modules
unsigned SprintfPass::getBufSizeLocal(llvm::Value* arg){
  return BufSizer.getBufSize(arg);
}


/*
  push this wrapper call entry to the EntryT
*/
void SprintfPass::pushWrapperTrace(CallInst* CI, EntryT* p){
  auto* wp = new WrapperT;
  wp->bufv = CI->arg_begin()->get();
  wp->ci = CI;
  wp->idx = getCallerIdx(CI->getParent()->getParent(), wp->bufv);
  p->trace.insert(wp);
}

/*
  get the last wrapper record in EntryT
*/
SprintfPass::WrapperT* SprintfPass::getWrapperTrace(EntryT* p){
  return *p->trace.rbegin();
}


/*
  get the index of the buffer we are tracing in the caller Function
  
*/
unsigned SprintfPass::getCallerIdx(Function* CallerF, Value* arg){
  for(auto* param = CallerF->arg_begin();param != CallerF->arg_end();param++){
    if(param == arg) return param - CallerF->arg_begin();
  }
  assert(0);
}

/*
  DFS in the caller graph
*/
void SprintfPass::itrWrapper(CallInst* CI, EntryT* p){
  auto* CallerF = CI->getParent()->getParent();
  auto* wp = getWrapperTrace(p);
  CallInstSet CIS = getCIS(CallerF->getName().str());
  for(auto* CI_ : CIS){
    if(isWrapper(CI_, wp->idx)){
      pushWrapperTrace(CI_, p);
      itrWrapper(CI_, p);
    }else{
      // invoke getBufSizeLocal here to get the buffer size, complete the p and add it to ctx
      auto* v = getArgAt(CI_, wp->idx);
      unsigned bufsize = BufSizer.getBufSize(v);
      p->bufv = v;
      p->bufsize = bufsize;
      EntryT* np = new EntryT(*p);
      Entries[CI_] = np;
    }
  }
}


void SprintfPass::run(){
  CallInstSet CIS = getCIS("sprintf");
  CallInstSet CIS_wrapper; // we store wrapper call here to check them later
  for(auto* CI : CIS){
    std::string fmtstr = getFmtStr(CI);
    if(fmtstr.empty()) continue;

    EntryT* p = new SprintfCallEntry();
    p->fmtstr = fmtstr;
    for(auto* arg = CI->arg_begin()+2;arg != CI->arg_end();arg++){
      p->argv.insert(arg->get());
    }
    p->bufv = CI->arg_begin()->get();
    if(isWrapper(CI)){
      errs() << "wrapper found\n";
      pushWrapperTrace(CI, p);
      itrWrapper(CI, p);
      delete p;
    }else{
      p->bufsize = getBufSizeLocal(p->bufv);
      p->trace = TraceT();
      if(p->bufsize != BUFFSIZER_FAIL)
        Entries[CI] = p;  // now this sprintf() call is ready for analysis
    }
  }
}

void SprintfPass::dump(){
  errs() << "[SprintfPass] dump sprintf invocation entry begin\n";
  for(auto itr : Entries){
    auto* CI = itr.first;
    auto* entry = itr.second;
    errs() << CI->getParent()->getParent()->getParent()->getName() << ":" << CI->getDebugLoc().getLine() << "\n";
    errs() << *CI << "\n";
    errs() << "fmtstring: " << entry->fmtstr << "\n";
    errs() << "buf size => " << entry->bufsize << "\n";
    errs() << "\n";
  }
  errs() << "[SprintfPass] dump sprintf invocation entry end\n";
}