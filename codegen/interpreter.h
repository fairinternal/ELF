#ifndef _INTERPRET_H_
#define _INTERPRET_H_

#include <stdio.h>
#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <functional>
#include <cctype>
#include <locale>
#include <random>
#include <map>
#include <fstream>

// Error code.
#define RES_END                  0
#define RES_RUNNING              1
#define RES_STACKOVERFLOW        2
#define RES_MEM_OOB              3
#define RES_PC_OOB               4
#define RES_MAX_CODE_RUN_REACHED 5

#define FLAG_EQUAL    1
#define FLAG_GREATER  2
#define FLAG_LESS     4

inline std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

inline std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  split(s, delim, elems);
  return elems;
}

inline std::string join(const std::vector<std::string>& v, char delim) {
  std::stringstream ss;
  for(int i = 0; i < v.size(); ++i) {
    if(i != 0) ss << delim;
    ss << v[i];
  }
  return ss.str();
}

// trim from start
inline std::string &ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
  return s;
}

// trim from end
inline std::string &rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
  return s;
}

// trim from both ends
inline std::string &trim(std::string &s) {
  return ltrim(rtrim(s));
}

struct Instruction {
  enum InsType { INS_INVALID = -1, INS_END = 0, INS_BOUND_J = INS_END, INS_REP,
    INS_INC, INS_DEC, INS_BOUND_UNARY = INS_DEC, INS_MOV, INS_CMP, INS_LOAD, INS_SAVE, INS_TOTAL };

  InsType ins_type;
  int arg1, arg2;

  static const std::vector<std::string> kInsTypename;
  static const std::map<std::string, InsType> kInsMap;

  Instruction(InsType t = INS_INVALID, int a1 = -1, int a2 = -1) : ins_type(t), arg1(a1), arg2(a2) { }

  Instruction(const std::string &s) {
    std::string s_copy = s;
    auto tokens = split(trim(s_copy), ' ');
    auto it = kInsMap.find(tokens[0]);
    if (it == kInsMap.end()) throw std::runtime_error("No such instruction" + s);

    ins_type = it->second;
    arg1 = tokens.size() >= 2 ? std::stoi(tokens[1]) : -1;
    arg2 = tokens.size() >= 3 ? std::stoi(tokens[2]) : -1;
  }

  std::string Print() const {
    std::stringstream ss;
    ss << kInsTypename[(int)ins_type] << " " << arg1;
    if (arg2 >= 0) ss << " " << arg2;
    return ss.str();
  }

  std::vector<int> GetFeature() const {
    return std::vector<int> { (int)ins_type, arg1, arg2 };
  }

  friend bool operator==(const Instruction& i1, const Instruction &i2) {
    return i1.ins_type == i2.ins_type && i1.arg1 == i2.arg1 && i1.arg2 == i2.arg2;
  }
  friend bool operator!=(const Instruction& i1, const Instruction &i2) { return ! (i1 == i2); }
};

typedef std::vector<int> Register;
typedef std::vector<int> Memory;
typedef std::vector<Memory> Memories;

class MachineState {
private:
  int _pc;
  int _flag;
  int _counter;
  int _return_code;

  // Memory of the machine.
  Register _register;
  Memory _memory;

public:
  MachineState(int reg_size, int memory_size);
  MachineState() { Reset(); }
  void Reset();

  void Init();
  void Init(const Register &reg, const Memory &mem);
  void InitRandomMemory(const Register &reg, std::function<int ()> f);

  std::string Dump() const;
  std::vector<int> GetFeature() const;

  const Memory &mem() const { return _memory; }
  const Register &reg() const { return _register; }

  const int &mem(int i) const {
    if (i < 0 || i >= _memory.size()) {
      std::stringstream ss;
      ss << "Memory addr out of bound! addr: " << i;
      throw std::runtime_error(ss.str());
    }
    return _memory[i];
  }
  int &mem(int i) {
    if (i < 0 || i >= _memory.size()) {
      std::stringstream ss;
      ss << "Memory addr out of bound! addr: " << i;
      throw std::runtime_error(ss.str());
    }
    return _memory[i];
  }

  const int &reg(int i) const {
    if (i < 0 || i >= _register.size()) {
      std::stringstream ss;
      ss << "Register addr out of bound! addr: " << i;
      throw std::runtime_error(ss.str());
    }
    return _register[i];
  }
  int &reg(int i) {
    if (i < 0 || i >= _register.size()) {
      std::stringstream ss;
      ss << "Register addr out of bound! addr: " << i;
      throw std::runtime_error(ss.str());
    }
    return _register[i];
  }

  const int &flag() const { return _flag; }
  int &flag() { return _flag; }

  const int &pc() const { return _pc; }
  int &pc() { return _pc; }

  int &return_code() { return _return_code; }
  const int &return_code() const { return _return_code; }

  int &counter() { return _counter; }
  const int &counter() const { return _counter; }
};

class Machine {
private:
  MachineState _state;
  bool _dump_all_state;
  int _max_code_run;

  std::vector< std::function<void ()> > _program;
  std::vector< Instruction > _code;
  std::map<int, int> _control_map;

  // Memory footprints.
  std::vector< MachineState > _dumped_states;
  // footprint, hashed. pc -> instruction indices.
  std::map<int, std::vector<int> > _pc2states;

  std::function<void ()> parse_one_line(int line_idx, const Instruction &ins);
public:
  // Reset the states.
  Machine(int reg_size, int memory_size) : _state(reg_size, memory_size), _dump_all_state(false), _max_code_run(1000) { }
  Machine(const Machine &) = delete;
  Machine(Machine &&) = default;

  void SetDumpAll(bool dump_all) { _dump_all_state = dump_all; }
  void SetMaxCodeRun(int max_code_run) { _max_code_run = max_code_run; }

  void Parse(const std::vector<std::string> &program);
  void Load(const std::vector<Instruction> &program);
  int Run();

  static std::vector<Instruction> SampleCode(std::mt19937& gen, int num_reg, int num_lines, bool loop);
  static std::vector<Instruction> LoadCodeFromString(const std::string &s);
  static std::vector<Instruction> LoadCode(const std::string &filename);
  static std::vector<Instruction> EnumerateInstructions(int line_idx, int num_reg, int num_lines);

  std::string Dump() const { return _state.Dump(); }

  const MachineState &state() const { return _state; }
  MachineState &state() { return _state; }

  const std::vector<Instruction> &code() const { return _code; }
  // Return all intermediate states, following temporal order.
  const std::vector<MachineState> &all_states() const { return _dumped_states; }
  const std::map<int, std::vector<int> > &pc2states() const { return _pc2states; }
};

// Greedy prune instructions with random memory samples.
bool ApplyToMemories(Machine *machine, const Register &init_reg, const Memories &mem_before, Memories *mem_after);
bool GreedyPrune(const std::vector<Instruction> &instructions, const std::vector<int>& init_reg, int memory_size, int num_memory_sample, std::vector<Instruction> *pruned);
std::string PrintInstructions(const std::vector<Instruction> &instructions);

#endif
