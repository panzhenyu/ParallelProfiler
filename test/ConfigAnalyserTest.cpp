#include <iostream>
#include "ConfigFactory.hpp"
#include "ConfigAnalyser.hpp"

using namespace std;

int main() {
    Config cfg;
    cfg.parseFile("example.json");
    cout << cfg.getJson() << endl;
}