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
    pid_t pid;
    Process proc;
    vector<string> args = {"/bin/ls", "-l"};
    pid = proc.start(&setup, args);
    waitpid(pid, 0, 0);
    proc.start(setup);
    return 0;
}