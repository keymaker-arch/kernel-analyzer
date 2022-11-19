#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>

#include <fstream>
#include <iostream>
#include <vector>

#include "BufSizer.h"
#include "FmtStrParser.h"

using namespace llvm;


std::ofstream resultFile;

cl::opt<std::string> BCFiles("f", cl::Required, cl::desc("a file containing input bc file pathes, one per line"));

namespace{
  using ArgList = std::vector<Value*>;

  class BugEntry{
    public:
    std::string ModuleName;
    unsigned Line;
    std::string FuncName;
    ArgList args;
  };

  class Analyzor{
    private:
    void OutputFound(){};
    unsigned long getFinalStrLen(std::string);
    std::string getFormatStr(Value* arg);
    

    public:
    BufSizeAnalyzor BufSizer;
    Analyzor(Module &M);
    Analyzor(void){};
    ~Analyzor(){};
    void run(Module &M);
  };

  std::string Analyzor::getFormatStr(Value* arg){
    errs() << "Analyzor::getFormatStr() => " << *arg << "\n";
    if(auto* gep = dyn_cast<GEPOperator>(arg)){
      if(auto* glb = dyn_cast<GlobalVariable>(gep->getPointerOperand())){
        if(dyn_cast<ConstantDataArray>(glb->getInitializer())){
          std::string format_string = cast<ConstantDataArray>(glb->getInitializer())->getAsString().str();
          // errs() << "[INFO] format string: " << format_string << "\n";
          if(gep->hasAllZeroIndices() && gep->getNumIndices() == 2){
            return format_string;
          }else{
            errs() << "OOPs, more than 2 indices or(and) non-zero indices\n";
          }
        }else{
          errs() << "OOPs, the initial value of the global var is not constantdataarray\n";
        }
      }else{
        errs() << "OOPs, the pointer operand of GEP is not global var\n";
      }
    }else{
      errs() << "OOPs, the 2nd argument of sprintf is not a GEP\n";
    }
    return std::string();
  }

  bool wrapper_filter(Function &F, User::op_iterator arg){
    for(auto* param = F.arg_begin();param != F.arg_end();param++){
      if(param == arg->get()){
        return true;
      }
    }
    return false;
  }

  void Analyzor::run(Module &M){
    for(auto& F : M){
      for(auto& B : F){
        for(auto& I : B){
          if(auto* inst = dyn_cast<CallInst>(&I)){  // found a call instruction
            Function* calledF = inst->getCalledFunction();
            if(calledF){
              if(calledF->getName().str() == "sprintf"){  // found a sprintf()
                errs() << "[INFO] sprintf() found in F: " << F.getName().str() << " L: " << I.getDebugLoc().getLine() << "\n";

                std::string format_string;
                unsigned long longest_string_len;
                unsigned long buffer_size = -1;
                
                auto* arg = inst->arg_begin();

                // TODO: check if the buffer was passed in
                if(wrapper_filter(F, arg)){
                  errs() << "wrapper function\n";
                }else{
                  // get buffer size
                  buffer_size = BufSizer.getBufSize(arg->get());
                }

                arg++;
                // get the format string
                format_string = getFormatStr(arg->get());
                longest_string_len = FmtStrParser::getFinalStrLen(format_string);
                if(buffer_size < longest_string_len) errs() << "[CRITICAL] POSSIBLE BUG: buflen=>" << buffer_size << ", fmtstrlen=>" << longest_string_len <<"\n";
                errs() << "\n";

                arg++;
                // get the arguments of sprintf
                ArgList arg_list;
                for(;arg!=inst->arg_end();arg++){
                  arg_list.push_back(arg->get());
                }

                
              }
            }
          }
        }
      }
    }
  }

  Analyzor::Analyzor(Module &M){
    run(M);
  }
}

bool do_basic_selection(Module& M){
  if(M.getFunction("sprintf")){
    errs() << "[INFO] sprintf() used in this module: " << M.getName() << "\n";
    return true;
  }else{
    // errs() << "[INFO] sprintf() not used in this module: " << M.getName() << "\n";
    return false;
  }
}



int main(int argc, char const *argv[]){
  sys::PrintStackTraceOnErrorSignal(StringRef());
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;

  resultFile.open("./result");

  cl::ParseCommandLineOptions(argc, argv);
  SMDiagnostic Err;

  std::ifstream in_file(BCFiles);
  if(!in_file.is_open()){
    errs() << "cannot open bc file list\n";return -1;
  }
  std::vector<std::string> lines;
  std::string line;
  while(std::getline(in_file, line)){
    lines.push_back(line);
  }
  outs() << "[CRITICAL] input bc file count: " << lines.size() << "\n";

  Analyzor analyzor;
  LLVMContext *LLVMCtx;
  Module *module;
  for(unsigned i=0; i<lines.size(); i++){
    LLVMCtx = new LLVMContext();
    // errs() << "loading bc file => " << lines[i] << "\n";
    if(!(i%100)){
      outs() << i << "\n";
    }
    std::unique_ptr<Module> M = parseIRFile(lines[i], Err, *LLVMCtx);
    if(M == NULL){
      errs() << "error loading IR file: " << lines[i] << "\n";
      continue;
    }
    module = M.release();
    if(do_basic_selection(*module)){
      analyzor.run(*module);
      errs() << "\n";
    }
    delete module;
    delete LLVMCtx;
  }
  
  return 0;
}
