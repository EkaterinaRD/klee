#ifndef KLEE_DATABASE_H
#define KLEE_DATABASE_H

/* -*- mode: c++; c-basic-offset: 2; -*- */
#include <cstdint>
#include <sqlite3.h>
#include <sys/types.h>
#include <utility>
#include <vector>

#include "Path.h"
#include "ProofObligation.h"
#include "klee/Expr/Expr.h"

namespace klee {

struct Lemma;

// Wrapper class for database interaction
class Database {
private:
  std::string db_filename;
  ::sqlite3 *db = nullptr;

  void finalize(const char *sql_create, sqlite3_stmt *st);

public:
  Database(std::string &db_filename);
  ~Database();

  struct DBLemma {
    std::string path;
    std::vector<uint64_t> exprs;
    explicit DBLemma(const unsigned char *_path) {
      path = std::string(reinterpret_cast<const char *>(_path));
    }
    DBLemma(const DBLemma &l) {
      path = l.path;
      exprs = l.exprs;
    }
  };

  struct DBState {
    std::string initLoc;
    std::string currLoc;
    std::string choiceBranch;
    std::string solverResult;
    std::string path;
    int countInstr;
    int isolated;
    int terminated;
    int reached;
    std::vector<std::pair<uint64_t, std::string>> expr_instr;
  };

  struct DBPob {
    unsigned root_id;
    unsigned parent_id;
    std::string location;
    std::string path;
    // std::vector<uint64_t> exprs;
    // std::vector<uint64_t> instr_expr;
    std::vector<std::pair<uint64_t, std::string>> expr_instr;
    std::vector<unsigned> children;
    // std::vector<std::string> stack;
    std::map<int64_t, std::string> stack;
  };

  int64_t array_write(const Array *a);
  int64_t expr_write(ref<Expr> e);
  int64_t lemma_write(const Path &path);
  int64_t functionhash_write(std::string name, size_t functionhash);
  void parent_write(int64_t child, int64_t parent);
  void constraint_write(int64_t expr, int64_t summary);
  void arraymap_write(int64_t array, int64_t expr);
  void pob_write(ProofObligation *pob);
  void pobsChildren_write(ProofObligation *pob);
  void pobsConstr_write(unsigned pob_id, uint64_t expr_id, std::string instr);
  void pobPropCount_write(const ProofObligation *pob);
  void pobStack_write(const ProofObligation *pob);
  void maxId_write(std::uint32_t maxIdState, unsigned maxIdPob);
  void state_write(std::string values);
  void target_write(std::uint32_t state_id, std::string target);
  void child_write(std::uint32_t state_id, std::uint32_t child_id, std::string location);
  void statesConstr_write(uint32_t state_id, uint64_t expr_id, std::string instr);
  void prop_write(uint32_t state_id, unsigned pob_id);

  std::string array_retrieve(int64_t id);
  std::string expr_retrieve(int64_t id);
  std::string lemma_retrieve(int64_t id);


  std::map<uint64_t, std::string> arrays_retrieve();
  std::map<uint64_t, std::string> exprs_retrieve();
  std::map<uint64_t, DBLemma> lemmas_retrieve();
  std::map<std::string, size_t> functionhash_retrieve();
  std::set<std::pair<uint64_t, uint64_t>> parents_retrieve();
  std::set<std::pair<uint64_t, uint64_t>> pobsChildren_retrieve();
  std::pair<std::uint32_t, unsigned> maxId_retrieve();
  std::map<unsigned, DBPob> pobs_retrieve();
  // std::vector<uint64_t> pobConstr_retrieve(std::string pob_id);
  std::vector<std::pair<uint64_t, std::string>> pobConstr_retrieve(std::string pob_id);
  std::vector<unsigned> pobChildren_retrieve(std::string pob_id);
  std::map<uint64_t, std::map<std::uint32_t, unsigned>> propsCount_retrieve();
  std::map<int64_t, std::string> pobStack_retrieve(std::string pob_id);
  std::map<uint32_t, DBState> states_retrieve();
  std::vector<std::string> targets_retrieve(std::string pob_id);
  std::vector<std::pair<uint64_t, std::string>> statesConstr_retrieve(std::string state_id);

  void lemma_delete(uint64_t);
  void hash_delete(std::string);
  void state_delete(std::uint32_t state_id);

  void exprs_purge();
  void arrays_purge();

  void create_schema();
  void drop_schema();
};
} // namespace klee


#endif /*KLEE_DATABASE_H*/