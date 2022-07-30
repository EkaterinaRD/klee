#include "SeedMap.h"

using namespace klee;

void SeedMap::update(ExecutionState *current,
                        const std::vector<ExecutionState *> &addedStates,
                        const std::vector<ExecutionState *> &removedStates) {
  for (auto it : removedStates) {
      ExecutionState *es = it;
      std::map<ExecutionState*, std::vector<SeedInfo> >::iterator it2 = 
      seedMap.find(es);
      if (it2 != seedMap.end())
        seedMap.erase(it2);
    }
}

bool SeedMap::empty() {
  return seedMap.empty();
}

std::map<ExecutionState*, std::vector<SeedInfo> >::iterator SeedMap::upper_bound(ExecutionState *state) {
  return seedMap.upper_bound(state);
}

std::map<ExecutionState*, std::vector<SeedInfo> >::iterator SeedMap::find(ExecutionState *state) {
  return seedMap.find(state);
}

std::map<ExecutionState*, std::vector<SeedInfo> >::iterator SeedMap::begin() {
  return seedMap.begin();
}

std::map<ExecutionState*, std::vector<SeedInfo> >::iterator SeedMap::end() {
  return seedMap.end();
} 

void SeedMap::erase(std::map<ExecutionState*, std::vector<SeedInfo> >::iterator it) {
  seedMap.erase(it);
}

void SeedMap::erase(ExecutionState *state) {
  seedMap.erase(state);
}

void SeedMap::push_back(ExecutionState *result, std::vector<SeedInfo>::iterator siit) {
  seedMap.at(result).push_back(*siit);
}

std::vector<SeedInfo> &SeedMap::at(ExecutionState *state) {
  return seedMap[state];
}

std::size_t SeedMap::count(ExecutionState *state) {
  return seedMap.count(state);
}