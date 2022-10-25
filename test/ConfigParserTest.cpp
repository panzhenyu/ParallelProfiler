#include <iostream>
#include "ConfigParser.hpp"

using namespace std;

int main() {
    ConfigParser parser;
    cout << "parse result: " << parser.parseFile("example.json") << endl;
}
