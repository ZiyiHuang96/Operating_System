#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <vector>
#include <deque>
using namespace std;

const int MAX_VPAGES = 64;
int num_frames;
unsigned long long cost = 0;
unsigned long inst_count = 0;
unsigned long ctx_switches = 0;
unsigned long process_exits = 0;
vector<int> randvals;
int ofs = 0;

struct Options {
    bool O = false;
    bool P = false;
    bool F = false;
    bool S = false;
    char algo;
};
Options options;

struct pte_t{
    unsigned int PRESENT:1;
    unsigned int WRITE_PROTECT:1;
    unsigned int MODIFIED:1;
    unsigned int REFERENCED:1;
    unsigned int PAGEDOUT:1;
    unsigned int PHYSICAL_FRAME_NUMBER:7;
    unsigned int FILE_MAPPED:1;
    unsigned int VMA_ASSIGNED:1;
    pte_t(): PRESENT(0), WRITE_PROTECT(0), MODIFIED(0), REFERENCED(0),
             PAGEDOUT(0), FILE_MAPPED(0), VMA_ASSIGNED(0){}
};

struct VMA{
    unsigned int start_vpage : 6;
    unsigned int end_vpage : 6;
    unsigned int write_protected : 1;
    unsigned int file_mapped : 1;
};

class Process {
public:
    int pid, num_vma;
    unsigned long unmaps, maps, ins, outs, fins, fouts, zeros, segv, segprot;
    VMA *vma;
    pte_t *page_table;
    Process(int num_vma) {
        this->num_vma = num_vma;
        this->vma = new VMA[num_vma];
        this->page_table = new pte_t[MAX_VPAGES];
        this->unmaps = 0;
        this->maps = 0;
        this->ins = 0;
        this->outs = 0;
        this->fins = 0;
        this->fouts = 0;
        this->zeros = 0;
        this->segv = 0;
        this->segprot = 0;
    }
};

struct frame_t {
    int frame_num = 0;
    Process* proc = nullptr;
    int vpage = -1;
    bool is_mapped = false;
    unsigned int age = 0;
    int timelastuse = 0;
};

struct Instruction {
    char instr;
    int num;
};

vector<Instruction> instruction_t;
vector<pte_t> page_table;
vector<frame_t> frame_table;
deque<frame_t*> free_pool;
vector<Process*> processes;
Process *current_process;

int myrandom(int burst) {
    if(ofs == randvals.size()) {
        ofs = 0;
    }
    return (randvals[ofs++] % burst);
}

void read_randfile(char* fileName) {
    ifstream randFile;
    randFile.open(fileName);
    string line;
    getline(randFile, line);
    while (getline(randFile, line)) {
        randvals.push_back(stoi(line));
    }
    randFile.close();
}

class Pager {
public:
    virtual frame_t* select_victim_frame() = 0;
};
class FIFO : public Pager {
public:
    int idx = 0;
    frame_t* select_victim_frame() {
        return &frame_table[(idx++) % num_frames];
    }
};
class Random : public Pager {
public:
    frame_t* select_victim_frame() {
        return &frame_table[myrandom(frame_table.size())];
    }
};
class Clock : public Pager {
    int i = 0;
    frame_t* hand = &frame_table.front();
public:
    frame_t* select_victim_frame() {
        while(processes[hand->proc->pid]->page_table[hand->vpage].REFERENCED) {
            i = (i + 1) % num_frames;
            processes[hand->proc->pid]->page_table[hand->vpage].REFERENCED = 0;
            hand = &frame_table[i];
        }
        frame_t* victim = hand;
        i = (i + 1) % num_frames;
        hand = &frame_table[i];
        return victim;
    }
};
class NRU : public Pager {
    frame_t* hand = &frame_table.front();
    frame_t** frame_class = new frame_t*[4];
    int idx = 0;
    int class_idx = 0;
    unsigned long last_count = -1;
    pte_t* pte;
public:
    frame_t* select_victim_frame() {
        for(int i=0; i<4; i++) {
            frame_class[i] = nullptr;
        }
        for(int i=0; i<num_frames; i++) {
            hand = &frame_table[idx];
            pte = &hand->proc->page_table[hand->vpage];
            class_idx = 2 * pte->REFERENCED + pte->MODIFIED;
            if(frame_class[class_idx] == nullptr) {
                frame_class[class_idx] = hand;
            }
            if(class_idx == 0) {
                break;
            }
            idx = (idx + 1) % num_frames;
        }
        if(inst_count - last_count >= 50) {
            for(int i=0; i<num_frames; i++) {
                pte = &frame_table[i].proc->page_table[frame_table[i].vpage];
                pte->REFERENCED = 0;
            }
            last_count = inst_count;
        }
        for(int i=0; i<4; i++) {
            if(frame_class[i] != nullptr) {
                hand = frame_class[i];
                idx = hand->frame_num;
                idx = (idx + 1) % num_frames;
                break;
            }
        }
        return hand;
    }
};
class Aging : public Pager {
    int idx = 0;
    frame_t* hand = &frame_table.front();
    frame_t* temp = &frame_table.front();
public:
    frame_t* select_victim_frame() {
        hand = &frame_table[idx];
        for(int i=0; i<num_frames; i++) {
            temp = &frame_table[(i+idx) % num_frames];
            temp->age = temp->age >> 1;
            if(temp->proc->page_table[temp->vpage].REFERENCED) {
                temp->proc->page_table[temp->vpage].REFERENCED = 0;
                temp->age = temp->age | 0x80000000;
            }
            if(temp->age < hand->age) {
                hand = temp;
            }
        }
        idx = (hand->frame_num + 1) % num_frames;
        return hand;
    }

};
class WorkingSet : public Pager {
    int idx = 0;
    frame_t* hand = &frame_table.front();
    frame_t* temp = &frame_table.front();
    int last_age;
    int age;
    int TAU = 49;
public:
    frame_t* select_victim_frame() {
        last_age = -1;
        hand = &frame_table[idx];
        for(int i=0; i<num_frames; i++) {
            temp = &frame_table[(i+idx) % num_frames];
            age = inst_count - temp->timelastuse;
            if(temp->proc->page_table[temp->vpage].REFERENCED) {
                temp->proc->page_table[temp->vpage].REFERENCED = 0;
                temp->timelastuse = inst_count;
            }
            else if(age > TAU) {
                idx = (temp->frame_num + 1) % num_frames;
                return temp;
            }
            else if(age > last_age) {
                last_age = max(age, last_age);
                hand = temp;
            }
        }
        idx = (hand->frame_num + 1) % num_frames;
        return hand;
    }
};

Pager *pager;

frame_t* get_frame() {
    frame_t *frame;
    if(free_pool.empty()) {
        frame = pager->select_victim_frame();
        return frame;
    }
    frame = free_pool.front();
    free_pool.pop_front();
    return frame;
}

void current_process_mapping(pte_t* pte, frame_t* newframe, int vpage) {
    pte->PRESENT = 1;
    pte->PHYSICAL_FRAME_NUMBER = newframe->frame_num;
    newframe->is_mapped = true;
    newframe->proc = current_process;
    newframe->vpage = vpage;
    newframe->age = 0;
    newframe->timelastuse = inst_count;

    cost += 300;
    current_process->maps++;
}

void pagefault_handler(pte_t *pte, Instruction cur_inst){
    frame_t* frame;
    pte_t* p_pte;
    for(int i=0; i<current_process->num_vma; i++) {
        if(current_process->vma[i].start_vpage <= cur_inst.num && current_process->vma[i].end_vpage >= cur_inst.num) {
            pte->VMA_ASSIGNED = 1;
            pte->WRITE_PROTECT = 1;
            pte->WRITE_PROTECT = current_process->vma[i].write_protected;
            pte->FILE_MAPPED = current_process->vma[i].file_mapped;
        }
    }
    if(!pte->VMA_ASSIGNED && options.O) {
        printf(" SEGV\n");
    }
    if(!pte->VMA_ASSIGNED) {
        cost++;
        cost += 340;
        current_process->segv++;
        return;
    }
    else {
        frame = get_frame();
        if(frame->is_mapped && options.O) {
            printf(" UNMAP %d:%d\n", frame->proc->pid, frame->vpage);
        }
        if(frame->is_mapped) {
            p_pte = &frame->proc->page_table[frame->vpage];
            cost += 400;
            frame->proc->unmaps++;
            if(p_pte->MODIFIED){
                if(p_pte->FILE_MAPPED && options.O){
                    printf(" FOUT\n");
                }
                if(p_pte->FILE_MAPPED){
                    cost += 2400;
                    frame->proc->fouts++;
                }
                if(!p_pte->FILE_MAPPED && options.O){
                    printf(" OUT\n");
                }
                if(!p_pte->FILE_MAPPED) {
                    p_pte->PAGEDOUT = 1;
                    cost += 2700;
                    frame->proc->outs++;
                }
                p_pte->MODIFIED = 0;
            }
            p_pte->PRESENT = 0;
            frame->is_mapped = false;
            frame->proc = nullptr;
            frame->vpage = -1;
        }
        if(pte->FILE_MAPPED && options.O) {
            printf(" FIN\n");
        }
        if(pte->FILE_MAPPED) {
            cost += 2800;
            current_process->fins++;
        }
        if(!pte->FILE_MAPPED) {
            if(pte->PAGEDOUT && options.O) {
                printf(" IN\n");
            }
            if(pte->PAGEDOUT) {
                cost += 3100;
                current_process->ins++;
            }
            if(!pte->PAGEDOUT && options.O){
                printf(" ZERO\n");
            }
            if(!pte->PAGEDOUT){
                pte->PRESENT = 0;
                pte->MODIFIED = 0;
                pte->REFERENCED = 0;
                pte->PHYSICAL_FRAME_NUMBER = 0;
                pte->PAGEDOUT = 0;
                cost += 140;
                current_process->zeros++;
            }
        }
        current_process_mapping(pte, frame, cur_inst.num);
        if(options.O) {
            printf(" MAP %d\n", pte->PHYSICAL_FRAME_NUMBER);
        }
    }
}

void Simulation(unsigned long inst_count) {
    pte_t* pte;
    frame_t* fte;
    while(!instruction_t.empty()) {
        Instruction cur_inst = instruction_t.front();
        instruction_t.erase(instruction_t.begin());
        if(options.O) {
            printf("%lu: ==> %c %d\n", inst_count, cur_inst.instr, cur_inst.num);
        }
        switch (cur_inst.instr) {
            case 'c':
                current_process = processes[cur_inst.num];
                ctx_switches++;
                cost += 130;
                break;
            case 'e':
                if(options.O && cur_inst.num == current_process->pid) {
                    printf("EXIT current process %d\n", current_process->pid);
                }
                process_exits++;
                cost += 1250;
                for(int i=0; i<MAX_VPAGES; i++) {
                    pte = &current_process->page_table[i];
                    if(pte->PRESENT && options.O){
                        printf(" UNMAP %d:%d\n", current_process->pid, i);
                    }
                    if(pte->PRESENT && pte->MODIFIED && pte->FILE_MAPPED) {
                        if(options.O) printf(" FOUT\n");
                        cost += 2400;
                        current_process->fouts += 1;
                        pte->MODIFIED = 0;
                    }
                    if(pte->PRESENT) {
                        cost += 400;
                        current_process->unmaps++;
                        fte = &frame_table[pte->PHYSICAL_FRAME_NUMBER];
                        fte->is_mapped = false;
                        fte->proc = nullptr;
                        fte->vpage = -1;
                        free_pool.push_back(fte);
                        pte->PRESENT = 0;
                    }
                    pte->MODIFIED = 0;
                    pte->REFERENCED = 0;
                    pte->PAGEDOUT = 0;
                }
                break;
            default:   // write/read
                pte = &current_process->page_table[cur_inst.num];
                if(!pte->PRESENT){   // pagefault
                    pagefault_handler(pte, cur_inst);
                }
                if(pte->VMA_ASSIGNED){
                    if(cur_inst.instr == 'w') {
                        if(!pte->WRITE_PROTECT) {
                            pte->REFERENCED = 1;
                            pte->MODIFIED = 1;
                            cost++;
                        }
                        else {
                            pte->REFERENCED = 1;
                            current_process->segprot += 1;
                            cost++;
                            cost += 420;
                        }
                        if(pte->WRITE_PROTECT && options.O) {
                            printf(" SEGPROT\n");
                        }
                    }
                    if(cur_inst.instr == 'r') {
                        pte->REFERENCED = 1;
                        cost++;
                    }
                }
                break;
        }
    }
}

void read_inputfile(char* fileName) {
    ifstream inputFile;
    inputFile.open(fileName);
    string line;
    int num_processes = 0;
    int num_vma = 0;
    int pid = 0;
    while(getline(inputFile, line)) {
        if(line[0] == '#') {
            continue;
        }
        else {
            num_processes = stoi(line);
            break;
        }
    }
    for(int i=0; i<num_processes; i++) {
        while(getline(inputFile, line)) {
            if(line[0] == '#') {
                continue;
            }
            else {
                num_vma = stoi(line);
                break;
            }
        }
        Process* proc = new Process(num_vma);
        proc->pid = pid;
        pid++;
        for(int j=0; j<num_vma; j++) {
            getline(inputFile, line);
            stringstream ss(line);
            int start, end, protect, mapped;
            ss >> start >> end >> protect >> mapped;
            proc->vma[j].start_vpage = start;
            proc->vma[j].end_vpage = end;
            proc->vma[j].write_protected = protect;
            proc->vma[j].file_mapped = mapped;
        }
        processes.push_back(proc);
    }
    getline(inputFile, line);
    pte_t *pte;
    while(getline(inputFile, line)) {
        if(line[0] == '#') {
            break;
        }
        Instruction instruction;
        stringstream ss(line);
        ss >> instruction.instr >> instruction.num;
        instruction_t.push_back(instruction);
        Simulation(inst_count);
        inst_count++;
    }

    inputFile.close();
}

void readCmd(int argc, char* argv[]) {
    int c;
    while ((c = getopt (argc, argv, "f:a:o:")) != -1) {
        switch (c) {
            case 'f':
                num_frames = atoi(optarg);
                frame_table.resize(num_frames);
                for(int i = 0; i < num_frames; i++){
                    frame_table[i].frame_num = i;
                    frame_table[i].is_mapped = false;
                    frame_table[i].proc = nullptr;
                    frame_table[i].vpage = -1;
                    free_pool.push_back(&frame_table[i]);
                }
                break;
            case 'a':
                options.algo = optarg[0];
                switch (options.algo) {
                    case 'f':
                        pager = new FIFO();
                        break;
                    case 'r':
                        pager = new Random();
                        break;
                    case 'c':
                        pager = new Clock();
                        break;
                    case 'e':
                        pager = new NRU();
                        break;
                    case 'a':
                        pager = new Aging();
                        break;
                    case 'w':
                        pager = new WorkingSet();
                        break;
                }
                break;
            case 'o':
                for (int i = 0; i < strlen(optarg); i++){
                    switch (optarg[i]){
                        case 'O':
                            options.O = true;
                            break;
                        case 'P':
                            options.P = true;
                            break;
                        case 'F':
                            options.F = true;
                            break;
                        case 'S':
                            options.S = true;
                            break;
                    }
                }
                break;
        }
    }
}

void printInfo() {
    if(options.P) {
        for(int i=0; i<processes.size(); i++) {
            Process* proc = processes[i];
            printf("PT[%d]: ", i);
            for(int j = 0; j<MAX_VPAGES; j++) {
                if(proc->page_table[j].PRESENT) {
                    printf("%d:", j);
                    if(proc->page_table[j].REFERENCED) {
                        printf("R");
                    }
                    else {
                        printf("-");
                    }
                    if(proc->page_table[j].MODIFIED) {
                        printf("M");
                    }
                    else {
                        printf("-");
                    }
                    if(proc->page_table[j].PAGEDOUT) {
                        printf("S ");
                    }
                    else {
                        printf("- ");
                    }
                }
                else {
                    if(proc->page_table[j].PAGEDOUT) {
                        printf("# ");
                    }
                    else {
                        printf("* ");
                    }
                }
            }
            printf("\n");
        }
    }
    if(options.F) {
        printf("FT: ");
        for(int i=0; i<frame_table.size(); i++) {
            if(frame_table[i].is_mapped) {
                printf("%d:%d ", frame_table[i].proc->pid, frame_table[i].vpage);
            }
            else {
                printf("* ");
            }
        }
        printf("\n");
    }
    if(options.S) {
        for(int i=0; i<processes.size(); i++) {
            Process* proc = processes[i];
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                   proc->pid,
                   proc->unmaps, proc->maps, proc->ins, proc->outs,
                   proc->fins, proc->fouts, proc->zeros,
                   proc->segv, proc->segprot);
        }
        printf("TOTALCOST %lu %lu %lu %llu %lu\n",
               inst_count, ctx_switches, process_exits, cost, sizeof(pte_t));
    }
}

int main(int argc, char* argv[]){
    readCmd(argc, argv);
    read_randfile(argv[5]);
    read_inputfile(argv[4]);
    printInfo();
    return 0;
}
