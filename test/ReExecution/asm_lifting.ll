; RUN: llvm-as %s -f -o %t1.bc
; RUN: rm -rf %t.klee-out1
; RUN: rm -rf %t.klee-out2
; RUN: %klee --output-dir=%t.klee-out1 --do-backward-first --optimize=false --write-to-db %t1.bc
; RUN: %klee --output-dir=%t.klee-out2 --do-backward-first --optimize=false --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 %t1.bc
; RUN: FileCheck %s --input-file=%t.klee-out2/assembly.ll

define i32 @asm_free() nounwind {
entry:
  call void asm sideeffect "", "~{memory},~{dirflag},~{fpsr},~{flags}"()
  ; Make sure simple memory barrier is lifted
  ; CHECK-NOT: call void asm sideeffect "", "~{memory},~{dirflag},~{fpsr},~{flags}"()
  ret i32 0
}

define i32 @unlifted_asm() nounwind {
entry:
  %0 = alloca [47 x i8], align 16
  %1 = getelementptr inbounds [47 x i8], [47 x i8]* %0, i64 0, i64 0
  ; Make sure memory barrier with function arguments is kept
  ; KLEE will put the call in a separate basic block so the index gets shifted
  %2 = call i8* asm sideeffect "", "=r,0,~{memory},~{dirflag},~{fpsr},~{flags}"(i8* nonnull %1)
  ; CHECK: %3 = call i8* asm sideeffect "", "=r,0,~{memory},~{dirflag},~{fpsr},~{flags}"(i8* nonnull %1)
  ret i32 0
}

define i32 @main() nounwind {
entry:
  ret i32 0
}
