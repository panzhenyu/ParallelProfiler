#include "Process.hpp"
#include <vector>
#include <iostream>
#include <sys/wait.h>

using namespace std;

int setup() {
    cout << "setup" << endl;
    return 0;
}

int main() {
    Process proc;
    vector<string> args = {"/bin/ls", "-l"};
    proc.start(&setup, args);
    waitpid(proc.getPid(), 0, 0);
    proc.start(setup);
    return 0;
}