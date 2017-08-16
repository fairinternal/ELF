/**
* Copyright (c) 2017-present, Facebook, Inc.
* All rights reserved.
*
* This source code is licensed under the BSD-style license found in the
* LICENSE file in the root directory of this source tree. An additional grant
* of patent rights can be found in the PATENTS file in the same directory.
*/

//File: game_context.h
//Author: Yuxin Wu <ppwwyyxxc@gmail.com>

#pragma once
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <pybind11/stl.h>

#include "codegen.h"
#include "../elf/pybind_interface.h"

class GameContext {
  public:
    using GC = Context;

  private:
    std::unique_ptr<GC> _context;
    std::vector<CodegenGame> games;

    int _num_action = 1;

  public:
    GameContext(const ContextOptions& context_options, const GameOptions& options) {
      _context.reset(new GC{context_options, options});
      for (int i = 0; i < context_options.num_games; ++i) {
        games.emplace_back(options);
      }
    }

    void Start() {
        auto f = [this](int game_idx, const GameOptions&,
                const std::atomic_bool& done, GC::AIComm* ai_comm) {
            auto& game = games[game_idx];
            game.initialize_comm(game_idx, ai_comm);
            game.MainLoop(done);
        };
        auto init = [this](int id, HistT<GameState> &state) {
            state.InitHist(1);
            for (auto &s : state.v()) {
                s.Init(id, _num_action);
            }
        };
        _context->Start(init, f);
    }

    std::map<std::string, int> GetParams() const {
        return std::map<std::string, int>{
          { "num_action", _num_action },
        };
    }

    EntryInfo EntryFunc(const std::string &key) {
        auto *mm = GameState::get_mm(key);
        if (mm == nullptr) return EntryInfo();

        std::string type_name = mm->type();

        if (key == "mem_in" || key == "mem_out") return EntryInfo(key, type_name, {10, 16});
        if (key == "code") return EntryInfo(key, type_name, {10, 3});
        else if (key == "last_r" || key == "last_terminal" || key == "id" || key == "seq" || key == "game_counter") return EntryInfo(key, type_name);
        else if (key == "pi") return EntryInfo(key, type_name, {_num_action});
        else if (key == "a" || key == "rv" || key == "V") return EntryInfo(key, type_name);

        return EntryInfo();
    }

    CONTEXT_CALLS(GC, _context);

    void Stop() {
      _context.reset(nullptr); // first stop the threads, then destroy the games
    }
};
