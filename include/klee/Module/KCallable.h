#ifndef KLEE_KCALLABLE_H
#define KLEE_KCALLABLE_H

namespace llvm {
  class Function;
  class InlineAsm;
  class LLVMContext;
  class StringRef;
  class PointerType;
}

namespace klee {
  class KCallable {
  private:
    static unsigned globalAsmId;

    union {
      llvm::Function* func;
      struct {
        llvm::InlineAsm* asmValue;
        unsigned asmId;
      };
    };
    bool isFunc;
  public:
    KCallable(llvm::Function*);
    KCallable(llvm::InlineAsm*);
  
    llvm::Function* getFunction();
    llvm::InlineAsm* getInlineAsm();

    llvm::PointerType* getType() const ;

    llvm::StringRef getName() const;
    llvm::LLVMContext &getContext() const;

    bool isFunction() const;

  };
}

#endif /* KLEE_KCALLABLE_H */
