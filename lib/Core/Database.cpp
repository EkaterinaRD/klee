#include "Database.h"
#include "Path.h"
#include "ExecutionState.h"
#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprBuilder.h"
#include "klee/Expr/ExprPPrinter.h"
#include "klee/Expr/Parser/Parser.h"
#include "klee/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <sqlite3.h>
#include <string>

namespace klee {

Database::Database(std::string &db_filename) {
  this->db_filename = db_filename;
  if (sqlite3_open(db_filename.c_str(), &db) != SQLITE_OK) {
    sqlite3_close(db);
    exit(1);
  }

  char const *sql_check = "SELECT name FROM sqlite_master WHERE type='table'"
                          "AND name='metadata'";
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql_check, -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  auto result = sqlite3_step(st);
  if (sqlite3_finalize(st) != SQLITE_OK) {
    exit(1);
  }
  if (result == SQLITE_ROW) {
    return;
  }
  if (result == SQLITE_DONE) {
    create_schema();
    return;
  }
  exit(1);
}

Database::~Database() { sqlite3_close(db); }

void Database::finalize(const char *sql_create, sqlite3_stmt *st) {
  if (sqlite3_prepare_v2(db, sql_create, -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  if (sqlite3_step(st) != SQLITE_DONE) {
    exit(1);
  }
  if (sqlite3_finalize(st) != SQLITE_OK) {
    exit(1);
  }
}

void Database::create_schema() {
  sqlite3_stmt *st = nullptr;
  char const *sql_create = "CREATE TABLE metadata("
                           "id INT PRIMARY KEY NOT NULL);";
  finalize(sql_create, st);
  sql_create =
      "CREATE TABLE lemma (id INTEGER NOT NULL PRIMARY KEY, path TEXT);";
  finalize(sql_create, st);
  sql_create =
      "CREATE TABLE array (id INTEGER NOT NULL PRIMARY KEY, array TEXT);";
  finalize(sql_create, st);
  sql_create =
      "CREATE TABLE expr (id INTEGER NOT NULL PRIMARY KEY, expr TEXT);";
  finalize(sql_create, st);
  sql_create = "CREATE TABLE constr"
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "expr_id INTEGER REFERENCES expr(id) ON DELETE CASCADE,"
               "summary_id INTEGER REFERENCES lemma(id) ON DELETE CASCADE,"
               "UNIQUE(expr_id, summary_id))";
  finalize(sql_create, st);
  sql_create = "CREATE TABLE arraymap"
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "array_id INTEGER REFERENCES array(id) ON DELETE CASCADE,"
               "expr_id INTEGER REFERENCES expr(id) ON DELETE CASCADE,"
               "UNIQUE(array_id, expr_id))";
  finalize(sql_create, st);
  sql_create = "CREATE TABLE parent"
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "child_id INTEGER REFERENCES array(id) ON DELETE CASCADE,"
               "parent_id INTEGER REFERENCES array(id) ON DELETE CASCADE,"
               "UNIQUE(child_id, parent_id))";
  finalize(sql_create, st);
  sql_create = "CREATE TABLE functionhash"
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "function TEXT,"
               "hash TEXT,"
               "UNIQUE(function, hash))";
  finalize(sql_create, st);
  sql_create = "CREATE TABLE maxID"
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "maxIdState INTEGER,"
               "maxIdPob INTEGER"
               ")";
  finalize(sql_create, st);
  sql_create = "CREATE TABLE pobs"
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "root INTEGER,"
               "parent INTEGER,"
               "location TEXT,"
               "path TEXT"
               ")";
  finalize(sql_create, st);
  // Constraints condition;
  sql_create = "CREATE TABLE pobsConstr"
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "pob_id INTEGER REFERENCES pobs(id) ON DELETE CASCADE,"
               "expr_id INTEGER REFERENCES expr(id) ON DELETE CASCADE,"
               "instr TEXT,"
               "UNIQUE(pob_id, expr_id)"
               ")";
  finalize(sql_create, st);
  // unordered_set<ProofObligation *> children;
  sql_create = "CREATE TABLE pobsChildren "
               "(pob_id INTEGER,"
               "child_id INTEGER"
              //  "UNIQUE(pob_id, child_id)"
               ")";
  finalize(sql_create, st);
  // vector<KInstruction *> stack;
  sql_create = "CREATE TABLE pobsStack "
               "(pob_id INTEGER,"
               "numOfInstr INTEGER,"
               "instr TEXT"
               ")";
  finalize(sql_create, st);
  sql_create = "CREATE TABLE states "
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "initLoc TEXT,"
               "currLoc TEXT,"
               "choiceBranch TEXT,"
               "solverResult TEXT,"
               "path TEXT,"
               "countInstr INTEGER,"
               "isolated INTEGER,"
               "terminated INTEGER,"
               "reached INTEGER"
               ")";
  finalize(sql_create, st);    
  // map<ExecutionState *, unsigned> propagationCount;
  sql_create =  "CREATE TABLE propagationCount "
                "(pob_id INTEGER REFERENCES pobs(id) ON DELETE CASCADE,"
                "state_id INTEGER REFERENCES states(id) ON DELETE CASCADE,"
                "count INTEGER"
                ")";
  finalize(sql_create, st);
  sql_create = "CREATE TABLE statesConstr"
               "(id INTEGER NOT NULL PRIMARY KEY,"
               "state_id INTEGER REFERENCES states(id) ON DELETE CASCADE,"
               "expr_id INTEGER REFERENCES expr(id) ON DELETE CASCADE,"
               "instr TEXT,"
               "UNIQUE(state_id, expr_id)"
               ")";
  finalize(sql_create, st);            
  // propagations
  sql_create = "CREATE TABLE propagations"
               "(state_id INTEGER REFERENCES states(id) ON DELETE CASCADE,"
               "pob_id INTEGER REFERENCES pobs(id) ON DELETE CASCADE"
               ")";
  finalize(sql_create, st);
  // target gor isolated states
  sql_create = "CREATE TABLE targets"
               "(state_id INTEGER REFERENCES states(id) ON DELETE CASCADE,"
               "target TEXT"
               ")";
  finalize(sql_create, st); 
  sql_create = "CREATE TABLE child"
               "(state_id INTEGER REFERENCES states(id) ON DELETE CASCADE,"
               "child_id INTEGER REFERENCES states(id) ON DELETE CASCADE,"
               "location TEXT"
               ")";
  finalize(sql_create, st);
}

void Database::drop_schema() {
  sqlite3_stmt *st = nullptr;
  char const *sql_drop = "DROP TABLE IF EXISTS summary, array, expr,"
                         "constr, arraymap, parent, functionhash,"
                         "pobs, pobsConstr, pobsChildren, pobsStack, propagationCount";
  finalize(sql_drop, st);
}

int64_t Database::array_write(const Array *A) {
  std::string str;
  llvm::raw_string_ostream o(str);
  ExprPPrinter::printSingleArray(o, A);
  std::string sql = "INSERT INTO array (array) VALUES ('" + str + "');";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    return -1;
  } else {
    return sqlite3_last_insert_rowid(db);
  }
}

int64_t Database::expr_write(ref<Expr> e) {
  std::string str;
  llvm::raw_string_ostream o(str);
  ExprPPrinter::printSingleExpr(o, e);
  std::string sql = "INSERT INTO expr (expr) VALUES ('" + str + "');";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    return -1;
  } else {
    return sqlite3_last_insert_rowid(db);
  }
}

int64_t Database::lemma_write(const Path &path) {
  std::string sql = "INSERT INTO lemma (path) "
                    "VALUES ('" +
                    path.toString() + "');";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  } else {
    return sqlite3_last_insert_rowid(db);
  }
}

int64_t Database::functionhash_write(std::string name, size_t functionhash) {
  std::string sql = "INSERT OR IGNORE INTO functionhash (function, hash) "
                    "VALUES ('" +
                    name + "', '" + std::to_string(functionhash) + "');";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  } else {
    return sqlite3_last_insert_rowid(db);
  }
}

void Database::parent_write(int64_t child, int64_t parent) {
  std::string sql = "INSERT OR IGNORE INTO parent (child_id, parent_id)"
                    "VALUES (" +
                    std::to_string(child) + ", " + std::to_string(parent) +
                    ");";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::constraint_write(int64_t expr, int64_t summary) {
  std::string sql = "INSERT OR IGNORE INTO constr (expr_id, summary_id)"
                    "VALUES (" +
                    std::to_string(expr) + ", " + std::to_string(summary) +
                    ");";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::pob_write(ProofObligation *pob) {
  
  std::string sql;
  std::string values;
  std::string pob_id = std::to_string(pob->id);

  std::string parent_id;
  if (pob->isOriginPob()) {
    parent_id = "NULL, ";
  } else {
    parent_id = std::to_string(pob->parent->id) + ", ";
    sql = "INSERT OR REPLACE INTO pobsChildren (pob_id, child_id) "
                  "VALUES (" + parent_id  + pob_id + ");";
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
      exit(1);
    }
  }

  values = pob_id + ", " + std::to_string(pob->root->id) + ", " + parent_id;
  values += "'" + pob->location->toStringLocation() + "', ";
  values += "'" + pob->path.toString() + "'";

  sql = "INSERT OR REPLACE INTO pobs "
                    "(id, root, parent, location, path)"
                    "VALUES (" + values + ")";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }

  pobStack_write(pob);
  pobPropCount_write(pob);
}

void Database::pobStack_write(const ProofObligation *pob) {
  size_t index = 0;
  for (auto instr : pob->stack) {
    std::string str_instr = instr->parent->parent->function->getName().str();
    str_instr += " ";
    auto instrNum = instr->dest;
    str_instr += std::to_string(instrNum);
    std::string sql = "INSERT INTO pobsStack (pob_id, numOfInstr, instr) "
                      "VALUES (" + std::to_string(pob->id) + ", " 
                     + std::to_string(index) + ", " 
                     + "'" + str_instr + "'" 
                     + ")";
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
      exit(1);
    }
    index++;
  }
}

void Database::pobPropCount_write(const ProofObligation *pob) {
  for (auto item : pob->propagationCount) {
    auto state = item.first;
    auto count = item.second;
    std::string values = std::to_string(pob->id) + ", "
                       + std::to_string(state->getID()) + ", "
                       + std::to_string(count);
    std::string sql = "INSERT INTO propagationCount (pob_id, state_id, count)"
                      "VALUES (" + values + ")";
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
      exit(1);
    }
  }
}

void Database::pobsChildren_write(ProofObligation *pob) {
  
  std::string pob_id = std::to_string(pob->id);
  for (auto child : pob->children) {
    std::string child_id = std::to_string(child->id);
    std::string sql = "INSERT INTO pobsChildren (pob_id, child_id) "
                      "VALUES (" + pob_id + ", " + child_id + ");";
    
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
      exit(1);
    }
  }
  

}

void Database::pobsConstr_write(unsigned pob_id, uint64_t expr_id, std::string instr) {
  std::string sql = "INSERT OR IGNORE INTO pobsConstr (pob_id, expr_id, instr) "
                    "VALUES (" + std::to_string(pob_id) + ", " + 
                    std::to_string(expr_id) + ", " +
                    "'" + instr + "'" +
                    ")";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::maxId_write(std::uint32_t maxIdState, unsigned maxIdPob) {

  std::string sql = "INSERT OR REPLACE INTO maxID (id, maxIdState, maxIdPob) " 
                    "VALUES (" + std::to_string(1) + ", "
                               + std::to_string(maxIdState) + ", "
                               + std::to_string(maxIdPob) + ");";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::state_write(std::string values) {
  std::string sql = "INSERT OR REPLACE INTO states (id, initLoc, currLoc, choiceBranch, solverResult, path, countInstr, isolated, terminated, reached) "
                    "VALUES (" + values + ");";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::target_write(std::uint32_t state_id, std::string target) {
  std::string sql = "INSERT INTO targets (state_id, target) "
                    "VALUES (" + std::to_string(state_id) + ", " + 
                    "'" + target + "'" + ")";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::child_write(std::uint32_t state_id, std::uint32_t child_id, std::string location) {
  std::string sql = "INSERT INTO child (state_id, child_id, location) "
                "VALUES (" + std::to_string(state_id) + ", " +
                             std::to_string(child_id) + ", " +
                             "'" + location + "'" + ")";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::statesConstr_write(uint32_t state_id, uint64_t expr_id, std::string instr) {
  std::string sql = "INSERT OR IGNORE INTO statesConstr (state_id, expr_id, instr) "
                    "VALUES (" + std::to_string(state_id) + ", " + 
                    std::to_string(expr_id) + ", " +
                    "'" + instr + "'" +
                    ")";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

std::vector<std::pair<uint64_t, std::string>> Database::statesConstr_retrieve(std::string state_id) {
  std::vector<std::pair<uint64_t, std::string>> exprs_instr;
  std::string sql = "SELECT expr_id, instr FROM statesConstr WHERE state_id = " + state_id;
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)){
    case SQLITE_ROW: {
      auto expr_id = sqlite3_column_int(st, 0);
      auto instr_str = std::string(
          reinterpret_cast<const char *>(sqlite3_column_text(st, 1))); 
      exprs_instr.push_back(std::make_pair(expr_id, instr_str));
      break;
    }
    case SQLITE_DONE:
      done = true;
      break;
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  
  return exprs_instr;
}

void Database::prop_write(uint32_t state_id, unsigned pob_id) {
  std::string sql = "INSERT INTO propagations (state_id, pob_id) "
                    "VALUES (" + std::to_string(state_id) + ", " 
                               + std::to_string(pob_id) 
                               + ")";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::arraymap_write(int64_t array, int64_t expr) {
  std::string sql = "INSERT OR IGNORE INTO arraymap (array_id, expr_id)"
                    "VALUES (" +
                    std::to_string(array) + ", " + std::to_string(expr) + ");";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

std::string Database::array_retrieve(int64_t id) {
  std::string sql =
      "SELECT arr FROM array WHERE rowid = " + std::to_string(id) + ";";
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  if (sqlite3_step(st) != SQLITE_ROW)
    exit(1);
  std::string arr_string(
      reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return arr_string;
}

std::string Database::expr_retrieve(int64_t id) {
  std::string sql =
      "SELECT expr FROM expr WHERE rowid = " + std::to_string(id) + ";";
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  if (sqlite3_step(st) != SQLITE_ROW)
    exit(1);
  std::string expr_string(
      reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return expr_string;
}

std::map<uint64_t, Database::DBLemma> Database::lemmas_retrieve() {
  std::map<uint64_t, Database::DBLemma> lemmas;
  std::string sql = "SELECT id, path FROM lemma";
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW:
      lemmas.insert(std::make_pair(sqlite3_column_int(st, 0),
                                   DBLemma(sqlite3_column_text(st, 1))));
      break;
    case SQLITE_DONE:
      done = true;
      break;
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  for (auto &lemma : lemmas) {
    sql = "SELECT expr_id FROM constr WHERE summary_id = " +
          std::to_string(lemma.first);
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
      exit(1);
    }
    done = false;
    while (!done) {
      switch (sqlite3_step(st)) {
      case SQLITE_ROW:
        lemma.second.exprs.push_back(sqlite3_column_int(st, 0));
        break;
      case SQLITE_DONE:
        done = true;
        break;
      }
    }
    if (sqlite3_finalize(st) != SQLITE_OK)
      exit(1);
  }
  return lemmas;
}

std::map<std::string, size_t> Database::functionhash_retrieve() {
  std::map<std::string, size_t> hashes;
  sqlite3_stmt *st;
  std::string sql = "SELECT function, hash FROM functionhash";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW: {
      std::string name(reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));
      std::string hash_str(reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
      std::istringstream iss(hash_str);
      size_t hash;
      iss >> hash;
      hashes.insert(
          std::make_pair(name, hash));
      break;
    }
    case SQLITE_DONE: {
      done = true;
      break;
    }
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return hashes;
}

std::set<std::pair<uint64_t, uint64_t>> Database::parents_retrieve() {
  std::set<std::pair<uint64_t, uint64_t>> parents;
  sqlite3_stmt *st;
  std::string sql = "SELECT child_id, parent_id FROM parent";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW:
      parents.insert(
          std::make_pair(sqlite3_column_int(st, 0), sqlite3_column_int(st, 1)));
      break;
    case SQLITE_DONE:
      done = true;
      break;
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return parents;
}


// Pobs Children
std::set<std::pair<uint64_t, uint64_t>> Database::pobsChildren_retrieve() {
  std::set<std::pair<uint64_t, uint64_t>> children;
  sqlite3_stmt *st;
  std::string sql = "SELECT pob_id, child_id FROM pobsChildren ";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW:
      children.insert(
          std::make_pair(sqlite3_column_int(st, 0), sqlite3_column_int(st, 1)));
      break;
    case SQLITE_DONE:
      done = true;
      break;
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return children;
}

std::map<uint64_t, std::string> Database::arrays_retrieve() {
  std::map<uint64_t, std::string> arrays;
  sqlite3_stmt *st;
  std::string sql = "SELECT id, array FROM array";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW: {
      std::string array_str = std::string(
          reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
      arrays.insert(std::make_pair(sqlite3_column_int(st, 0), array_str));
      break;
    }
    case SQLITE_DONE: {
      done = true;
      break;
    }
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return arrays;
}

std::map<uint64_t, std::string> Database::exprs_retrieve() {
  std::map<uint64_t, std::string> exprs;
  sqlite3_stmt *st;
  std::string sql = "SELECT id, expr FROM expr";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW: {
      std::string expr_str = std::string(
          reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
      exprs.insert(std::make_pair(sqlite3_column_int(st, 0), expr_str));
      break;
    }
    case SQLITE_DONE: {
      done = true;
      break;
    }
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return exprs;
}

std::pair<std::uint32_t, unsigned> Database::maxId_retrieve() {
  std::pair<std::uint32_t, unsigned> result;

  sqlite3_stmt *st;
  std::string sql = "SELECT maxIDState, maxIDPob FROM maxID";
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW: {
      result = std::make_pair(sqlite3_column_int(st, 0), sqlite3_column_int(st, 1));
      break;
    }
    case SQLITE_DONE: {
      done = true;
      break;
    }
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);

  return result;
}

std::map<unsigned, Database::DBPob> Database::pobs_retrieve() {
  std::map<unsigned, Database::DBPob> result;
  std::string sql = "SELECT id, root, parent, location, path FROM pobs";
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW: {
      // auto root_id = sqlite3_column_int(st, 1);
      DBPob pob;
      pob.root_id = sqlite3_column_int(st, 1);
      pob.parent_id = sqlite3_column_int(st, 2);
      // pob.location = *sqlite3_column_text(st, 3);
      pob.location = std::string(
          reinterpret_cast<const char *>(sqlite3_column_text(st, 3)));
      // pob.path = *sqlite3_column_text(st, 4);
      pob.path = std::string(
          reinterpret_cast<const char *>(sqlite3_column_text(st, 4)));
      result.insert(std::make_pair(sqlite3_column_int(st, 0), pob));
      break;
    }
    case SQLITE_DONE: {
      done = true;
      break;
    }
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);

  for (auto &pob : result) {
    // auto exprs_instr = pobConstr_retrieve(std::to_string(pob.first));
    // pob.second.expr_instr = exprs_instr;
     pob.second.expr_instr = pobConstr_retrieve(std::to_string(pob.first));

    auto children_id = pobChildren_retrieve(std::to_string(pob.first));
    for (auto id : children_id) {
      pob.second.children.push_back(id);
    }

    auto stack = pobStack_retrieve(std::to_string(pob.first));
    for (auto instr : stack) {
      pob.second.stack.insert(instr);
    }
  }
  return result;
}

std::vector<std::pair<uint64_t, std::string>> Database::pobConstr_retrieve(std::string pob_id) {
  std::vector<std::pair<uint64_t, std::string>> exprs_instr;
  std::string sql = "SELECT expr_id, instr FROM pobsConstr WHERE pob_id = " + pob_id;
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)){
    case SQLITE_ROW: {
      auto expr_id = sqlite3_column_int(st, 0);
      auto instr_str = std::string(
          reinterpret_cast<const char *>(sqlite3_column_text(st, 1))); 
      exprs_instr.push_back(std::make_pair(expr_id, instr_str));
      break;
    }
    case SQLITE_DONE:
      done = true;
      break;
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);

  return exprs_instr;
}

std::map<uint64_t, std::map<std::uint32_t, unsigned>> Database::propsCount_retrieve() {
  std::map<uint64_t, std::map<std::uint32_t, unsigned>> result;
  std::string sql = "SELECT pob_id, state_id, count FROM propagationCount;";
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)){
    case SQLITE_ROW: {
      auto pob_id = sqlite3_column_int(st, 0);
      auto state_id = sqlite3_column_int(st, 1);
      auto count = sqlite3_column_int(st, 2);

      result[pob_id].insert(std::make_pair(state_id, count));
      break;
    }
    case SQLITE_DONE:
      done = true;
      break;
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);

  return result;
}

std::vector<unsigned> Database::pobChildren_retrieve(std::string pob_id) {
  std::vector<unsigned> childre_id;
  std::string sql = "SELECT child_id FROM pobsChildren WHERE pob_id = " + pob_id;
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)){
    case SQLITE_ROW:
      childre_id.push_back(sqlite3_column_int(st, 0));
      break;
    case SQLITE_DONE:
      done = true;
      break;
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return childre_id;
}

std::map<int64_t, std::string> Database::pobStack_retrieve(std::string pob_id) {
  std::map<int64_t, std::string> stack;
  std::string sql = "SELECT numOfInstr, instr FROM pobsStack WHERE pob_id = " + pob_id;
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)){
    case SQLITE_ROW: {
      std::string instr_str = std::string(
          reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
      stack.insert(std::make_pair(sqlite3_column_int(st, 0), instr_str));
      break;
    }
    case SQLITE_DONE:
      done = true;
      break;
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);
  return stack;
}

std::map<uint32_t, Database::DBState> Database::states_retrieve() {
  std::map<uint32_t, Database::DBState> result;
  std::string sql = "SELECT id, initLoc, currLoc, " 
                    "choiceBranch, solverResult, path, "
                    "countInstr, isolated, terminated, reached "
                    "FROM states";
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW: {
      DBState state;
      state.initLoc = std::string(
            reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
      state.currLoc = std::string(
            reinterpret_cast<const char *>(sqlite3_column_text(st, 2)));
      state.choiceBranch = std::string(
            reinterpret_cast<const char *>(sqlite3_column_text(st, 3)));
      state.solverResult = std::string(
            reinterpret_cast<const char *>(sqlite3_column_text(st, 4)));
      state.path = std::string(
            reinterpret_cast<const char *>(sqlite3_column_text(st, 5)));
      state.countInstr = sqlite3_column_int(st, 6);
      state.isolated = sqlite3_column_int(st, 7);
      state.terminated = sqlite3_column_int(st, 8);
      state.reached = sqlite3_column_int(st, 9);
      
      result.insert(std::make_pair(sqlite3_column_int(st, 0),state));
      break;
    }
    case SQLITE_DONE: {
      done = true;
      break;
    }
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);

  for (auto &state : result) {
    state.second.expr_instr = statesConstr_retrieve(std::to_string(state.first));
  }
  //pob.second.expr_instr = pobConstr_retrieve(std::to_string(pob.first));
  return result;
}

std::vector<std::string> Database::targets_retrieve(std::string state_id) {
  std::vector<std::string> result;
  std::string sql = "SELECT target FROM targets WHERE state_id = " + state_id;
  sqlite3_stmt *st;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
    exit(1);
  }
  bool done = false;
  while (!done) {
    switch (sqlite3_step(st)) {
    case SQLITE_ROW: {
      std::string target = std::string(
            reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));
      result.push_back(target);
      break;
    }
    case SQLITE_DONE: {
      done = true;
      break;
    }
    }
  }
  if (sqlite3_finalize(st) != SQLITE_OK)
    exit(1);

  return result;
}

void Database::lemma_delete(uint64_t id) {
  std::string sql = "DELETE FROM lemma WHERE id = " + std::to_string(id);
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::hash_delete(std::string name) {
  std::string sql = "DELETE FROM functionhash WHERE function = '" + name + "'";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::state_delete(std::uint32_t state_id) {

  std::string sql = "DELETE FROM propagationCount WHERE state_id = " + std::to_string(state_id);
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }

  sql = "DELETE FROM statesConstr WHERE state_id = " + std::to_string(state_id);
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }

  sql = "DELETE FROM propagations WHERE state_id = " + std::to_string(state_id);
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }

  sql = "DELETE FROM targets WHERE state_id = " + std::to_string(state_id);
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }

  sql = "DELETE FROM child WHERE state_id = " + std::to_string(state_id);
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }


  sql = "DELETE FROM states WHERE id = " + std::to_string(state_id);
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::exprs_purge() {
  std::string sql = "DELETE from expr where NOT EXISTS (SELECT expr_id FROM "
                    "constr WHERE expr.id = expr_id)";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

void Database::arrays_purge() {
  std::string sql = "DELETE from array where NOT EXISTS (SELECT array_id FROM "
                    "arraymap WHERE array.id = array_id)";
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
    exit(1);
  }
}

} // namespace klee
