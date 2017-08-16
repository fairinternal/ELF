/**
* Copyright (c) 2017-present, Facebook, Inc.
* All rights reserved.
*
* This source code is licensed under the BSD-style license found in the
* LICENSE file in the root directory of this source tree. An additional grant
* of patent rights can be found in the PATENTS file in the same directory.
*/

#pragma once

#include "../elf/pybind_helper.h"
#include "../elf/comm_template.h"
#include "../elf/hist.h"
#include "../elf/copier.hh"

struct GameState {
    using State = GameState;
    // Seq information.
    int32_t id = -1;
    int32_t seq = 0;
    int32_t game_counter = 0;
    char last_terminal = 0;

    std::vector<int> mem_in;
    std::vector<int> mem_out;
    std::vector<int> code;
    // Reply
    int64_t a;
    float V;
    std::vector<float> pi;
    int32_t rv;

    void Clear() { a = 0; V = 0.0; fill(pi.begin(), pi.end(), 0.0); rv = 0; }

    void Init(int iid, int num_action) {
        id = iid;
        pi.resize(num_action, 0.0);
    }

    GameState &Prepare(const SeqInfo &seq_info) {
        seq = seq_info.seq;
        game_counter = seq_info.game_counter;
        last_terminal = seq_info.last_terminal;

        Clear();
        return *this;
    }

    std::string PrintInfo() const {
        std::stringstream ss;
        ss << "[id:" << id << "][seq:" << seq << "][game_counter:" << game_counter << "][last_terminal:" << last_terminal << "]";
        return ss.str();
    }

    void Restart() {
        seq = 0;
        game_counter = 0;
        last_terminal = 0;
    }

    DECLARE_FIELD(GameState, id, seq, game_counter, last_terminal, mem_in, mem_out, code, a, V, pi, rv);

    REGISTER_PYBIND_FIELDS(id, seq, game_counter, last_terminal, mem_in, mem_out, code, a, V, pi, rv);
};

struct GameOptions {
    bool eval_only = false;
    bool use_loop = true;
    REGISTER_PYBIND_FIELDS(eval_only, use_loop);
};
