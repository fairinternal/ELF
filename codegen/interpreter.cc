#include "interpreter.h"
#include <iostream>
#include <utility>
#include <sstream>
#include <vector>
#include <map>

const std::vector<std::string> Instruction::kInsTypename = {
  "end", "rep", "inc", "dec", "mov", "cmp", "load", "save"
};

const std::map<std::string, Instruction::InsType> Instruction::kInsMap = {
  { "rep", Instruction::INS_REP }, { "end", Instruction::INS_END },
  { "inc", Instruction::INS_INC }, { "dec", Instruction::INS_DEC }, { "mov", Instruction::INS_MOV }, { "cmp", Instruction::INS_CMP },
  { "load", Instruction::INS_LOAD }, { "save", Instruction::INS_SAVE }
};

/*
static const std::map<std::string, std::function<void ()>> g_command_map = {
  { "jmp" : [](const std::vector<std::string> &tokens) {
      const int line_number = std::stoi(tokens[1]);
      return [=, &this]() { this->pc() = line_number - 1; return RES_RUNNING; };
    },
    "je"

};
*/

MachineState::MachineState(int reg_size, int memory_size)
  : _register(reg_size), _memory(memory_size) {
    Reset();
}

void MachineState::Reset() {
  _pc = 0;
  _flag = 0;
  _return_code = RES_RUNNING;
  _counter = 0;
  fill(_memory.begin(), _memory.end(), 0);
  fill(_register.begin(), _register.end(), 0);
}

void MachineState::Init() {
  Init(Register(), Memory());
}

void MachineState::Init(const Register &reg, const Memory &mem) {
  Reset();
  for (int i = 0; i < std::min(reg.size(), this->reg().size()); ++i) _register[i] = reg[i];
  for (int i = 0; i < std::min(mem.size(), this->mem().size()); ++i) _memory[i] = mem[i];
}

void MachineState::InitRandomMemory(const Register &reg, std::function<int ()> f) {
  Reset();
  for (int i = 0; i < std::min(reg.size(), this->reg().size()); ++i) _register[i] = reg[i];
  for (int i = 0; i < this->mem().size(); ++i) _memory[i] = f();
}

std::vector<int> MachineState::GetFeature() const {
  // register size + memory size + pc + flag
  std::vector<int> f(_register.size() + _memory.size() + 2);
  int offset = 0;
  std::copy(_memory.begin(), _memory.end(), f.begin() + offset);
  offset += _memory.size();
  std::copy(_register.begin(), _register.end(), f.begin() + offset);
  offset += _register.size();
  f[offset ++] = _pc;
  f[offset ++] = _flag;

  return f;
}

// Type of code:
// jmp, je, jg, jl, mov, inc, dec, cmp, load, save
//
// jmp/je/jg/jl line_diff.
//    jmp -1 => go to the previous line,
//    jmp 1 => go to the next line.
// mov reg_dst reg_src
// inc reg
// dec reg
// cmp reg1 reg2 (compare and set the flag)
// load reg_dst reg_addr (load data from [reg_addr] to reg_dst)
// save reg_src reg_addr (save data to [reg_addr] from reg_src)
//
// For sampling, here is the #number of possible instructions (only depends on #registers)
// jmp/je/jg/jl: #len * 4
// mov           #reg * (#reg - 1)
// inc/dec:      #reg * 2
// cmp:          #reg * (#reg - 1) / 2
// load/save:    2 * #reg * #reg
// total: k = 4 #len + 3.5 * #reg * #reg + 0.5 * #reg
// Total space: k^#len
//   If #len == 20, #reg = 5, then k = 80 + 90 = 170. 170^20 = 4.064e44
static int check_oob(int addr, int limit) {
  if (addr < 0 || addr >= limit) throw std::runtime_error("register static address out of bound!");
  return addr;
}

std::function<void ()> Machine::parse_one_line(int line_idx, const Instruction &s) {
  // Run the current line pointed by _pc
  // std::cout << s.Print() << std::endl;
  // Check the code.
  //
  const int num_reg = _state.reg().size();
  if (s.ins_type == Instruction::INS_REP) {
    const int dst = check_oob(s.arg1, num_reg);
    // rep loop
    return [=]() {
      if (this->state().reg(dst) > 0) {
        this->state().reg(dst) -= 1;
      } else {
        this->state().pc() = _control_map[line_idx];
      }
    };
  } else if (s.ins_type == Instruction::INS_END) {
    return [=]() {
      auto it = _control_map.find(line_idx);
      if (it != _control_map.end()){
        //rep loop, go back
        this->state().pc() = it->second - 1;
      }
    };
  } else if (s.ins_type == Instruction::INS_MOV) {
    const int dst = check_oob(s.arg1, num_reg);
    const int src = check_oob(s.arg2, num_reg);
    return [=]() {
      this->state().reg(dst) = this->state().reg(src);
      // std::cout << "finish mov << " << dst << " " << src << std::endl;
    };
  } else if (s.ins_type == Instruction::INS_INC || s.ins_type == Instruction::INS_DEC) {
    const int dst = check_oob(s.arg1, num_reg);
    const int increment = s.ins_type == Instruction::INS_INC ? 1 : -1;
    return [=]() {
      // std::cout << "add << " << dst << " " << increment << std::endl;
      this->state().reg(dst) += increment;
    };
  } else if (s.ins_type == Instruction::INS_CMP) {
    const int addr1 = check_oob(s.arg1, num_reg);
    const int addr2 = check_oob(s.arg2, num_reg);
    return [=]() {
      const int v1 = this->state().reg(addr1);
      const int v2 = this->state().reg(addr2);
      if (v1 >= v2) this->state().pc() = _control_map[line_idx] - 1;
    };
  } else if (s.ins_type == Instruction::INS_LOAD) {
    const int dst = check_oob(s.arg1, num_reg);
    const int addr = check_oob(s.arg2, num_reg);
    return [=]() {
      this->state().reg(dst) = this->state().mem(this->state().reg(addr));
    };
  } else if (s.ins_type == Instruction::INS_SAVE) {
    const int src = check_oob(s.arg1, num_reg);
    const int addr = check_oob(s.arg2, num_reg);
    return [=]() {
      this->state().mem(this->state().reg(addr)) = this->state().reg(src);
    };
  } else {
    std::cout << "Unknown command " << s.ins_type << std::endl;
    throw std::runtime_error("Unknown command");
  }
}

void Machine::Load(const std::vector<Instruction> &codes) {
  _program.clear();
  _control_map.clear();
  std::vector<int> control_pc;
  int control_type = 1;
  for (int i = 0; i < codes.size(); ++i) {
    if (codes[i].ins_type == Instruction::INS_REP){
      control_pc.push_back(i);
      control_type = control_type << 1;
    } else if (codes[i].ins_type == Instruction::INS_CMP){
      control_pc.push_back(i);
      control_type = (control_type << 1) + 1;
    } else if (codes[i].ins_type == Instruction::INS_END){
      if (control_pc.size() == 0) {
        continue;
      }
      int current_pc = control_pc.back();
      control_pc.pop_back();
      if (control_type % 2 == 0) {
        // handle rep jumps
        _control_map[i] = current_pc;
        _control_map[current_pc] = i;
      } else {
        // handle if jumps
        _control_map[current_pc] = i;
      }
      control_type = control_type >> 1;
    }
  }
  int n = codes.size();
  while (control_pc.size() > 0) {
    int current_pc = control_pc.back();
    control_pc.pop_back();
    if (control_type % 2 == 0) {
      // handle rep jumps
      _control_map[n] = current_pc;
      _control_map[current_pc] = n;
    } else {
      // handle if jumps
      _control_map[current_pc] = n;
    }
    n++;
    control_type = control_type >> 1;
  }
  for (int i = 0; i < codes.size(); ++i) {
    _program.push_back(parse_one_line(i, codes[i]));
  }
  _code = codes;
  for (int i = codes.size(); i < n; ++i) {
    _code.push_back(Instruction(Instruction::INS_END));
    _program.push_back(parse_one_line(i, Instruction(Instruction::INS_END)));
  }

}

void Machine::Parse(const std::vector<std::string> &codes) {
  std::vector<Instruction> instructions;
  for (int i = 0; i < codes.size(); ++i) {
    instructions.push_back(Instruction(codes[i]));
  }
  Load(instructions);
}

int Machine::Run() {
  if (_dump_all_state) {
    _pc2states.clear();
    _dumped_states.clear();
  }
  // Run the program.
  while (_state.pc() >= 0 && _state.pc() < _program.size() && _state.counter() < _max_code_run) {
    // std::cout << "Run instruction: " << _code[_state.pc()].Print() << std::endl;

    try {
      if (_dump_all_state) {
        _dumped_states.push_back(_state);
        _pc2states[_state.pc()].push_back(_dumped_states.size() - 1);
      }
      _program[_state.pc()]();
      _state.pc() ++;
    } catch (const std::runtime_error& e) {
      // std::cout << e.what() << std::endl;
      _state.return_code() = RES_MEM_OOB;
      break;
    }

    // std::cout << Dump() << std::endl;
    _state.counter() ++;
  }
  if (_state.return_code() == RES_RUNNING) {
    if (_state.counter() == _max_code_run) _state.return_code() = RES_MAX_CODE_RUN_REACHED;
    else {
      // It terminates normally.
      _state.return_code() = RES_END;
      if (_dump_all_state) {
        _dumped_states.push_back(_state);
        _pc2states[_state.pc()].push_back(_dumped_states.size() - 1);
      }
    }
  }
  return _state.return_code();
}

std::string PrintInstructions(const std::vector<Instruction> &instructions) {
  std::stringstream ss;
  for (int i = 0; i < instructions.size(); ++i) {
    ss << instructions[i].Print() << std::endl;
  }
  return ss.str();
}

bool ApplyToMemories(Machine *machine, const Register &init_reg, const Memories &mem_before, Memories *mem_after) {
  if (mem_before.empty()) {
    mem_after->clear();
    return true;
  }

  mem_after->resize(mem_before.size());
  for (int j = 0; j < mem_before.size(); j++) {
    machine->state().Init(init_reg, mem_before[j]);
    if (machine->Run() != RES_END) return false;
    (*mem_after)[j] = machine->state().mem();
  }

  return true;
}

bool GreedyPrune(const std::vector<Instruction> &instructions, const std::vector<int>& init_reg, int size_memory, int num_sample, std::vector<Instruction> *pruned) {
  Machine machine(init_reg.size(), size_memory);
  machine.Load(instructions);

  Memories mem_before(num_sample), mem_after(num_sample);

  for (int i = 0; i < num_sample; ++i) {
    machine.state().InitRandomMemory(init_reg, []() { return std::rand() % 20; });
    mem_before[i] = machine.state().mem();
    // std::cout << machine.Dump() << std::endl;
    int return_code = machine.Run();
    if (return_code != RES_END) return false;
    mem_after[i] = machine.state().mem();
    // std::cout << machine.Dump() << std::endl;
  }

  // Then let's do a greedy prune so that the output memory remains the same.
  std::vector<Instruction> ins = instructions;
  bool all_prune_failed = false;

  while (! all_prune_failed) {
    all_prune_failed = true;
    for (int i = 0; i < ins.size(); ++i) {
      std::vector<Instruction> new_instructions = ins;
      // Prune the i-th element, note that all jmp operations needs to be recomputed.
      new_instructions.erase(new_instructions.begin() + i);
      // std::cout << "Candidate pruned: " << std::endl;
      // std::cout << PrintInstructions(new_instructions) << std::endl;
      machine.Load(new_instructions);

      bool mem_altered = false;
      for (int j = 0; j < num_sample; j++) {
        machine.state().Init(init_reg, mem_before[j]);
        int return_code = machine.Run();
        if (return_code != RES_END || machine.state().mem() != mem_after[j]) {
          /*
          std::cout << "return_code = " << return_code << std::endl;
          std::cout << "Mem before: ";
          for (int k = 0; k < mem_before[j].size(); ++k) std::cout << mem_before[j][k] << " ";
          std::cout << std::endl;

          std::cout << "Mem expected after: ";
          for (int k = 0; k < mem_after[j].size(); ++k) std::cout << mem_after[j][k] << " ";
          std::cout << std::endl;

          std::cout << "Mem after: ";
          for (int k = 0; k < machine.state().mem().size(); ++k) std::cout << machine.state().mem(k) << " ";
          std::cout << std::endl;
          */

          mem_altered = true;
          break;
        }
      }
      if (! mem_altered) {
        // valid prune.
        ins = new_instructions;
        all_prune_failed = false;
        break;
      }
    }
  }
  *pruned = ins;
  return true;
}

std::vector<Instruction> Machine::SampleCode(std::mt19937& gen, int num_reg, int num_lines, bool loop = true) {
  std::uniform_int_distribution<> dis_reg(0, num_reg - 1);
  std::uniform_int_distribution<> dis2(0, Instruction::INS_TOTAL - 1);
  std::uniform_int_distribution<> dis_line(0, num_lines - 1);

  std::vector<Instruction> code;
  Instruction ins;

  // jmp/je/jg/jl: #len * 4
  // mov           #reg * (#reg - 1)
  // inc/dec:      #reg * 2
  // cmp:          #reg * (#reg - 1) / 2
  // load/save:    2 * #reg * #reg
  while (code.size() < num_lines) {
    if (loop) {ins.ins_type = (Instruction::InsType)dis2(gen);} else {
      do {
        ins.ins_type = (Instruction::InsType)dis2(gen);
      } while (ins.ins_type == Instruction::INS_END || ins.ins_type == Instruction::INS_REP || ins.ins_type == Instruction::INS_CMP);
    }
    if (ins.ins_type <= Instruction::INS_BOUND_J) {
      ins.arg1 = -1;
      ins.arg2 = -1;
    } else if (ins.ins_type <= Instruction::INS_BOUND_UNARY) {
      ins.arg1 = dis_reg(gen);
      ins.arg2 = -1;
    } else {
      do {
        ins.arg1 = dis_reg(gen);
        ins.arg2 = dis_reg(gen);
      } while (ins.arg1 == ins.arg2);
    }

    // Repeat the same instruction for mov/cmp/save/load will not change the state.
    if (ins.ins_type == Instruction::INS_MOV || ins.ins_type == Instruction::INS_CMP
        || ins.ins_type == Instruction::INS_SAVE || ins.ins_type == Instruction::INS_LOAD) {
      if (! code.empty() && code.back() == ins) continue;
    }

    code.push_back(ins);
  }
  return code;
}

std::vector<Instruction> Machine::EnumerateInstructions(int line_idx, int num_reg, int num_lines) {
  std::vector<Instruction> instructions;

  for (int i = 0; i < Instruction::INS_TOTAL; ++i) {
    auto t = (Instruction::InsType)i;
    if (i <= Instruction::INS_BOUND_J) {
      instructions.push_back(Instruction(t));
    } else if (i <= Instruction::INS_BOUND_UNARY) {
      for (int j = 0; j < num_reg; ++j) {
        instructions.push_back(Instruction(t, j));
      }
    } else {
      for (int j = 0; j < num_reg; ++j) {
        for (int k = 0; k < num_reg; ++k) {
          if (j != k) instructions.push_back(Instruction(t, j, k));
        }
      }
    }
  }

  return instructions;
}

std::string MachineState::Dump() const {
  std::stringstream ss;
  ss << "pc: " << _pc << "  flag: " << _flag << " #code_run: " << _counter << " return code: " << _return_code << std::endl;
  ss << "Register: ";
  for (int i = 0; i < _register.size(); ++i) {
    ss << _register[i] << " ";
  }
  ss << std::endl;

  ss << "Memory: ";
  for (int i = 0; i < _memory.size(); ++i) {
    ss << _memory[i] << " ";
  }
  ss << std::endl;

  return ss.str();
}

std::vector<Instruction> Machine::LoadCodeFromString(const std::string &s) {
  std::vector<Instruction> res;
  for (std::string token : split(s, ';')) {
    res.push_back(Instruction(token));
  }
  return res;
}

std::vector<Instruction> Machine::LoadCode(const std::string &filename) {
  // Load filename
  std::ifstream f(filename.c_str());
  char buf[256];
  std::vector<Instruction> res;
  while (! f.eof()) {
    f.getline(buf, 256);
    if (buf[0] != '#' && buf[0] != '\n' && buf[0] != 0) {
      // We read this line.
      res.push_back(Instruction(buf));
    }
  }
  return res;
}
