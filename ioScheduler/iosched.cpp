#include <iostream>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <stdlib.h>
#include <deque>
#include <limits.h>
#include <algorithm>

using namespace std;
struct Options {
    char algo;
};
struct IOS {
    int op_num = 0;
    int arrival_time = 0;
    int start_time = 0;
    int end_time = 0;
    int track = 0;
};
Options options;
void readCmd(int argc, char* argv[]) {
    getopt(argc, argv, "s:");
    options.algo = optarg[0];
    switch (options.algo) {
        case 'i':
            cout<<"i"<<endl;
            break;
        case 'j':
            cout<<"j"<<endl;
            break;
        case 's':
            cout<<"s"<<endl;
            break;
        case 'c':
            cout<<"c"<<endl;
            break;
        case 'f':
            cout<<"f"<<endl;
            break;
    }
}
void read_inputfile(char* fileName) {
    ifstream inputFile;
    inputFile.open(fileName);
    string line;
    int num = 0
    while(getline(inputFile, line)) {
        if(line[0] == '#') {
            continue;
        }
        IOS ios;
        stringstream ss(line);
        ss >> ios.arrival_time;
        ss >> ios.track;

    }
}
int main(int argc, char* argv[]) {
    readCmd(argc, argv);
    read_inputfile(argv[2]);
}