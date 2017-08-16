/**
* Copyright (c) 2017-present, Facebook, Inc.
* All rights reserved.
*
* This source code is licensed under the BSD-style license found in the
* LICENSE file in the root directory of this source tree. An additional grant
* of patent rights can be found in the PATENTS file in the same directory.
*/

//File: atari_game.h

#pragma once

#include <vector>
#include <string>

#include "../elf/pybind_helper.h"
#include "../elf/comm_template.h"
#include "../elf/ai_comm.h"
#include "codegen_specific.h"
#include "interpreter.h"

using Context = ContextT<GameOptions, HistT<GameState>>;
using Comm = typename Context::Comm;
using AIComm = AICommT<Comm>;

class CodegenGame {
  private:
    int _game_idx = -1;
    AIComm *_ai_comm;

    // The number of memory samples to draw.
    int num_memory_sample = 10;

    // The size of memory.
    int size_memory = 10;

    // The number of register to use.
    int num_reg = 4;

    int instruction_dim = 3;

    // Initial program length.
    int initial_program_length = 20;

    // Return the program whose length is >= min_program_length_after_pruning.
    int min_program_length_after_pruning = 5;
    int max_program_length_after_pruning = 10;

    // Each memory slot will be in [0, max_mem_item]
    int mem_upper_bound = 10;

    // How many samples to draw before we proceed to the next program.
    int redraw_after_n = 3;

    // How many instructions between two memories.
    int instruction_span = 2;

    // Percent of memory sample picked.
    // If num_memory_sample == 1, then the percent is always 100%.
    int percent_mem_picked = 50;

    bool use_random_span = false;
    bool use_first_memory = false;
    bool _eval_only = false;
    bool _use_loop = true;

    Machine machine;
    std::vector<int> init_reg;
    std::vector<std::vector<int> > memories;
    std::vector<Instruction> code_pruned;
    std::vector< std::vector<MachineState> > all_states;
    std::vector< std::map<int, std::vector<int> > > pc2states;
    std::mt19937 gen;

  public:
    CodegenGame(const GameOptions& options) : machine(num_reg, size_memory), init_reg(num_reg), gen(std::random_device{}()) {
        _eval_only = options.eval_only;
        _use_loop = options.use_loop;
        std::fill(init_reg.begin(), init_reg.end(), 0);
        init_reg[0] = size_memory;
    }

    void initialize_comm(int game_idx, AIComm* ai_comm) {
      assert(!_ai_comm);
      _ai_comm = ai_comm;
      _game_idx = game_idx;
    }
    void get_pruned_code(bool loop);
    int get_half_feature(int sample_idx, int state_idx, std::vector<int>* f, int offset);
    void regenerate_memory();
    void reload_code();

    int GetEnsembleMachineStateDim();
    void GenerateProgram(GameState& state, const char* program);
    void MainLoop(const std::atomic_bool& done);
};
