#include <iostream>
#include <list>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <sstream>
using namespace std;
int quantum=1000000;
int maxprio=4;
int total_cpu_time = 0;
int total_turnaround_time = 0;
int total_cpu_wait = 0;
double cpu_util = 0.0;
double IO_util = 0.0;
double avg_turnaround_time = 0.0;
double avg_cpu_wait = 0.0;
double throughput = 0.0;
int total_time;
int CURRENT_TIME;
int timeInPrevState;
string scheduler_to_print;
enum trans{
    TRANS_TO_READY,
    TRANS_TO_RUN,
    TRANS_TO_BLOCK,
    TRANS_TO_PREEMPT,
    TRANS_TO_FINISH
};
enum schedulers{
    FCFS,
    LCFS,
    SRTF,
    RR,
    PRIO,
    PREPRIO
};
schedulers schedule_method;

struct Process{
    int pid;
    int AT;
    int TC;
    int CB;
    int IO;
    int PRIO;
    int DYNAMIC_PRIO;
    int FT;
    int TT;
    int IT;
    int CW;
    int totalRemaining;
    int burstRemaining;
    int state_ts;
    int start_time;
    int burstTemp;
};
vector<Process> processes;

struct Event {
    int timestamp;
    Process* evtProc;
    trans transition;

    int getTimestamp() const {
        return timestamp;
    }

    void setTimestamp(int timestamp) {
        Event::timestamp = timestamp;
    }

    Process *getevtProc() const {
        return evtProc;
    }

    void setevtProc(Process *evtProc) {
        Event::evtProc = evtProc;
    }

    trans getTransition() const {
        return transition;
    }

    void setTransition(trans transition) {
        Event::transition = transition;
    }
};

Event encodeEvent(Event event, int timeStamp, Process &eventProcess, trans state) {
    event.setTimestamp(timeStamp);
    event.setevtProc(&eventProcess);
    event.setTransition(state);
    return event;
}

class DES_layer {
public:
    vector<Event> evts;
    Process* current_running = nullptr;
    Process* current_blocked = nullptr;
    list<Process*> *activeQ, *expiredQ;
    int IOT = 0;
    int IOE = 0;
    Event get_event() {
        if(evts.size() == 0) {
            Event evt;
            evt.setTimestamp(-1);
            evt.setevtProc(nullptr);
            return evt;
        }
        Event evt;
        evt = evts.front();
        evts.erase(evts.begin());
        return evt;
    }

    void put_event(Event evt) { //insert_sort()
        int i = 0;
        while(i< evts.size() && evt.getTimestamp() >= evts[i].getTimestamp()) {
            i++;
        }
        evts.insert(evts.begin()+i, evt);
    }
};
DES_layer des;

class Scheduler {
public:
    list<Process*> readyQ;
    virtual void add_process(Process *proc){};
    virtual Process* get_next_process(){ return nullptr; };
    virtual void test_preempt(Process *proc, int curtime){}; // typically NULL but for ‘E’
};
bool remComp(const Process* lhs, const Process* rhs) {
    return lhs->totalRemaining < rhs->totalRemaining;
}
class FCFS : public Scheduler {
public:
    void add_process(Process *proc) override {
        readyQ.push_back(proc);
    };
    Process* get_next_process() override {
        if(readyQ.empty()) {
            return nullptr;
        }
        Process* proc = readyQ.front();
        readyQ.pop_front();
        return proc;
    };
    void test_preempt(Process *proc, int curtime) override {

    };
};
class LCFS : public Scheduler {
public:
    void add_process(Process *proc) override {
        readyQ.push_front(proc);
    };
    Process* get_next_process() override {
        if(readyQ.empty()) {
            return nullptr;
        }
        Process* proc = readyQ.front();
        readyQ.pop_front();
        return proc;
    };
    void test_preempt(Process *proc, int curtime) override {

    };
};
class SRTF : public Scheduler {
public:
    void add_process(Process *proc) override {
        readyQ.push_back(proc);
    };
    Process* get_next_process() override {
        if(readyQ.empty()) {
            return nullptr;
        }
        list<Process*>::iterator it = std::min_element(readyQ.begin(), readyQ.end(), remComp);
        readyQ.remove(*it);
        return *it;
    };
    void test_preempt(Process *proc, int curtime) override {

    };
};
class RR : public Scheduler {
public:
    void add_process(Process *proc) override {
        readyQ.push_back(proc);
    };
    Process* get_next_process() override {
        if(readyQ.empty()) {
            return nullptr;
        }
        Process* proc = readyQ.front();
        readyQ.pop_front();
        return proc;
    };
    void test_preempt(Process *proc, int curtime) override {

    };
};
class PRIO : public Scheduler {
public:
    void add_process(Process *proc) override {
        if(proc->DYNAMIC_PRIO >= 0) {
            des.activeQ[proc->DYNAMIC_PRIO].push_back(proc);
        }
        else {
            proc->DYNAMIC_PRIO = proc->PRIO - 1;
            des.expiredQ[proc->DYNAMIC_PRIO].push_back(proc);
        }
    };
    Process* get_next_process() override {
        bool no_proc = true;
        Process* proc;
        for(int i = maxprio - 1; i >= 0; i--) {
            if(des.activeQ[i].size() > 0) {
                proc = des.activeQ[i].front();
                des.activeQ[i].pop_front();
                no_proc = false;
                return proc;
            }
        }
        if(no_proc) {
            swap(des.activeQ, des.expiredQ);
            for(int i = maxprio - 1; i >= 0; i--) {
                if(des.activeQ[i].size() > 0) {
                    proc = des.activeQ[i].front();
                    des.activeQ[i].pop_front();
                    return proc;
                }
            }
        }
        return nullptr;
    };
    void test_preempt(Process *proc, int curtime) override {

    };
};
class PREPRIO : public Scheduler {
public:
    int pendingTimeStamp(vector<Event>& events, Process* current_running){
        for (int i=0; i<events.size();i++){
            if(events[i].evtProc->pid==current_running->pid){
                return events[i].timestamp;
            }
        }
        return -1;
    }
    void add_process(Process *proc) override {
        if(proc->DYNAMIC_PRIO >= 0) {
            des.activeQ[proc->DYNAMIC_PRIO].push_back(proc);
        }
        else {
            proc->DYNAMIC_PRIO = proc->PRIO - 1;
            des.expiredQ[proc->DYNAMIC_PRIO].push_back(proc);
        }
        if(des.current_running && proc->DYNAMIC_PRIO > des.current_running->DYNAMIC_PRIO && CURRENT_TIME != pendingTimeStamp(des.evts, des.current_running)) {
            int current_used = CURRENT_TIME - des.current_running->start_time;
            if(des.current_running->burstTemp <= quantum && des.current_running->burstRemaining == 0) {
                des.current_running->totalRemaining += des.current_running->burstTemp - current_used;
                des.current_running->burstRemaining += des.current_running->burstTemp - current_used;
                des.current_running->burstTemp = des.current_running->burstRemaining;
            }
            else {
                des.current_running->totalRemaining += quantum - current_used;
                des.current_running->burstRemaining += quantum - current_used;
            }
            for(int i=0; i<des.evts.size(); i++) {
                if(des.evts[i].evtProc->pid == des.current_running->pid) {
                    des.evts.erase(des.evts.begin() + i);
                }
            }
            Event evt;
            evt = encodeEvent(evt, CURRENT_TIME, *des.current_running, TRANS_TO_PREEMPT);
            des.put_event(evt);

        }
    };
    Process* get_next_process() override {
        bool no_proc = true;
        Process* proc;
        for(int i = maxprio - 1; i >= 0; i--) {
            if(des.activeQ[i].size() > 0) {
                proc = des.activeQ[i].front();
                des.activeQ[i].pop_front();
                no_proc = false;
                return proc;
            }
        }
        if(no_proc) {
            swap(des.activeQ, des.expiredQ);
            for(int i = maxprio - 1; i >= 0; i--) {
                if(des.activeQ[i].size() > 0) {
                    proc = des.activeQ[i].front();
                    des.activeQ[i].pop_front();
                    return proc;
                }
            }
        }
        return nullptr;
    };
    void test_preempt(Process *proc, int curtime) override {

    };
};
//
vector<int> randvals;
int ofs=0;
int myrandom(int burst){
    if (ofs == randvals.size()){
        ofs = 0;
    }
    return 1+(randvals[ofs++] % burst);
}
//

void handle_trans(Process* proc, Event evt, bool& CALL_SCHEDULER, int& burst, int& io_burst, Scheduler* sched) {
    switch (evt.getTransition()) {
        case TRANS_TO_READY: {
            CALL_SCHEDULER = true;
            proc->state_ts = CURRENT_TIME;
            if (des.current_blocked == proc) {
                des.current_blocked = nullptr;
            }
            sched->add_process(proc);
            break;
        }
        case TRANS_TO_RUN: {
            int cpu_wait = CURRENT_TIME - proc->state_ts;
            proc->CW += cpu_wait;
            proc->start_time = CURRENT_TIME;
            des.current_running = proc;
            des.current_running->state_ts = CURRENT_TIME;
            if(des.current_running->burstRemaining > 0) {
                burst = des.current_running->burstRemaining;
                des.current_running->burstTemp = des.current_running->burstRemaining;
            }
            if(des.current_running->burstRemaining <= 0) {
                burst = myrandom(des.current_running->CB);
                des.current_running->burstRemaining = burst;
                des.current_running->burstTemp = des.current_running->burstRemaining;
            }
            if(burst > des.current_running->totalRemaining) {
                burst = des.current_running->totalRemaining;
                des.current_running->burstRemaining = des.current_running->totalRemaining;
                des.current_running->burstTemp = des.current_running->burstRemaining;
            }
            if(burst > quantum) {
                int cur_rem = burst - quantum;
                des.current_running->burstRemaining = cur_rem;
                des.current_running->burstTemp = cur_rem;
                des.current_running->totalRemaining -= quantum;
                Event evt;
                evt = encodeEvent(evt, CURRENT_TIME + quantum, *proc, TRANS_TO_PREEMPT);
                des.put_event(evt);
            }
            else if(burst >= des.current_running->totalRemaining) {
                des.current_running->totalRemaining -= burst;
                Event evt;
                evt = encodeEvent(evt, CURRENT_TIME + burst, *proc, TRANS_TO_FINISH);
                des.put_event(evt);
            }
            else if(burst < des.current_running->totalRemaining) {
                des.current_running->totalRemaining -= burst;
                des.current_running->burstRemaining = 0;
                Event evt;
                evt = encodeEvent(evt, CURRENT_TIME + burst, *proc, TRANS_TO_BLOCK);
                des.put_event(evt);
            }
            break;
        }
        case TRANS_TO_BLOCK:{
            CALL_SCHEDULER = true;
            io_burst = myrandom(proc->IO);
            des.current_running = nullptr;
            proc->IT += io_burst;
            proc->DYNAMIC_PRIO = proc->PRIO - 1;
            Event evt;
            evt = encodeEvent(evt, CURRENT_TIME + io_burst, *proc, TRANS_TO_READY);
            des.put_event(evt);
            proc->state_ts=CURRENT_TIME;
            if(des.current_blocked == nullptr) {
                des.IOT += io_burst;
                des.IOE = io_burst + CURRENT_TIME;
                des.current_blocked = proc;
            }
            if(io_burst + CURRENT_TIME > des.IOE) {
                des.IOT += io_burst + CURRENT_TIME - des.IOE;
                des.IOE = max(io_burst + CURRENT_TIME, des.IOE);
                des.current_blocked = proc;
            }
            break;
        }
        case TRANS_TO_PREEMPT: {
            CALL_SCHEDULER = true;
            proc->state_ts = CURRENT_TIME;
            proc->DYNAMIC_PRIO--;
            des.current_running = nullptr;
            sched->add_process(proc);
            break;
        }
        case TRANS_TO_FINISH: {
            CALL_SCHEDULER = true;
            proc->FT = CURRENT_TIME;
            int turnaroundTime = proc->FT - proc->AT;
            proc->TT = turnaroundTime;
            des.current_running = nullptr;
            total_time = CURRENT_TIME;
            break;
        }
    }
}

void Simulation() {
    Scheduler* sched;
    bool CALL_SCHEDULER = false;
    int burst;
    int io_burst;
    list<Process*> active[maxprio];
    list<Process*> expired[maxprio];
    des.activeQ = active;
    des.expiredQ = expired;
    for(auto & processe : processes) {
        Event evt;
        evt = encodeEvent(evt, processe.AT, processe, TRANS_TO_READY);
        des.put_event(evt);
    }
    switch (schedule_method) {
        case FCFS:
            sched = new class FCFS();
            break;
        case LCFS:
            sched = new class LCFS();
            break;
        case SRTF:
            sched = new class SRTF();
            break;
        case RR:
            sched = new class RR();
            break;
        case PRIO:
            sched = new class PRIO();
            break;
        case PREPRIO:
            sched = new class PREPRIO();
            break;
    }
    for (Event evt = des.get_event(); evt.timestamp!=-1; evt=des.get_event()){
        Process* proc = evt.getevtProc();
        CURRENT_TIME = evt.getTimestamp();
        timeInPrevState = CURRENT_TIME - proc->state_ts;
        handle_trans(proc, evt, CALL_SCHEDULER, burst, io_burst, sched);
        int nextTimestamp;
        if(des.evts.size() == 0) {
            nextTimestamp = -1;
        }
        else{
            nextTimestamp = des.evts.front().getTimestamp();
        }
        if(CALL_SCHEDULER && nextTimestamp == CURRENT_TIME) {
            continue;
        }
        if(CALL_SCHEDULER && nextTimestamp != CURRENT_TIME) {
            CALL_SCHEDULER = false;
            if(des.current_running == nullptr) {
                des.current_running = sched->get_next_process();
                if(!des.current_running) {
                    continue;
                }
                Event evt;
                evt = encodeEvent(evt, CURRENT_TIME, *des.current_running, TRANS_TO_RUN);
                des.put_event(evt);
            }
        }
    }
}

void readCmd(int argc, char* argv[]) {
    int c;
    char* cmd;
    while ((c = getopt (argc, argv, "tevs:")) != -1) {
        switch (c) {
            case 's':
                switch (optarg[0]) {
                    case 'F':
                        schedule_method = FCFS;
                        scheduler_to_print = "FCFS";
                        break;
                    case 'L':
                        schedule_method = LCFS;
                        scheduler_to_print = "LCFS";
                        break;
                    case 'S':
                        schedule_method = SRTF;
                        scheduler_to_print = "SRTF";
                        break;
                    case 'R':
                        schedule_method = RR;
                        scheduler_to_print = "RR ";
                        cmd = optarg + 1;
                        sscanf(cmd, "%d", &quantum);
                        break;
                    case 'P':
                        schedule_method = PRIO;
                        scheduler_to_print = "PRIO ";
                        cmd = optarg + 1;
                        sscanf(cmd, "%d:%d",&quantum,&maxprio);
                        break;
                    case 'E':
                        schedule_method = PREPRIO;
                        scheduler_to_print = "PREPRIO ";
                        cmd = optarg + 1;
                        sscanf(cmd, "%d:%d",&quantum,&maxprio);
                        break;
                }
                break;
            case 'v':
                cout<<"trying..."<<endl;
                break;
        }
    }
}
bool valid_input_file = false;
bool valid_rand_file = false;
string input_file_name = "null";
string random_file_name = "null";
void read_randfile(char* filename){
    ifstream file;
    file.open(filename);
    string newline;
    if(file.is_open()) {
        valid_rand_file = true;
        random_file_name = filename;
        getline(file, newline);
        while (getline(file, newline)){
            randvals.push_back(stoi(newline));
        }
        file.close();
    }
    else {
        valid_rand_file = false;
    }


};

void parseLine(string line, int pid, int maxprio) {
    stringstream ss(line);
    Process process;
    ss>>process.AT;
    ss>>process.TC;
    ss>>process.CB;
    ss>>process.IO;
    process.pid=pid;
    process.PRIO=myrandom(maxprio);
    process.DYNAMIC_PRIO= process.PRIO - 1;
    process.burstRemaining = 0;
    process.burstTemp = 0;
    process.totalRemaining = process.TC;
    process.state_ts = -1;
    process.IT = 0;
    process.CW =0;
    process.start_time = -1;
    processes.push_back(process);
}
void read_inputfile(char* filename, int maxprio){
    ifstream file;
    string newline;
    file.open(filename);
    if(file.is_open()) {
        valid_input_file = true;
        input_file_name = filename;
        int pid = 0;
        while(getline(file, newline)){
            parseLine(newline, pid, maxprio);
            pid++;
        }
        file.close();
    }
    else {
        valid_input_file = false;
    }
};

void calculate() {
    for(int i = 0; i < processes.size(); i++) {
        total_cpu_time += processes[i].TC;
        total_turnaround_time += processes[i].TT;
        total_cpu_wait += processes[i].CW;
        avg_turnaround_time = total_turnaround_time / (double)processes.size();
        avg_cpu_wait = total_cpu_wait / (double)processes.size();
        throughput = processes.size() / (double)total_time * 100.0;
    }
    cpu_util = 100.0 * total_cpu_time / (double)total_time;
    IO_util = 100.0 * des.IOT / (double)total_time;
}

void print() {
    cout<<scheduler_to_print;
    if (schedule_method == RR || schedule_method == PRIO || schedule_method == PREPRIO) {
        cout<<quantum<<endl;
    }
    else {
        cout<<endl;
    }
    calculate();
    for(int i = 0; i < processes.size(); i++) {
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",
               processes[i].pid,
               processes[i].AT,
               processes[i].TC,
               processes[i].CB,
               processes[i].IO,
               processes[i].PRIO,
               processes[i].FT,
               processes[i].TT,
               processes[i].IT,
               processes[i].CW);
    }
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
           total_time,
           cpu_util,
           IO_util,
           avg_turnaround_time,
           avg_cpu_wait,
           throughput);
}

int main(int argc, char* argv[]) {
    readCmd(argc, argv);
    read_randfile(argv[3]);
    read_inputfile(argv[2], maxprio);
    if(valid_input_file && valid_rand_file) {
        Simulation();
        print();
    }
    else if(!valid_input_file){
        cout<<"Not a valid inputfile <"<< input_file_name <<">"<<endl;
    }
    else if(!valid_rand_file) {
        cout<<"Not a valid random file <"<< random_file_name <<">"<<endl;
    }
    return 0;
}