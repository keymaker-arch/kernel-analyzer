#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>

using namespace llvm;

#define BUFFSIZER_FAIL -1


class BufSizeAnalyzor{
  private:
  unsigned long getIdxNR(User::op_iterator idx_begin, User::op_iterator idx_end);
  unsigned long getRealSize(ArrayType* arrayt, User::op_iterator idx_begin, User::op_iterator idx_end);
  unsigned long getRealSize(StructType* structt, User::op_iterator idx_begin, User::op_iterator idx_end);
  unsigned long getBufSize(GetElementPtrInst* gepi);
  unsigned long getBufSize(LoadInst* ldi);
  unsigned long getBufSize(GEPOperator* gepo);

  public:
  BufSizeAnalyzor(){};
  ~BufSizeAnalyzor(){};
  unsigned long getBufSize(Value* arg);
};