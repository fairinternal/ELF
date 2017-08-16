#ifndef _INTERPRETER_INTERFACE_H_
#define _INTERPRETER_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int num_reg;

  int size_memory;
  int num_memory_sample;
  int percent_mem_picked;
  int mem_upper_bound;

  int initial_program_length;
  int min_program_length_after_pruning;
  int max_program_length_after_pruning;

  int redraw_after_n;
  int instruction_span;

  bool use_random_span;
  // Always use the first memory for any instruction.
  bool use_first_memory;
} InterpreterParams;

void InitInterpreterParams(InterpreterParams *params);
void *InitInterpreter(const InterpreterParams *);
void destroyInterpreter(void *handle);

void PrintParams(void *handle);

void GetInitMemory(void *handle, int i, int *);
void GetInitFinalFeature(void *handle, int ins_idx, int *f);

int GetNumMemorySample(void *handle);
int GetMemoryUpperBound(void *handle);
int GetMemoryDim(void *handle);
int GetEnsembleMachineStateDim(void *handle);
int GetInstructionDim(void *handle);
int GetNumRegs(void *handle);
int GetNumInstructionType(void *handle);
int GetMaxProgramLength(void *handle);

const char *GetInstructionString(void *handle, int, int, int);

// Return integer format.
void GetInstructionFeature(void *handle, int pc, int *f);
int GetNumInstructions(void *handle);

// Return all possible instructions for line_idx / num_lines.
// Each instruction is represented by integer format; All instructions are separated by semi-column.
const char *EnumerateInstructions(void *handle, int line_idx, int num_lines);

const char *GetCurrentProgram(void *handle);

// Generate input/output memory pairs (as input of the network) and target. The span is specified by InterpreterParams.
// The target is represented by integer format (0 2 -1 means jmp 2, etc), obtained with instruction.GetFeature()
void GenerateFeature(void *handle, int *f, int *target);

void GenerateProgram(void *handle, int *in, int *out, int *code, const char *program, bool regen);

void GenerateProgram3(void *handle, int *mi1, int *mo1, int *mi2, int *mo2, int *mi3, int *mo3, int *m4, int *code);

void GenerateProgram4(void *handle, int *i1, int* o1, int *m1, int* m2, int* tmp, int *code);

// Generate input/output memory pairs and their integer span (in the code, not in the unrolled program).
void GenerateFeaturePredictSpan(void *handle, int *f, int *target);

// Load code from program string delim'ed by semi-colon(;). Regen indictes if memort is regenerated.
int LoadCodeFromString(void *handle, const char *s, bool regen, bool oo);

// Load code from a file.
int LoadCode(void *handle, const char *filename);

#ifdef __cplusplus
}
#endif

#endif
