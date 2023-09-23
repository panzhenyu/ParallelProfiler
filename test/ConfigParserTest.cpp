#include <iostream>
#include "ConfigParser.hpp"
#include <time.h>
using namespace std;

int main() {
    ConfigParser parser;
    cout << "parse result: " << parser.parseFile("example.json") << endl;
}
