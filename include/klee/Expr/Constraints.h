//===-- Constraints.h -------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_CONSTRAINTS_H
#define KLEE_CONSTRAINTS_H

#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprHashMap.h"
#include "klee/Module/KInstruction.h"
#include <memory>
#include <string>

namespace klee {

extern llvm::cl::opt<bool> RewriteEqualities;

/// Resembles a set of constraints that can be passed around
///
class ConstraintSet {
  friend class ConstraintManager;

public:
  using constraints_ty = std::vector<ref<Expr>>;
  using iterator = constraints_ty::iterator;
  using const_iterator = constraints_ty::const_iterator;

  using constraint_iterator = const_iterator;

  bool empty() const;
  constraint_iterator begin() const;
  constraint_iterator end() const;
  size_t size() const noexcept;

  explicit ConstraintSet(constraints_ty cs) : constraints(std::move(cs)) {}
  ConstraintSet() = default;

  void push_back(const ref<Expr> &e);

  bool operator==(const ConstraintSet &b) const {
    return constraints == b.constraints;
  }

private:
  constraints_ty constraints;
};

class Constraints {
  friend class ConstraintManager;

public:
  bool empty() const;
  ConstraintSet::constraint_iterator begin() const;
  ConstraintSet::constraint_iterator end() const;
  size_t size() const noexcept;

  Constraints() = default;
  void insert(const ref<Expr> &e, KInstruction *location);
  KInstruction *getLocation(const ref<Expr> &e) const;
  const ConstraintSet &set() const;

  bool operator==(const Constraints &b) const {
    return constraints == b.constraints;
  }

  operator const ConstraintSet &() const { return constraints; }

private:
  ConstraintSet constraints;
  ExprHashMap<KInstruction *> constraintLocations;
};

class ExprVisitor;

/// Manages constraints, e.g. optimisation
class ConstraintManager {
public:
  /// Create constraint manager that modifies constraints
  /// \param constraints
  explicit ConstraintManager(Constraints &constraints);

  /// Simplify expression expr based on constraints
  /// \param constraints set of constraints used for simplification
  /// \param expr to simplify
  /// \return simplified expression
  static ref<Expr> simplifyExpr(const ConstraintSet &constraints,
                                const ref<Expr> &expr);

  /// Add constraint to the referenced constraint set
  /// \param constraint
  void addConstraint(const ref<Expr> &constraint, KInstruction *location, bool *sat = 0);

private:
  /// Rewrite set of constraints using the visitor
  /// \param visitor constraint rewriter
  /// \return true iff any constraint has been changed
  bool rewriteConstraints(ExprVisitor &visitor, bool *sat = 0);

  /// Add constraint to the set of constraints
  void addConstraintInternal(const ref<Expr> &constraint, KInstruction *location, bool *sat = 0);

  Constraints &constraints;
};

#ifndef divider
#define divider(n) std::string(n, '-') + "\n"
#endif

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const ConstraintSet &constraints) {
  if (!constraints.empty()) {
    os << "\n" << divider(30);
    for(const auto &expr: constraints) {
      os << divider(30);
      os << expr << "\n";
      os << divider(30);
    }
    os << divider(30);
  }
  return os;
}

} // namespace klee

#endif /* KLEE_CONSTRAINTS_H */
