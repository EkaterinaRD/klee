//===-- ExecutionState.cpp ------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ExecutionState.h"

#include "Memory.h"

#include "klee/Expr/ArrayExprVisitor.h"
#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprHashMap.h"
#include "klee/Module/Cell.h"
#include "klee/Module/InstructionInfoTable.h"
#include "klee/Module/KInstruction.h"
#include "klee/Module/KModule.h"
#include "klee/Support/Casting.h"
#include "klee/Support/OptionCategories.h"

#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <queue>
#include <sstream>
#include <stdarg.h>

#include <fstream>
#include <string>

using namespace llvm;
using namespace klee;

namespace {
cl::opt<bool> DebugLogStateMerge(
    "debug-log-state-merge", cl::init(false),
    cl::desc("Debug information for underlying state merging (default=false)"),
    cl::cat(MergeCat));
}

StackFrame::StackFrame(KInstIterator _caller, KFunction *_kf)
  : caller(_caller), kf(_kf), callPathNode(0), 
    minDistToUncoveredOnReturn(0), varargs(0) {
  locals = new Cell[kf->numRegisters];
}

StackFrame::StackFrame(const StackFrame &s) 
  : caller(s.caller),
    kf(s.kf),
    callPathNode(s.callPathNode),
    allocas(s.allocas),
    minDistToUncoveredOnReturn(s.minDistToUncoveredOnReturn),
    varargs(s.varargs) {
  locals = new Cell[s.kf->numRegisters];
  for (unsigned i=0; i<s.kf->numRegisters; i++)
    locals[i] = s.locals[i];
}

StackFrame::~StackFrame() { 
  delete[] locals;
}

void StackFrame::print() const {
  for(unsigned i = 0; i < kf->numRegisters; i++) {
    if(!locals[i].value.isNull()) {
      llvm::errs() << i << ":  " << locals[i].value << "\n";
    }
  }
  llvm::errs() << "\n";
}

/***/

ExecutionState::ExecutionState(KFunction *kf) :
    initPC(nullptr),
    pc(nullptr),
    prevPC(nullptr),
    stackBalance(0),
    incomingBBIndex(-1),
    depth(0),
    ptreeNode(nullptr),
    steppedInstructions(0),
    steppedMemoryInstructions(0),
    instsSinceCovNew(0),
    coveredNew(false),
    forkDisabled(false),
    isolated(false),
    redundant(false),
    targets(),
    path({kf->entryKBlock->basicBlock})
{
  pushFrame(nullptr, kf);
  stackBalance = 0;
}

ExecutionState::ExecutionState(KFunction *kf, KBlock *kb) :
    initPC(kb->instructions),
    pc(initPC),
    prevPC(pc),
    stackBalance(0),
    incomingBBIndex(-1),
    depth(0),
    ptreeNode(nullptr),
    steppedInstructions(0),
    steppedMemoryInstructions(0),
    instsSinceCovNew(0),
    coveredNew(false),
    forkDisabled(false),
    isolated(false),
    redundant(false),
    targets(),
    path({kb->basicBlock})
{
  pushFrame(nullptr, kf);
  stackBalance = 0;
}

ExecutionState::~ExecutionState() {
  for (const auto &cur_mergehandler: openMergeStack){
    cur_mergehandler->removeOpenState(this);
  }

  while (!stack.empty()) popFrame();
}

ExecutionState::ExecutionState(const ExecutionState& state):
    initPC(state.initPC),
    pc(state.pc),
    prevPC(state.prevPC),
    stack(state.stack),
    stackBalance(state.stackBalance),
    incomingBBIndex(state.incomingBBIndex),
    depth(state.depth),
    multilevel(state.multilevel),
    level(state.level),
    transitionLevel(state.transitionLevel),
    addressSpace(state.addressSpace),
    constraints(state.constraints),
    pathOS(state.pathOS),
    symPathOS(state.symPathOS),
    executionPath(state.executionPath),
    coveredLines(state.coveredLines),
    symbolics(state.symbolics),
    arrayNames(state.arrayNames),
    openMergeStack(state.openMergeStack),
    steppedInstructions(state.steppedInstructions),
    steppedMemoryInstructions(state.steppedMemoryInstructions),
    instsSinceCovNew(state.instsSinceCovNew),
    unwindingInformation(state.unwindingInformation
                             ? state.unwindingInformation->clone()
                             : nullptr),
    coveredNew(state.coveredNew),
    forkDisabled(state.forkDisabled),
    isolated(state.isolated),
    redundant(state.redundant),
    targets(state.targets),
    path(state.path)
{
  for (const auto &cur_mergehandler: openMergeStack)
    cur_mergehandler->addOpenState(this);
}

ExecutionState *ExecutionState::branch() {
  depth++;

  auto *falseState = new ExecutionState(*this);
  falseState->coveredNew = false;
  falseState->coveredLines.clear();

  return falseState;
}

ExecutionState *ExecutionState::withKFunction(KFunction *kf) const {
  assert(stack.size() == 0);
  ExecutionState *newState = new ExecutionState(*this);
  newState->pushFrame(nullptr, kf);
  newState->stackBalance = 0;
  newState->initPC = kf->blockMap[&*kf->function->begin()]->instructions;
  newState->pc = newState->initPC;
  newState->prevPC = newState->pc;
  return newState;
}

ExecutionState *ExecutionState::withKBlock(KBlock *kb) const {
  assert(stack.size() == 0);
  ExecutionState *newState = new ExecutionState(*this);
  newState->pushFrame(nullptr, kb->parent);
  newState->stackBalance = 0;
  newState->initPC = kb->instructions;
  newState->pc = newState->initPC;
  newState->prevPC = newState->pc;
  return newState;
}

ExecutionState *ExecutionState::copy() const {
  return new ExecutionState(*this);
}

void ExecutionState::pushFrame(KInstIterator caller, KFunction *kf) {
  stack.emplace_back(StackFrame(caller, kf));
  ++stackBalance;
}

void ExecutionState::popFrame() {
  const StackFrame &sf = stack.back();
  for (const auto * memoryObject : sf.allocas)
    addressSpace.unbindObject(memoryObject);
  stack.pop_back();
  --stackBalance;
}

void ExecutionState::addSymbolic(const MemoryObject *mo, const Array *array) {
  symbolics.emplace_back(ref<const MemoryObject>(mo), array);
}

/**/

llvm::raw_ostream &klee::operator<<(llvm::raw_ostream &os, const MemoryMap &mm) {
  os << "{";
  MemoryMap::iterator it = mm.begin();
  MemoryMap::iterator ie = mm.end();
  if (it!=ie) {
    os << "MO" << it->first->id << ":" << it->second.get();
    for (++it; it!=ie; ++it)
      os << ", MO" << it->first->id << ":" << it->second.get();
  }
  os << "}";
  return os;
}

bool ExecutionState::merge(const ExecutionState &b) {
  if (DebugLogStateMerge)
    llvm::errs() << "-- attempting merge of A:" << this << " with B:" << &b
                 << "--\n";
  if (pc != b.pc)
    return false;

  // XXX is it even possible for these to differ? does it matter? probably
  // implies difference in object states?

  if (symbolics != b.symbolics)
    return false;

  {
    std::vector<StackFrame>::const_iterator itA = stack.begin();
    std::vector<StackFrame>::const_iterator itB = b.stack.begin();
    while (itA!=stack.end() && itB!=b.stack.end()) {
      // XXX vaargs?
      if (itA->caller!=itB->caller || itA->kf!=itB->kf)
        return false;
      ++itA;
      ++itB;
    }
    if (itA!=stack.end() || itB!=b.stack.end())
      return false;
  }

  ExprHashSet aConstraints(constraints.begin(), constraints.end());
  ExprHashSet bConstraints(b.constraints.begin(), 
                                     b.constraints.end());
  ExprHashSet commonConstraints, aSuffix, bSuffix;
  std::set_intersection(aConstraints.begin(), aConstraints.end(),
                        bConstraints.begin(), bConstraints.end(),
                        std::inserter(commonConstraints, commonConstraints.begin()));
  std::set_difference(aConstraints.begin(), aConstraints.end(),
                      commonConstraints.begin(), commonConstraints.end(),
                      std::inserter(aSuffix, aSuffix.end()));
  std::set_difference(bConstraints.begin(), bConstraints.end(),
                      commonConstraints.begin(), commonConstraints.end(),
                      std::inserter(bSuffix, bSuffix.end()));
  if (DebugLogStateMerge) {
    llvm::errs() << "\tconstraint prefix: [";
    for (ExprHashSet::iterator it = commonConstraints.begin(),
                                        ie = commonConstraints.end();
         it != ie; ++it)
      llvm::errs() << *it << ", ";
    llvm::errs() << "]\n";
    llvm::errs() << "\tA suffix: [";
    for (ExprHashSet::iterator it = aSuffix.begin(),
                                        ie = aSuffix.end();
         it != ie; ++it)
      llvm::errs() << *it << ", ";
    llvm::errs() << "]\n";
    llvm::errs() << "\tB suffix: [";
    for (ExprHashSet::iterator it = bSuffix.begin(),
                                        ie = bSuffix.end();
         it != ie; ++it)
      llvm::errs() << *it << ", ";
    llvm::errs() << "]\n";
  }

  // We cannot merge if addresses would resolve differently in the
  // states. This means:
  // 
  // 1. Any objects created since the branch in either object must
  // have been free'd.
  //
  // 2. We cannot have free'd any pre-existing object in one state
  // and not the other

  if (DebugLogStateMerge) {
    llvm::errs() << "\tchecking object states\n";
    llvm::errs() << "A: " << addressSpace.objects << "\n";
    llvm::errs() << "B: " << b.addressSpace.objects << "\n";
  }
    
  std::set<const MemoryObject*> mutated;
  MemoryMap::iterator ai = addressSpace.objects.begin();
  MemoryMap::iterator bi = b.addressSpace.objects.begin();
  MemoryMap::iterator ae = addressSpace.objects.end();
  MemoryMap::iterator be = b.addressSpace.objects.end();
  for (; ai!=ae && bi!=be; ++ai, ++bi) {
    if (ai->first != bi->first) {
      if (DebugLogStateMerge) {
        if (ai->first < bi->first) {
          llvm::errs() << "\t\tB misses binding for: " << ai->first->id << "\n";
        } else {
          llvm::errs() << "\t\tA misses binding for: " << bi->first->id << "\n";
        }
      }
      return false;
    }
    if (ai->second.get() != bi->second.get()) {
      if (DebugLogStateMerge)
        llvm::errs() << "\t\tmutated: " << ai->first->id << "\n";
      mutated.insert(ai->first);
    }
  }
  if (ai!=ae || bi!=be) {
    if (DebugLogStateMerge)
      llvm::errs() << "\t\tmappings differ\n";
    return false;
  }
  
  // merge stack

  ref<Expr> inA = ConstantExpr::alloc(1, Expr::Bool);
  ref<Expr> inB = ConstantExpr::alloc(1, Expr::Bool);
  for (ExprHashSet::iterator it = aSuffix.begin(), 
         ie = aSuffix.end(); it != ie; ++it)
    inA = AndExpr::create(inA, *it);
  for (ExprHashSet::iterator it = bSuffix.begin(), 
         ie = bSuffix.end(); it != ie; ++it)
    inB = AndExpr::create(inB, *it);

  // XXX should we have a preference as to which predicate to use?
  // it seems like it can make a difference, even though logically
  // they must contradict each other and so inA => !inB

  std::vector<StackFrame>::iterator itA = stack.begin();
  std::vector<StackFrame>::const_iterator itB = b.stack.begin();
  for (; itA!=stack.end(); ++itA, ++itB) {
    StackFrame &af = *itA;
    const StackFrame &bf = *itB;
    for (unsigned i=0; i<af.kf->numRegisters; i++) {
      ref<Expr> &av = af.locals[i].value;
      const ref<Expr> &bv = bf.locals[i].value;
      if (!av || !bv) {
        // if one is null then by implication (we are at same pc)
        // we cannot reuse this local, so just ignore
      } else {
        av = SelectExpr::create(inA, av, bv);
      }
    }
  }

  for (std::set<const MemoryObject*>::iterator it = mutated.begin(), 
         ie = mutated.end(); it != ie; ++it) {
    const MemoryObject *mo = *it;
    const ObjectState *os = addressSpace.findObject(mo);
    const ObjectState *otherOS = b.addressSpace.findObject(mo);
    assert(os && !os->readOnly && 
           "objects mutated but not writable in merging state");
    assert(otherOS);

    ObjectState *wos = addressSpace.getWriteable(mo, os);
    for (unsigned i=0; i<mo->size; i++) {
      ref<Expr> av = wos->read8(i);
      ref<Expr> bv = otherOS->read8(i);
      wos->write(i, SelectExpr::create(inA, av, bv));
    }
  }

  ConstraintSet oldConstraints = constraints;
  constraints = ConstraintSet();

  ConstraintManager m(constraints);
  for (const auto &constraint : commonConstraints)
    m.addConstraint(constraint, oldConstraints.get_location(constraint));
  m.addConstraint(OrExpr::create(inA, inB), std::nullopt);

  return true;
}

void ExecutionState::dumpStack(llvm::raw_ostream &out) const {
  unsigned idx = 0;
  const KInstruction *target = prevPC;
  for (ExecutionState::stack_ty::const_reverse_iterator
         it = stack.rbegin(), ie = stack.rend();
       it != ie; ++it) {
    const StackFrame &sf = *it;
    Function *f = sf.kf->function;
    const InstructionInfo &ii = *target->info;
    out << "\t#" << idx++;
    std::stringstream AssStream;
    AssStream << std::setw(8) << std::setfill('0') << ii.assemblyLine;
    out << AssStream.str();
    out << " in " << f->getName().str() << " (";
    // Yawn, we could go up and print varargs if we wanted to.
    unsigned index = 0;
    for (Function::arg_iterator ai = f->arg_begin(), ae = f->arg_end();
         ai != ae; ++ai) {
      if (ai!=f->arg_begin()) out << ", ";

      out << ai->getName().str();
      // XXX should go through function
      ref<Expr> value = sf.locals[sf.kf->getArgRegister(index++)].value;
      if (isa_and_nonnull<ConstantExpr>(value))
        out << "=" << value;
    }
    out << ")";
    if (ii.file != "")
      out << " at " << ii.file << ":" << ii.line;
    out << "\n";
    target = sf.caller;
  }
}

void ExecutionState::addConstraint(ref<Expr> e, std::optional<size_t> loc, bool *sat) {
  ConstraintManager c(constraints);
  c.addConstraint(e, loc, sat);
}

int ExecutionState::resolveLazyInstantiation(std::map<ref<Expr>, std::pair<Symbolic, ref<Expr>>> &resolved) {
  int status = 0;
  for (auto i : symbolics) {
    if (!i.first->isLazyInstantiated()) {
      continue;
    }
    status = 1;
    auto lisource = i.first->lazyInstantiatedSource;
    switch (lisource->getKind()) {
    case Expr::Read: {
      ref<ReadExpr> base = dyn_cast<ReadExpr>(lisource);
      auto parent = base->updates.root->binding->getObject();
      if (!parent) {
        return -1;
      }
      resolved[lisource] = std::make_pair(
        std::make_pair(parent, base->updates.root),
        base->index);
      break;
    } 
    case Expr::Concat: {
      ref<ReadExpr> base =
          ArrayExprHelper::hasOrderedReads(*dyn_cast<ConcatExpr>(lisource));
      auto parent = base->updates.root->binding->getObject();
      if (!parent) {
        return -1;
      }
      resolved[lisource] = std::make_pair(
        std::make_pair(parent, base->updates.root),
        base->index);
      break;
    }
    default:
      return -1;
    }
  }
  return status;
}

void ExecutionState::exctractForeignSymbolics(std::vector<Symbolic> &foreign) {
  std::map<ref<Expr>, std::pair<Symbolic, ref<Expr>>> resolved;
  resolveLazyInstantiation(resolved);
  for (auto &moArray : symbolics) {
    if (moArray.second->isForeign)
      foreign.push_back(moArray);
  }

  bool keepSearching = true;
  Symbolic parent;
  while (keepSearching) {
    keepSearching = false;
    for (auto &moArray : symbolics) {
      if (moArray.first->isLazyInstantiated()) {
        parent = resolved[moArray.first->lazyInstantiatedSource].first;
        if (std::find(foreign.begin(), foreign.end(), parent) != foreign.end()) {
          foreign.push_back(moArray);
          keepSearching = true;
        }
      }
    }
  }
}

BasicBlock *ExecutionState::getInitPCBlock() const{
  return initPC->inst->getParent();
}

BasicBlock *ExecutionState::getPrevPCBlock() const {
  return prevPC->inst->getParent();
}

BasicBlock *ExecutionState::getPCBlock() const {
  return pc->inst->getParent();
}

void ExecutionState::addLevel(BasicBlock *srcbb, BasicBlock *dstbb) {
  multilevel.insert(srcbb);
  level.insert(srcbb);
  transitionLevel.insert(std::make_pair(srcbb, dstbb));
}

bool ExecutionState::isEmpty() const {
  return stack.empty();
}

bool ExecutionState::isCriticalPC() const {
  KInstruction *ki = pc;
  KInstruction *prevKI = prevPC;
  return ((this->getPCBlock()->hasNPredecessors(0) && (this->isIntegrated() || stack.size() > 1)) ||
          (prevKI->inst->isTerminator() && prevKI != ki &&
            (this->getPCBlock()->hasNPredecessorsOrMore(2) ||
             prevKI->parent->getKBlockType() == KBlockType::Call ||
             ki->parent->getKBlockType() == KBlockType::Call)));
}

bool ExecutionState::isIntegrated() const {
  return !isolated;
}

bool ExecutionState::isIsolated() const {
  return isolated;
}

void ExecutionState::printCompareList(const ExecutionState &fst, const ExecutionState &snd, 
                                      llvm::raw_ostream &os) {
  os << divider(50) << divider(50) << divider(50);
  os << "STACK\n";
  if(fst.stack.back().kf != snd.stack.back().kf) {
    os << "Functions mismatched\n\n";
  }
  auto & fstFrame = fst.stack.back();
  auto & sndFrame = snd.stack.back();
  for (unsigned i = 0; i < fstFrame.kf->numRegisters; i++) {
    if(!fstFrame.locals[i].value.isNull() || !sndFrame.locals[i].value.isNull()) {
      os << "reg " << i << ":\n";
      os << "state 1\n";
      if(!fstFrame.locals[i].value.isNull())
        os << fstFrame.locals[i].value << "\n";
      else
        os << "NONE\n";
      os << "state 2\n";
      if(!sndFrame.locals[i].value.isNull())
        os << sndFrame.locals[i].value << "\n";
      else
        os << "NONE\n";
      os << "\n" << divider(40);
    }
  }
  os << divider(40) << divider(40);


  os << "CONSTRAINTS\n";
  os << "state 1 constraints\n" << fst.constraints << "\n" << divider(40);
  os << "state 2 constraints\n" << fst.constraints << "\n" << divider(40);


  os << "OBJECTS\n";
  std::set<const MemoryObject*> common;
  std::set<const MemoryObject*> onlyFirst;
  std::set<const MemoryObject*> onlySecond;
  for(const auto & obj : fst.addressSpace.objects) {
    const ObjectState *os = snd.addressSpace.findObject(obj.first);
    if(os) common.insert(obj.first);
    else   onlyFirst.insert(obj.first);
  }
  for(const auto & obj : snd.addressSpace.objects) {
    if(common.find(obj.first) == common.end()) 
      onlySecond.insert(obj.first);
  }
  os << "Common objects : \n";
  for(auto & obj : common) {
    const ObjectState * objState1 = fst.addressSpace.findObject(obj);
    const ObjectState * objState2 = snd.addressSpace.findObject(obj);
    assert(objState1 && objState2);
    os << obj->name << "\n";
    os << "state 1\n" << objState1->read(0, 8*objState1->size) << "\n";
    os << "state 2\n" << objState2->read(0, 8*objState2->size) << "\n";
    os << "\n";
  }
  os << "Only in state 1 : \n";
  for(auto & obj : onlyFirst) {
    const ObjectState * objState1 = fst.addressSpace.findObject(obj);
    assert(objState1);
    os << obj->name << "\n";
    os << objState1->read(0, 8*objState1->size) << "\n";
    os << "\n";
  }
  os << "Only in state 2 : \n";
  for(auto & obj : onlySecond) {
    const ObjectState * objState2 = snd.addressSpace.findObject(obj);
    assert(objState2);
    os << obj->name << "\n";
    os << objState2->read(0, 8*objState2->size) << "\n";
    os << "\n";
  }
}

void ExecutionState::print(llvm::raw_ostream &os) const {
  os << divider(50) << divider(50) << divider(50);
  os << "STACK\n";
  auto & frame = this->stack.back();
  for (unsigned i = 0; i < frame.kf->numRegisters; i++) {
    if(!frame.locals[i].value.isNull()) {
      os << "reg " << i << ":\n";
      os << frame.locals[i].value << "\n";
      os << "\n" << divider(40);
    }
  }
  os << divider(40) << divider(40);


  os << "CONSTRAINTS\n";
  os << this->constraints;


  os << "OBJECTS\n";
  for(auto & obj : this->addressSpace.objects) {
    const ObjectState * objState = obj.second.get();
    os << obj.first->name << "\n";
    os << objState->read(0, 8*objState->size) << "\n";
    os << "\n";
  }
}
