#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <iomanip>
using namespace std;
#define BUFFERSIZE 1024
#define MEMORYSIZE 512
#define ERRORBUFFER 256
int linenum = 0;
int lineoffset = 0;
bool afterDef = true;
ifstream is;

class Symbol {
public:
    string symName;
    int value;
    int module;
    bool duplicated;
    bool used;
    bool inUseList;

    Symbol() {
        symName = "";
        value = 0;
        module = 0;
        duplicated = false;
        used = false;
        inUseList = true;
    }
};
class Module {
public:
    int moduleNum;
    int instNum;
    int currentBase;
    vector<Symbol*> symbolTable;

    int getModuleNum() const {
        return moduleNum;
    }

    void setModuleNum(int moduleNum) {
        Module::moduleNum = moduleNum;
    }

    int getInstNum() const {
        return instNum;
    }

    void setInstNum(int instNum) {
        Module::instNum = instNum;
    }

    int getCurrentBase() const {
        return currentBase;
    }

    void setCurrentBase(int currentBase) {
        Module::currentBase = currentBase;
    }

    const vector<Symbol *> &getSymbolTable() const {
        return symbolTable;
    }

    void setSymbolTable(const vector<Symbol *> &symbolTable) {
        Module::symbolTable = symbolTable;
    }
};

void __parseerror(int errcode) {
    static char* errstr[] = {
            "NUM_EXPECTED",             // Number expect
            "SYM_EXPECTED",             // Symbol Expected
            "ADDR_EXPECTED",            // Addressing Expected which is A/E/I/R
            "SYM_TOO_LONG",             // Symbol Name is too long
            "TOO_MANY_DEF_IN_MODULE",   // > 16
            "TOO_MANY_USE_IN_MODULE",   // > 16
            "TOO_MANY_INSTR",           // total num_instr exceeds memory size (512)
    };
    printf("Parse Error line %d offset %d: %s\n", linenum, lineoffset, errstr[errcode]);
}

char* getToken() {
    static bool eol = true;
    static char buffer[BUFFERSIZE];
    static int length;
    static char* strtokPtr;
    static char* tok;

    while(1) {
        if(eol) {
            if(is.eof()) {
//                cout<<"linenum: "<<linenum<<" offset: "<<lineoffset<<endl;
                return nullptr;
            }
            if(!is.getline(buffer, BUFFERSIZE)) {
                lineoffset = 1 + length;
                return nullptr;
            }
            else {
                eol = false;
                length = strlen(buffer);
                strtokPtr = buffer;
                linenum++;
            }
        }
        tok = strtok(strtokPtr, " \t\n");
        strtokPtr = nullptr;
        if(tok == nullptr) {
            eol = true;
            continue;
        }
        lineoffset = tok - buffer + 1;
        return tok;
    }
}

int readInt() {
    char* tok = getToken();
    if(tok != nullptr) {
        for(int i=0; i<strlen(tok); i++) {
            if(!isdigit(tok[i])) {
                __parseerror(0);
                exit(1);
            }
        }
        return atoi(tok);
    }
    if (afterDef) {
        __parseerror(0);
        exit( EXIT_FAILURE );
    }
    else {
        return -1;
    }
}

char* readSymbol() {
    char* tok = getToken();
    if(tok == nullptr) {
        __parseerror(1);
        exit(1);
    }
    else if(isdigit(tok[0]) || !isalnum(tok[0])) {
        __parseerror(1);
        exit(1);
    }
    else if(strlen(tok) > 16) {
        __parseerror(3);
        exit(1);
    }
    return tok;
}

char readIAER() {
    char* tok = getToken();
    if(tok == nullptr) {
        __parseerror(2);
        exit(1);
    }
    else if(*tok != 'I' && *tok != 'A' && *tok != 'E' && *tok != 'R') {
        __parseerror(2);
        exit(1);
    }
    return *tok;
}

Module module;
// Pass1
void Pass1() {
    module.setInstNum(0);
    module.setModuleNum(1);
    vector<Symbol*> defList;
    vector<Symbol*> symbolTable = module.getSymbolTable();
    while(!is.eof()) {
        int moduleNum = module.getModuleNum();
        int instNum = module.getInstNum();
        // get defcount
        afterDef = false;
        int defcount = readInt();
        if(defcount == -1) {
            break;
        }
        if(defcount > 16) {
            __parseerror(4);
            exit(1);
        }
        for(int i=0; i<defcount; i++) {
            char* defSym = readSymbol();
            afterDef = true;
            int val = readInt();
            bool inTable = false;
            Symbol *newSym = nullptr;
            for(int j=0; j<symbolTable.size(); j++) {
                if(symbolTable[j]->symName == defSym) {
                    symbolTable[j]->duplicated = true;
                    newSym = symbolTable[j];
                    inTable = true;
                    break;
                }
            }
            // create symbol
            if(!inTable) {
                newSym = new Symbol();
                newSym->symName = defSym;
                newSym->value = val + instNum;
                newSym->module = moduleNum;
                symbolTable.push_back(newSym);

            }
            defList.push_back(newSym);
        }

        // get usecount
        afterDef = true;
        int usecount = readInt();
        if(usecount == -1) {
            break;
        }
        if(usecount > 16) {
            __parseerror(5);
            exit(1);
        }
        for(int i=0; i<usecount; i++) {
            char* useSym = readSymbol();
        }

        // get instcount
        afterDef = true;
        int instcount = readInt();
        if(instcount == -1) {
            break;
        }
        if(instcount + instNum >= MEMORYSIZE) {
            __parseerror(6);
            exit(1);
        }
        for(int i=0; i<instcount; i++) {
            char addressmode = readIAER();
            afterDef = true;
            int op = readInt();
        }
        // done reading
        // check warning
        for(int i=0; i<defList.size(); i++) {
            if(defList[i]->value >= instNum + instcount) {
                string sym = defList[i]->symName.c_str();
                printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", moduleNum, defList[i]->symName.c_str(), defList[i]->value - instNum, instcount - 1);
                defList[i]->value = instNum;
            }
        }
        module.setModuleNum(moduleNum + 1);
        module.setInstNum(instNum + instcount);
        module.setSymbolTable(symbolTable);
        defList.clear();
    }

}
// Pass1 end

void printSymbolTable() {
//    vector<Symbol*> symbolTable = module.symbolTable;
    vector<Symbol*> symbolTable = module.getSymbolTable();
    cout<<"Symbol Table"<<endl;
    for(int i=0; i<symbolTable.size(); i++) {
        cout<<symbolTable[i]->symName<<"="<<symbolTable[i]->value;
        if(symbolTable[i]->duplicated) {
            cout<<" Error: This variable is multiple times defined; first value used";
        }
        cout<<endl;
    }
}

string dealA(char* buffer, int opcode, int operand, int& op) {
    string error;
    if(opcode >= 10) {
        error = sprintf(buffer, " Error: Illegal opcode; treated as 9999");
        op = 9999;
    }
    else if(operand >= MEMORYSIZE) {
        error = sprintf(buffer, " Error: Absolute address exceeds machine size; zero used");
        operand = 0;
        op = opcode * 1000; //
    }
    return error;
}

string dealR(char* buffer, int opcode, int operand, int& op, int currentBase, int instcount) {
    string error;
    if(opcode >= 10) {
        error = sprintf(buffer, " Error: Illegal opcode; treated as 9999");
        op = 9999;
    }
    else {
        if(operand > instcount - 1) {
            error = sprintf(buffer, " Error: Relative address exceeds module size; zero used");
            op = op - operand;
        }
        if(module.moduleNum != 0) {
            op = op + currentBase;
        }
    }
    return error;
}

string dealE(char* buffer, int opcode, int operand, int& op, vector<Symbol>& useList, vector<Symbol*> symbolTable) {
    string error;
    if(opcode >= 10) {
        error = sprintf(buffer, " Error: Illegal opcode; treated as 9999");
        op = 9999;
    }
    else {
        if(operand >= useList.size()) {
            error = sprintf(buffer, " Error: External address exceeds length of uselist; treated as immediate");
        }
        else {
            string usedSym = useList[operand].symName;
            bool defined = false;
            for(int i=0; i<symbolTable.size(); i++) {
                if(usedSym == symbolTable[i]->symName) {
                    defined = true;
                    op = opcode*1000 + symbolTable[i]->value;
                    symbolTable[i]->used = true;
                    break;
                }
            }
            useList[operand].inUseList = true;
            if(!defined) {
                error = sprintf(buffer, " Error: %s is not defined; zero used", usedSym.c_str());
                op = opcode * 1000;
            }
        }
    }
    return error;
}

string dealI(char* buffer, int opcode, int& op) {
    string error;
    if(opcode >= 10) {
        error = sprintf(buffer, " Error: Illegal immediate value; treated as 9999");
        op = 9999;
    }
    return error;
}

// Pass2
void Pass2() {
    module.setInstNum(-1);
    module.setModuleNum(1);
    module.setCurrentBase(0);
    vector<Symbol> useList;
    vector<Symbol*> symbolTable = module.getSymbolTable();
    int currentBase = module.getCurrentBase();

    while(!is.eof()) {
        int moduleNum = module.getModuleNum();
        int instNum = module.getInstNum();
        // get defcount
        afterDef = false;
        int defcount = readInt();
        if(defcount == -1) {
            break;
        }
        for(int i=0; i<defcount; i++) {
            char* defSym = readSymbol();
            afterDef = true;
            int val = readInt();
        }

        // get usecount
        afterDef = true;
        int usecount = readInt();
        if(usecount == -1) {
            break;
        }
        for(int i=0; i<usecount; i++) {
            char* useSym = readSymbol();
            Symbol newSym;
            newSym.symName = useSym;
            newSym.module = moduleNum;
            newSym.inUseList = false;
            useList.push_back(newSym);
        }

        // get instcount
        afterDef = true;
        int instcount = readInt();
        if(instcount == -1) {
            break;
        }
        char buffer[ERRORBUFFER] = "";
        string error;
        for(int i=0; i<instcount; i++) {
            // get addressmode
            char addressmode = readIAER();
            module.instNum = module.instNum + 1;
            instNum = module.instNum;
            afterDef = true;
            int op = readInt();
            int opcode = op / 1000;
            int operand = op % 1000;
            buffer[0] = '\0';
            switch (addressmode) {
                case 'A':
                    error = dealA(buffer, opcode, operand, op);
                    break;
                case 'R':
                    error = dealR(buffer, opcode, operand, op, currentBase, instcount);
                    break;
                case 'E':
                    error = dealE(buffer, opcode, operand, op, useList, symbolTable);
                    break;
                case 'I':
                    error = dealI(buffer, opcode, op);
                    break;
            }
            cout << setfill ('0') << setw(3) << instNum << ": " << setfill ('0') << setw(4) << op;
            if (strlen(buffer) > 0) {
                cout << buffer;
            }
            printf("\n");
        }
        for(int i=0; i<useList.size(); i++) {
            if(!useList[i].inUseList) {
                printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", moduleNum, useList[i].symName.c_str());
            }
        }
        module.setModuleNum(moduleNum + 1);
        currentBase += instcount;
        module.setCurrentBase(currentBase);
        useList.clear();
    }
}
// Pass2 end

void printWarning() {
    vector<Symbol*> symbolTable = module.getSymbolTable();
    printf("\n");
    for(int i = 0; i < symbolTable.size(); i++){
        if(!symbolTable[i]->used){
            printf("Warning: Module %d: %s was defined but never used\n", symbolTable[i] -> module, symbolTable[i] -> symName.c_str());
        }
    }
    printf("\n");
}

int main (int argc, char* argv[]) {
    is.open(argv[1]);
    if(argc == 1) {
        cout<<"Expected argument after options"<<endl;
        return 0;
    }
    if(!is.is_open() || argc > 2) {
        cout<<"Not a valid inputfile <"<<argv[1]<<">"<<endl;
        return 0;
    }
    Pass1();
    printSymbolTable();
    is.close();
    is.open(argv[1]);
    printf("\nMemory Map\n");
    Pass2();
    printWarning();
    is.close();
    return 0;
}
