#include "codegen.h"
#include <mutex>
#include <cmath>
#include <iostream>

void CodegenGame::get_pruned_code(bool loop = true) {
  while (true) {
    std::vector<Instruction> codes = Machine::SampleCode(gen, init_reg.size(), initial_program_length, loop);
    bool is_valid = GreedyPrune(codes, init_reg, size_memory, num_memory_sample, &code_pruned);
    if (is_valid && code_pruned.size() <= max_program_length_after_pruning && code_pruned.size() >= min_program_length_after_pruning) {
      break;
    }
  }
}

int CodegenGame::get_half_feature(int sample_idx, int state_idx, std::vector<int>* f,int offset) {
  if (sample_idx < 0 || sample_idx >= all_states.size()) return 0;
  if (state_idx < 0 || state_idx >= all_states[sample_idx].size()) return 0;

  const MachineState &s = all_states[sample_idx][state_idx];
  std::vector<int> f_ = s.GetFeature();
  for (int i = 0; i < f_.size(); ++i) (*f)[i + offset] = f_[i];

  return f_.size();
}

void CodegenGame::regenerate_memory() {
  std::uniform_int_distribution<> dis_mem(0, mem_upper_bound);

  // Generate memory.
  memories.resize(num_memory_sample);
  for (int i = 0; i < num_memory_sample; ++i) {
    memories[i].resize(size_memory);
    std::for_each(memories[i].begin(), memories[i].end(), [&](int &n) { n = dis_mem(gen); });
  }
}

void CodegenGame::reload_code() {
  Machine &m = machine;

  // Then you get the code and run on the memories.
  m.Load(code_pruned);
  m.SetDumpAll(true);

  all_states.clear();
  pc2states.clear();

  for (int j = 0; j < memories.size(); j++) {
    m.state().Init(init_reg, memories[j]);
    m.Run();

    all_states.push_back(m.all_states());
    pc2states.push_back(m.pc2states());
  }
}

int CodegenGame::GetEnsembleMachineStateDim() {
  return size_memory + num_reg + 2;
}

void CodegenGame::GenerateProgram(GameState& state, const char* program = nullptr) {
  const int num_trial = 100;
  bool retry = true;
  state.mem_in.resize(GetEnsembleMachineStateDim() * num_memory_sample);
  state.mem_out.resize(GetEnsembleMachineStateDim() * num_memory_sample);
  state.code.resize(max_program_length_after_pruning * instruction_dim);
  std::fill(state.mem_in.begin(), state.mem_in.end(), 0);
  std::fill(state.mem_out.begin(), state.mem_out.end(), 0);
  std::fill(state.code.begin(), state.code.end(), 0);

  for (int k = 0; k < num_trial; ++k) {
    // clear all.
    if (program == nullptr) {
      get_pruned_code(_use_loop);
    } else {
      code_pruned = Machine::LoadCodeFromString(std::string(program));
    }
    regenerate_memory();
    reload_code();
    retry = false;
    int mem_in_offset = 0;
    int mem_out_offset = 0;
    for (int i = 0; i < memories.size() && ! retry; i++) {
      int offset_in = get_half_feature(i, 0, &state.mem_in, mem_in_offset);
      int offset_out = get_half_feature(i, all_states[i].size() - 1, &state.mem_out, mem_out_offset);
      if (offset_in == 0 || offset_out == 0) retry = true;

      mem_in_offset += offset_in;
      mem_out_offset += offset_out;
    }

    int code_offset = 0;
    if (! retry) {
      // Also dump the code.
      for (int i = 0; i < code_pruned.size(); ++i) {
        std::vector<int> v = code_pruned[i].GetFeature();
        std::copy(v.begin(), v.end(), state.code.begin() + code_offset);
        code_offset += v.size();
      }
      break;
    }
  }

  if (retry) {
    std::cout << "Failed after " << num_trial << " retries! " << std::endl;
    throw std::runtime_error("");
  }
}

void CodegenGame::MainLoop(const std::atomic_bool& done) {
      while (1) {
          GameState& gs = _ai_comm->Prepare();
          GenerateProgram(gs);
          _ai_comm->SendDataWaitReply();
      }
}
