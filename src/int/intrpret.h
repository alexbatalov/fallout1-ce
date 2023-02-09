#ifndef FALLOUT_INT_INTRPRET_H_
#define FALLOUT_INT_INTRPRET_H_

#include <setjmp.h>

#include <vector>

namespace fallout {

typedef enum Opcode {
    OPCODE_NOOP = 0x8000,
    OPCODE_PUSH = 0x8001,
    OPCODE_ENTER_CRITICAL_SECTION = 0x8002,
    OPCODE_LEAVE_CRITICAL_SECTION = 0x8003,
    OPCODE_JUMP = 0x8004,
    OPCODE_CALL = 0x8005,
    OPCODE_CALL_AT = 0x8006,
    OPCODE_CALL_WHEN = 0x8007,
    OPCODE_CALLSTART = 0x8008,
    OPCODE_EXEC = 0x8009,
    OPCODE_SPAWN = 0x800A,
    OPCODE_FORK = 0x800B,
    OPCODE_A_TO_D = 0x800C,
    OPCODE_D_TO_A = 0x800D,
    OPCODE_EXIT = 0x800E,
    OPCODE_DETACH = 0x800F,
    OPCODE_EXIT_PROGRAM = 0x8010,
    OPCODE_STOP_PROGRAM = 0x8011,
    OPCODE_FETCH_GLOBAL = 0x8012,
    OPCODE_STORE_GLOBAL = 0x8013,
    OPCODE_FETCH_EXTERNAL = 0x8014,
    OPCODE_STORE_EXTERNAL = 0x8015,
    OPCODE_EXPORT_VARIABLE = 0x8016,
    OPCODE_EXPORT_PROCEDURE = 0x8017,
    OPCODE_SWAP = 0x8018,
    OPCODE_SWAPA = 0x8019,
    OPCODE_POP = 0x801A,
    OPCODE_DUP = 0x801B,
    OPCODE_POP_RETURN = 0x801C,
    OPCODE_POP_EXIT = 0x801D,
    OPCODE_POP_ADDRESS = 0x801E,
    OPCODE_POP_FLAGS = 0x801F,
    OPCODE_POP_FLAGS_RETURN = 0x8020,
    OPCODE_POP_FLAGS_EXIT = 0x8021,
    OPCODE_POP_FLAGS_RETURN_EXTERN = 0x8022,
    OPCODE_POP_FLAGS_EXIT_EXTERN = 0x8023,
    OPCODE_POP_FLAGS_RETURN_VAL_EXTERN = 0x8024,
    OPCODE_POP_FLAGS_RETURN_VAL_EXIT = 0x8025,
    OPCODE_POP_FLAGS_RETURN_VAL_EXIT_EXTERN = 0x8026,
    OPCODE_CHECK_PROCEDURE_ARGUMENT_COUNT = 0x8027,
    OPCODE_LOOKUP_PROCEDURE_BY_NAME = 0x8028,
    OPCODE_POP_BASE = 0x8029,
    OPCODE_POP_TO_BASE = 0x802A,
    OPCODE_PUSH_BASE = 0x802B,
    OPCODE_SET_GLOBAL = 0x802C,
    OPCODE_FETCH_PROCEDURE_ADDRESS = 0x802D,
    OPCODE_DUMP = 0x802E,
    OPCODE_IF = 0x802F,
    OPCODE_WHILE = 0x8030,
    OPCODE_STORE = 0x8031,
    OPCODE_FETCH = 0x8032,
    OPCODE_EQUAL = 0x8033,
    OPCODE_NOT_EQUAL = 0x8034,
    OPCODE_LESS_THAN_EQUAL = 0x8035,
    OPCODE_GREATER_THAN_EQUAL = 0x8036,
    OPCODE_LESS_THAN = 0x8037,
    OPCODE_GREATER_THAN = 0x8038,
    OPCODE_ADD = 0x8039,
    OPCODE_SUB = 0x803A,
    OPCODE_MUL = 0x803B,
    OPCODE_DIV = 0x803C,
    OPCODE_MOD = 0x803D,
    OPCODE_AND = 0x803E,
    OPCODE_OR = 0x803F,
    OPCODE_BITWISE_AND = 0x8040,
    OPCODE_BITWISE_OR = 0x8041,
    OPCODE_BITWISE_XOR = 0x8042,
    OPCODE_BITWISE_NOT = 0x8043,
    OPCODE_FLOOR = 0x8044,
    OPCODE_NOT = 0x8045,
    OPCODE_NEGATE = 0x8046,
    OPCODE_WAIT = 0x8047,
    OPCODE_CANCEL = 0x8048,
    OPCODE_CANCEL_ALL = 0x8049,
    OPCODE_START_CRITICAL = 0x804A,
    OPCODE_END_CRITICAL = 0x804B,
} Opcode;

typedef enum ProcedureFlags {
    PROCEDURE_FLAG_TIMED = 0x01,
    PROCEDURE_FLAG_CONDITIONAL = 0x02,
    PROCEDURE_FLAG_IMPORTED = 0x04,
    PROCEDURE_FLAG_EXPORTED = 0x08,
    PROCEDURE_FLAG_CRITICAL = 0x10,
} ProcedureFlags;

typedef enum ProgramFlags {
    PROGRAM_FLAG_EXITED = 0x01,
    PROGRAM_FLAG_0x02 = 0x02,
    PROGRAM_FLAG_0x04 = 0x04,
    PROGRAM_FLAG_STOPPED = 0x08,

    // Program is in waiting state with `checkWaitFunc` set.
    PROGRAM_IS_WAITING = 0x10,
    PROGRAM_FLAG_0x20 = 0x20,
    PROGRAM_FLAG_0x40 = 0x40,
    PROGRAM_FLAG_CRITICAL_SECTION = 0x80,
    PROGRAM_FLAG_0x0100 = 0x0100,
} ProgramFlags;

enum RawValueType {
    RAW_VALUE_TYPE_OPCODE = 0x8000,
    RAW_VALUE_TYPE_INT = 0x4000,
    RAW_VALUE_TYPE_FLOAT = 0x2000,
    RAW_VALUE_TYPE_STATIC_STRING = 0x1000,
    RAW_VALUE_TYPE_DYNAMIC_STRING = 0x0800,
};

#define VALUE_TYPE_MASK 0xF7FF

#define VALUE_TYPE_INT 0xC001
#define VALUE_TYPE_FLOAT 0xA001
#define VALUE_TYPE_STRING 0x9001
#define VALUE_TYPE_DYNAMIC_STRING 0x9801
#define VALUE_TYPE_PTR 0xE001

typedef unsigned short opcode_t;

typedef struct Procedure {
    int field_0;
    int field_4;
    int field_8;
    int field_C;
    int field_10;
    int field_14;
} Procedure;

typedef struct ProgramValue {
    opcode_t opcode;
    union {
        int integerValue;
        float floatValue;
        void* pointerValue;
    };

    bool isEmpty();
} ProgramValue;

typedef std::vector<ProgramValue> ProgramStack;

typedef struct Program Program;
typedef int(InterpretCheckWaitFunc)(Program* program);

// It's size in original code is 144 (0x8C) bytes due to the different
// size of `jmp_buf`.
typedef struct Program {
    char* name;
    unsigned char* data;
    struct Program* parent;
    struct Program* child;
    int instructionPointer; // current pos in data
    int framePointer; // saved stack 1 pos - probably beginning of local variables - probably called base
    int basePointer; // saved stack 1 pos - probably beginning of global variables
    unsigned char* staticStrings; // static strings table
    unsigned char* dynamicStrings; // dynamic strings table
    unsigned char* identifiers;
    unsigned char* procedures;
    jmp_buf env;
    unsigned int waitEnd; // end time of timer (field_74 + wait time)
    unsigned int waitStart; // time when wait was called
    int field_78; // time when program begin execution (for the first time)?, -1 - program never executed
    InterpretCheckWaitFunc* checkWaitFunc;
    int flags; // flags
    int windowId;
    bool exited;
    ProgramStack* stackValues;
    ProgramStack* returnStackValues;
} Program;

typedef char*(InterpretMangleFunc)(char* fileName);
typedef int(InterpretOutputFunc)(char* string);
typedef unsigned int(InterpretTimerFunc)();
typedef void(OpcodeHandler)(Program* program);

void interpretSetTimeFunc(InterpretTimerFunc* timerFunc, int timerTick);
char* interpretMangleName(char* fileName);
void interpretOutputFunc(InterpretOutputFunc* func);
int interpretOutput(const char* format, ...);
void interpretError(const char* format, ...);
void interpretFreeProgram(Program* program);
Program* allocateProgram(const char* path);
char* interpretGetString(Program* program, opcode_t opcode, int offset);
char* interpretGetName(Program* program, int offset);
int interpretAddString(Program* program, char* string);
void initInterpreter();
void interpretClose();
void interpretEnableInterpreter(int enabled);
void interpret(Program* program, int a2);
void executeProc(Program* program, int procedureIndex);
int interpretFindProcedure(Program* prg, const char* name);
void executeProcedure(Program* program, int procedureIndex);
void runProgram(Program* program);
Program* runScript(char* name);
void interpretSetCPUBurstSize(int value);
void updatePrograms();
void clearPrograms();
void clearTopProgram();
char** getProgramList(int* programListLengthPtr);
void freeProgramList(char** programList, int programListLength);
void interpretAddFunc(int opcode, OpcodeHandler* handler);
void interpretSetFilenameFunc(InterpretMangleFunc* func);
void interpretSuspendEvents();
void interpretResumeEvents();
int interpretSaveProgramState();
int interpretLoadProgramState();

void programStackPushValue(Program* program, ProgramValue& programValue);
void programStackPushInteger(Program* program, int value);
void programStackPushFloat(Program* program, float value);
void programStackPushString(Program* program, char* string);
void programStackPushPointer(Program* program, void* value);

ProgramValue programStackPopValue(Program* program);
int programStackPopInteger(Program* program);
float programStackPopFloat(Program* program);
char* programStackPopString(Program* program);
void* programStackPopPointer(Program* program);

void programReturnStackPushValue(Program* program, ProgramValue& programValue);
void programReturnStackPushInteger(Program* program, int value);
void programReturnStackPushPointer(Program* program, void* value);

ProgramValue programReturnStackPopValue(Program* program);
int programReturnStackPopInteger(Program* program);
void* programReturnStackPopPointer(Program* program);

} // namespace fallout

#endif /* FALLOUT_INT_INTRPRET_H_ */
