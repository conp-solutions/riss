
#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>    // std::sort
#include <assert.h>

#include "useful.h"


struct FamConfig {
    vector<float> satTimes;
    vector<float> unsatTimes;
    int uniqueContributions;
    int satContributions, unsatContributions;
    int totalInstances; // only for the overall family, but space is not a problem here ...
    FamConfig() : uniqueContributions(0), satContributions(0), unsatContributions(0), totalInstances(0) {}
};

inline bool sortfunction(const float& i, const float& j) { return (i < j); }

int main(int argc, char* argv[])
{
    if (argc != 6) {   // no arguments, then print the headline of a data file for the extracted data
        cerr << "usage: ./formatResults SAT.dat TIMES.dat FAM.dat CATEGORY.dat timeout" << endl;
        cerr << "structure of SAT.dat:" << endl
             << "first line: Instance ConfigName1 ... ConfigNameN" << endl
             << "other lines: path/CNF exitCode1 ... exitCodeN" << endl
             << endl;
        cerr << "structure of TIMES.dat:" << endl
             << "first line: Instance ConfigName1 ... ConfigNameN" << endl
             << "other lines: path/CNF runTime1 ... runTime1" << endl
             << "run times are float values for the number of seconds used to solve an instance" << endl
             << endl ;
        cerr << "structure of FAM.dat:" << endl
             << "each line: FAMILYNAME patterns to match in CNF path and instance name" << endl
             << "Note: once a pattern is hit, the other families are not tested any more!" << endl
             << endl ;
        cerr << "structure of CATEGORY.dat:" << endl
             << "each line: CATEGORY patterns to match in CNF path and instance name" << endl
             << "Note: an instance can be in multiple categories (e.g. industrial, and year 2013)" << endl
             << "Note: a leading ! excludes an instance from the category, if the following pattern is matched" << endl
             << "Note: a pattern with a § in the name, e.g. ABC$DEF$EFG is split into the three patterns ABC,DEF and EFG. Only added, if all patterns are matched" << endl
             << endl ;
        cerr << "timeout: number of seconds as timeout for a solver" << endl << endl;
        exit(1);
    }

    const int timeout = atoi(argv[5]);
    cerr << "c work with timeout " << timeout << endl;

    vector < string > configurations;

    enum state {
        unknown = 0,
        sat = 10,
        unsat = 20,
    };

    vector< string > satConfigs;
    vector< string > satFileNames;
    vector< int > fileState; // 0 = unknown,
    vector< string > timeConfigs;
    vector< vector<float> > runTime;
    vector < vector<string> > familyNames; // first field is name of the family, remaining fields are patterns to be matched
    vector < vector<string> > categoryNames; // first field is name of the family, remaining fields are patterns to be matched

    vector< vector<FamConfig> > familyConfigData;
    vector< vector<FamConfig> > categoryConfigData;

    // first, run over the files and collect the SAT

    ifstream fileptr(argv[1], ios_base::in);
    if (!fileptr) {
        cerr << "c ERROR cannot open file " << argv[1] << " for reading" << endl;
        exit(1);
    }

    string line;        // current line
    string field ;
    int lineNr = 0;
    // parse every single line
    while (getline(fileptr, line)) {
        lineNr ++;
        if (lineNr == 1) {   // extract configurations!
            int f = 0;

            do {
                field =  Get(line, f).c_str();
                f ++;
                if (f == 1 && field != "Instance") { cerr << "c first field should be <Instance>" << endl; exit(1);}
                if (f > 1 && field != "") { satConfigs.push_back(field.substr(field.find('.') + 1)); }
            } while (field != "");
            cerr << "c found " << f << " fields" << endl;

            for (int i = 0 ; i < satConfigs.size(); ++ i) {
                cerr << "c config[" << i << "] : " << satConfigs[i] << endl;
            }
            continue;
        }

        satFileNames.push_back(Get(line, 0));

        fileState.push_back(unknown);
        for (int i = 1; i <= satConfigs.size(); ++ i) {
            field = Get(line, i);
            if (field == "10")       { fileState[ fileState.size() - 1 ] =  sat;   break; }
            else if (field == "20") { fileState[ fileState.size() - 1 ] =  unsat; break; }
            //else if( i + 1 == satConfigs.size() ) { fileState.push_back( unknown ); break; }
        }
    }

    cerr << "c found " << satFileNames.size() << " files with " << fileState.size() << " states" << endl;
    fileptr.close();


    cerr << endl << "c load family information ... " << endl;

    fileptr.open(argv[3], ios_base::in);
    if (!fileptr) {
        cerr << "c ERROR cannot open file " << argv[3] << " for reading" << endl;
        exit(1);
    }

    lineNr = 0;
    // parse every single line
    while (getline(fileptr, line)) {
        field = Get(line, 0);
        if (field == "") { continue; }   // jump over empty line!
        familyNames.push_back(vector<string>());

        vector<string>& last = familyNames[familyNames.size() - 1];
        last.push_back(field);

        int f = 1;
        do {
            field = Get(line, f);
            if (field != "") { last.push_back(field); }
            f++;
        } while (field != "");
        if (last.size() == 1) {
            cerr << "c drop family " << last[0] << endl;
            familyNames.pop_back();
        }
    }
    fileptr.close();

    cerr << "c found " << familyNames.size() << " families" << endl;
    familyNames.push_back(vector<string>());
    familyNames[ familyNames.size() - 1]. push_back(string("remaining"));

    familyConfigData.resize(familyNames.size() + 1);   // have one more family with the name "remaining"

    // reserve space for all configurations and families!
    for (int i = 0 ; i < familyConfigData.size(); ++ i) {
        familyConfigData[i].resize(satConfigs.size() + 1);   // first one is generic one!
    }

    cerr << endl << "c load category information ... " << endl;

    fileptr.open(argv[4], ios_base::in);
    if (!fileptr) {
        cerr << "c ERROR cannot open file " << argv[4] << " for reading" << endl;
        exit(1);
    }

    lineNr = 0;
    // parse every single line
    while (getline(fileptr, line)) {
        field = Get(line, 0);
        if (field == "") { continue; }   // jump over empty line!
        categoryNames.push_back(vector<string>());

        vector<string>& last = categoryNames[categoryNames.size() - 1];
        last.push_back(field);

        int f = 1;
        do {
            field = Get(line, f);
            if (field != "") { last.push_back(field); }
            f++;
        } while (field != "");
        if (last.size() == 1) {
            cerr << "c drop category " << last[0] << endl;
            categoryNames.pop_back();
        }
    }
    fileptr.close();

    cerr << "c found " << categoryNames.size() << " categories" << endl;
    categoryNames.push_back(vector<string>());
    categoryNames[ categoryNames.size() - 1]. push_back(string("remaining"));

    categoryConfigData.resize(categoryNames.size());   // have one more family with the name "remaining"

    // reserve space for all configurations and families!
    for (int i = 0 ; i < categoryConfigData.size(); ++ i) {
        categoryConfigData[i].resize(satConfigs.size() + 1);   // first one is generic one!
    }

    cerr << endl << "c use time information ... " << endl;

    fileptr.open(argv[2], ios_base::in);
    if (!fileptr) {
        cerr << "c ERROR cannot open file " << argv[2] << " for reading" << endl;
        exit(1);
    }

    lineNr = 0;
    // parse every single line
    vector<int> categoryIDs;
    while (getline(fileptr, line)) {
        lineNr ++;
        if (lineNr == 1) {   // extract configurations!
            int f = 0;

            do {
                field =  Get(line, f).c_str();
                f ++;
                if (f == 1 && field != "Instance") { cerr << "c first field should be <Instance>" << endl; exit(1);}
                if (f > 1 && field != "") if (field.substr(field.find('.') + 1) != satConfigs[f - 2]) { cerr << "wrong config at " << f << ": " << field.substr(field.find('.') + 1) << " vs " << satConfigs[f - 2] << " in " << argv[1] << endl; exit(1); }
            } while (field != "");
            cerr << "c found " << f << " fields" << endl;
            continue;
        }

        field = Get(line, 0);
        if (field != satFileNames[lineNr - 2]) { cerr << "c at line " << lineNr << " is a wrong file: " << field << " vs " << satFileNames[lineNr - 2] << endl; exit(1); }

        // find correct family
        int familyID = -1;
        for (int i = 0 ; i < familyNames.size(); ++ i) {
            const vector<string>& fam = familyNames[i];
            for (int j = 1; j < fam.size(); ++ j) {
                if (field.find(fam[j]) != string::npos) {
                    familyID = i;
                    break;
                }
            }
            if (familyID != -1) { break; }   // found a family;
        }
        if (familyID == -1) { familyID = familyNames.size() - 1 ; }   // put into remaining family


        // add instance to the selected family
        familyConfigData[familyID][0].totalInstances ++;
        if (fileState[ lineNr - 2 ] == unsat) { familyConfigData[familyID][0].unsatTimes.push_back(0); }
        else if (fileState[ lineNr - 2 ] == sat) { familyConfigData[familyID][0].satTimes.push_back(0); }
//       totalInstances


        // for each configuration collect the run time and put it into the correct family
        int uniqueConfig = -1; // id of config that is unique contribution ...
        for (int i = 1; i <= satConfigs.size(); ++ i) {
            field = Get(line, i);
            if (field != "-") {
                if (fileState[ lineNr - 2 ] == unknown) { cerr << "c instance on line " << lineNr << " is unknown, although its solved by config " << i << " ..." ; assert(false); exit(1); }
                float runTime = atoi(field.c_str());
                if (fileState[ lineNr - 2 ] == unsat) { familyConfigData[familyID][i].unsatTimes.push_back(runTime); }
                else if (fileState[ lineNr - 2 ] == sat) { familyConfigData[familyID][i].satTimes.push_back(runTime); }
                if (uniqueConfig == -1) { uniqueConfig = i; }   // first config that solved the instance
                else if (uniqueConfig > 0) { uniqueConfig = -2; }   // there is more than one instance
            }
        }
        if (uniqueConfig >= 0) {
            familyConfigData[familyID][uniqueConfig].uniqueContributions ++; // store unique contributions!
            if (fileState[ lineNr - 2 ] == unsat) { familyConfigData[familyID][uniqueConfig].unsatContributions ++; }
            else if (fileState[ lineNr - 2 ] == sat) { familyConfigData[familyID][uniqueConfig].satContributions ++; }
        }

        // find correct categories
        field = Get(line, 0);
        categoryIDs.clear();
        for (int i = 0 ; i < categoryNames.size(); ++ i) {
            const vector<string>& category = categoryNames[i];
            bool addToCategory = false;
            for (int j = 1; j < category.size(); ++ j) {
                if (category[j][0] == '!') {   // exclude, if this string is found!
                    cerr << "c exclude-string: " << category[j].substr(1) << endl;
                    if (field.find(category[j].substr(1)) != string::npos) {
                        addToCategory = false;
                        break;
                    }
                }

                if (category[j].find('$') == string::npos) {
                    // use instance in this category, if the simple pattern is found
                    if (field.find(category[j]) != string::npos) {
                        addToCategory = true;
                    }
                } else { // split the complex pattern, and look for all patterns!
                    bool foundAll = true;
                    stringstream ss(category[j]);
                    string item;
                    while (getline(ss, item, (char)'$')) {
                        if (field.find(item) == string::npos) { foundAll = false; break; }
                    }
                    if (foundAll) { addToCategory = true; }
                }
            }
            if (addToCategory) { categoryIDs.push_back(i); }
        }

        if (categoryIDs.size() == 0) { categoryIDs.push_back(categoryConfigData.size() - 1); }     // put into remaining category

        // process the whole line one more time
        for (int j = 0; j < categoryIDs.size(); ++ j) {
            // add instance to the selected family
            int categoryID = categoryIDs[j];
            categoryConfigData[categoryID][0].totalInstances ++;
            if (fileState[ lineNr - 2 ] == unsat) { categoryConfigData[categoryID][0].unsatTimes.push_back(0); }
            else if (fileState[ lineNr - 2 ] == sat) { categoryConfigData[categoryID][0].satTimes.push_back(0); }

            // for each configuration collect the run time and put it into the correct family
            int uniqueConfig = -1; // id of config that is unique contribution ...
            for (int i = 1; i <= satConfigs.size(); ++ i) {
                field = Get(line, i);
                if (field != "-") {
                    if (fileState[ lineNr - 2 ] == unknown) { cerr << "c instance on line " << lineNr << " is unknown, although its solved by config " << i << " ..." ; exit(1); }
                    float runTime = atoi(field.c_str());
                    if (fileState[ lineNr - 2 ] == unsat) { categoryConfigData[categoryID][i].unsatTimes.push_back(runTime); }
                    else if (fileState[ lineNr - 2 ] == sat) { categoryConfigData[categoryID][i].satTimes.push_back(runTime); }
                    if (uniqueConfig == -1) { uniqueConfig = i; }   // first config that solved the instance
                    else if (uniqueConfig > 0) { uniqueConfig = -2; }   // there is more than one instance
                }
            }
            if (uniqueConfig >= 0) {
                categoryConfigData[categoryID][uniqueConfig].uniqueContributions ++; // store unique contributions!
                if (fileState[ lineNr - 2 ] == unsat) { categoryConfigData[categoryID][uniqueConfig].unsatContributions ++; }
                else if (fileState[ lineNr - 2 ] == sat) { categoryConfigData[categoryID][uniqueConfig].satContributions ++; }
            }
        }
    }
    fileptr.close();
    vector<float> times;


    // collect information about all families into on overall family:
    vector< FamConfig > allInstances(familyConfigData[0].size());
    for (int f = 0 ; f < familyConfigData.size(); ++ f) {
        // print the info "nicely" ...
        vector< FamConfig >& fam = familyConfigData[f];
        for (int i = 0; i < fam.size(); ++ i) {
            for (int j = 0; j < fam[i].satTimes.size(); ++ j) {
                allInstances[i].satTimes.push_back(fam[i].satTimes[j]) ;
            }
            for (int j = 0; j < fam[i].unsatTimes.size(); ++ j) {
                allInstances[i].unsatTimes.push_back(fam[i].unsatTimes[j]) ;
            }
            allInstances[i].totalInstances += fam[i].totalInstances;
            allInstances[i].satContributions += fam[i].satContributions;
            allInstances[i].unsatContributions += fam[i].unsatContributions;
        }
    }

    cerr << "c print cumulated information ... " << endl;
    cout << endl << endl << "\\textbf{ALL INSTANCES}\\\\" << endl;
    cout << "\\begin{tabular}{c ";
    for (int i = 0; i < satConfigs.size(); ++ i) { cout << " @{\\hspace{1ex}} c " ; }
    cout << "}" << endl;
    cout << "\\hline" << endl;
    cout << "Family (instances) ";

    for (int i = 0; i < satConfigs.size(); ++ i) { cout << " & " << satConfigs[i]; }
    cout << "  \\\\" << endl;

    cout << "\\hline" << endl;
    {
        // print the info "nicely" ...
        vector< FamConfig >& category = allInstances;

        // first line
        cout << "  ALLINSTANCES  (" << category[0].totalInstances << ")" ;

        // select best configurations
        int bestUnique = 1, bestSATUnique = 1, bestUNSATUnique = 1, bestSat = 1, bestUnsat = 1, bestTotal = 1;
        for (int i = 2; i < category.size(); ++ i) {
            if (category[i].satTimes.size() > category[ bestSat].satTimes.size()) { bestSat = i; }
            if (category[i].unsatTimes.size() > category[ bestUnsat].unsatTimes.size()) { bestUnsat = i; }
            if (category[i].satTimes.size() + category[i].unsatTimes.size()  > category[ bestTotal].satTimes.size() + category[ bestTotal].unsatTimes.size()) { bestTotal = i; }
            if (category[i].uniqueContributions > category[ bestUnique ].uniqueContributions) { bestUnique = i; }
            if (category[i].satContributions > category[ bestSATUnique].satContributions) { bestSATUnique = i; }
            if (category[i].unsatContributions > category[ bestUNSATUnique].unsatContributions) { bestUNSATUnique = i; }
        }


        // print information
        for (int i = 1; i < category.size(); ++ i) {
            cout << " & ";
            bool boldTotal = category[ bestTotal].satTimes.size() + category[ bestTotal].unsatTimes.size() != 0 && (category[i].satTimes.size() + category[i].unsatTimes.size() == category[ bestTotal].satTimes.size() + category[ bestTotal].unsatTimes.size());
            bool boldUnique = category[i].uniqueContributions != 0 && (category[i].uniqueContributions == category[ bestUnique].uniqueContributions);
            bool boldSATUnique   = category[i].satContributions != 0 && (category[i].satContributions == category[ bestSATUnique].satContributions);
            bool boldUNSATUnique = category[i].unsatContributions != 0 && (category[i].unsatContributions == category[ bestUNSATUnique].unsatContributions);
            cout << " "
                 << (boldTotal ? "\\textbf{" : "")
                 << category[i].satTimes.size() + category[i].unsatTimes.size()
                 << (boldTotal ? "}" : "")
                 << " / "
                 << (boldUnique ? "\\textbf{" : "")
                 << category[i].uniqueContributions
                 << (boldUnique ? "}" : "")
                 << " /s "
                 << (boldSATUnique ? "\\textbf{" : "")
                 << category[i].satContributions
                 << (boldSATUnique ? "}" : "")
                 << " /u "
                 << (boldUNSATUnique ? "\\textbf{" : "")
                 << category[i].unsatContributions
                 << (boldUNSATUnique ? "}" : "")
                 << " ";
        }
        cout << "\\\\" << endl;


        // second line
        cout << " s:" << category[0].satTimes.size() << " " ;
        // print information
        for (int i = 1; i < category.size(); ++ i) {
            cout << " & ";
            bool boldSat   = category[i].satTimes.size() != 0 && (category[i].satTimes.size()   == category[ bestSat].satTimes.size());
            bool boldUnsat = category[i].unsatTimes.size() != 0 && (category[i].unsatTimes.size() == category[ bestUnsat].unsatTimes.size());
            cout << " "
                 << (boldSat ? "\\textbf{" : "")
                 << category[i].satTimes.size()
                 << (boldSat ? "}" : "")
                 ;
        }
        cout << "\\\\" << endl;

        // second and a half line :)
        cout << " u:" << category[0].unsatTimes.size() << " " ;
        // print information
        for (int i = 1; i < category.size(); ++ i) {
            cout << " & ";
            bool boldSat   = category[i].satTimes.size() != 0 && (category[i].satTimes.size()   == category[ bestSat].satTimes.size());
            bool boldUnsat = category[i].unsatTimes.size() != 0 && (category[i].unsatTimes.size() == category[ bestUnsat].unsatTimes.size());
            cout << " "
                 << (boldUnsat ? "\\textbf{" : "")
                 << category[i].unsatTimes.size() << " "
                 << (boldUnsat ? "}" : "")
                 ;
        }
        cout << "\\\\" << endl;

        // third line
        cout << " MEDIAN " ;
        // print information
        for (int i = 1; i < category.size(); ++ i) {
            times.clear();
            for (int j = 0 ; j < category[i].satTimes.size(); ++ j) { times.push_back(category[i].satTimes[j]); }
            for (int j = 0 ; j < category[i].unsatTimes.size(); ++ j) { times.push_back(category[i].unsatTimes[j]); }
            while (times.size() <  allInstances[0].totalInstances) { times.push_back(timeout); }
            sort(times.begin(), times.end(), sortfunction);
            cout << " & ";
            if (times.size() > 0) { cout << " " << times[ times.size() / 2 ]; }
            else { cout << " -"; }
        }
        cout << "\\\\" << endl;

        // fourth line
        cout << " PAR10 " ;
        // print information
        for (int i = 1; i < category.size(); ++ i) {
            float pten = 0;
            for (int j = 0 ; j < category[i].satTimes.size(); ++ j) { pten += category[i].satTimes[j]; }
            for (int j = 0 ; j < category[i].unsatTimes.size(); ++ j) { pten += category[i].unsatTimes[j]; }
            pten += 10 * timeout * (allInstances[0].totalInstances - category[i].satTimes.size() - category[i].unsatTimes.size());   // add penalty for timed out instances!
            cout << " & ";
            cout << " "
                 << pten
                 ;
        }
        cout << "\\\\" << endl;

        cout << "\\hline" << endl;
    }

    cout << "\\end{tabular}" << endl;

    cerr << "c print cumulated CATEGORY information ... " << endl;
    cout << endl << endl << "\\textbf{CATEGORIES}\\\\" << endl;
    cout << "\\begin{tabular}{c ";
    for (int i = 0; i < satConfigs.size(); ++ i) { cout << " @{\\hspace{1ex}} c " ; }
    cout << "}" << endl;
    cout << "\\hline" << endl;
    cout << "Category (instances) ";

    for (int i = 0; i < satConfigs.size(); ++ i) { cout << " & " << satConfigs[i]; }
    cout << "  \\\\" << endl;

    cout << "\\hline" << endl;

    for (int f = 0 ; f < categoryConfigData.size(); ++ f) {
        // print the info "nicely" ...
        vector< FamConfig >& category = categoryConfigData[f];
        // first line
        cout << " " << categoryNames[f][0] << " (" << category[0].totalInstances << ")" ;

        // select best configurations
        int bestUnique = 1, bestSATUnique = 1, bestUNSATUnique = 1, bestSat = 1, bestUnsat = 1, bestTotal = 1;
        for (int i = 2; i < category.size(); ++ i) {
            if (category[i].satTimes.size() > category[ bestSat].satTimes.size()) { bestSat = i; }
            if (category[i].unsatTimes.size() > category[ bestUnsat].unsatTimes.size()) { bestUnsat = i; }
            if (category[i].satTimes.size() + category[i].unsatTimes.size()  > category[ bestTotal].satTimes.size() + category[ bestTotal].unsatTimes.size()) { bestTotal = i; }
            if (category[i].uniqueContributions > category[ bestUnique].uniqueContributions) { bestUnique = i; }
            if (category[i].satContributions > category[ bestSATUnique].satContributions) { bestSATUnique = i; }
            if (category[i].unsatContributions > category[ bestUNSATUnique].unsatContributions) { bestUNSATUnique = i; }
        }


        // print information
        for (int i = 1; i < category.size(); ++ i) {
            cout << " & ";
            bool boldTotal = category[ bestTotal].satTimes.size() + category[ bestTotal].unsatTimes.size() != 0 && (category[i].satTimes.size() + category[i].unsatTimes.size() == category[ bestTotal].satTimes.size() + category[ bestTotal].unsatTimes.size());
            bool boldUnique = category[i].uniqueContributions != 0 && (category[i].uniqueContributions == category[ bestUnique].uniqueContributions);
            bool boldSATUnique   = category[i].satContributions != 0 && (category[i].satContributions == category[ bestSATUnique].satContributions);
            bool boldUNSATUnique = category[i].unsatContributions != 0 && (category[i].unsatContributions == category[ bestUNSATUnique].unsatContributions);
            cout << " "
                 << (boldTotal ? "\\textbf{" : "")
                 << category[i].satTimes.size() + category[i].unsatTimes.size()
                 << (boldTotal ? "}" : "")
                 << " / "
                 << (boldUnique ? "\\textbf{" : "")
                 << category[i].uniqueContributions
                 << (boldUnique ? "}" : "")
                 << " /s "
                 << (boldSATUnique ? "\\textbf{" : "")
                 << category[i].satContributions
                 << (boldSATUnique ? "}" : "")
                 << " /u "
                 << (boldUNSATUnique ? "\\textbf{" : "")
                 << category[i].unsatContributions
                 << (boldUNSATUnique ? "}" : "")
                 << " ";
        }
        cout << "\\\\" << endl;


        // second line
        cout << " s:" << category[0].satTimes.size() << " " ;
        // print information
        for (int i = 1; i < category.size(); ++ i) {
            cout << " & ";
            bool boldSat   = category[i].satTimes.size() != 0 && (category[i].satTimes.size()   == category[ bestSat].satTimes.size());
            bool boldUnsat = category[i].unsatTimes.size() != 0 && (category[i].unsatTimes.size() == category[ bestUnsat].unsatTimes.size());
            cout << " "
                 << (boldSat ? "\\textbf{" : "")
                 << category[i].satTimes.size()
                 << (boldSat ? "}" : "")
                 ;
        }
        cout << "\\\\" << endl;

        cout << " u:" << category[0].unsatTimes.size() << " " ;
        // print information
        for (int i = 1; i < category.size(); ++ i) {
            cout << " & ";
            bool boldSat   = category[i].satTimes.size() != 0 && (category[i].satTimes.size()   == category[ bestSat].satTimes.size());
            bool boldUnsat = category[i].unsatTimes.size() != 0 && (category[i].unsatTimes.size() == category[ bestUnsat].unsatTimes.size());
            cout << " "
                 << (boldUnsat ? "\\textbf{" : "")
                 << category[i].unsatTimes.size()
                 << (boldUnsat ? "}" : "")
                 << " "
                 ;
        }
        cout << "\\\\" << endl;

        // third line
        cout << " MEDIAN " ;
        // print information
        for (int i = 1; i < category.size(); ++ i) {
            times.clear();
            for (int j = 0 ; j < category[i].satTimes.size(); ++ j) { times.push_back(category[i].satTimes[j]); }
            for (int j = 0 ; j < category[i].unsatTimes.size(); ++ j) { times.push_back(category[i].unsatTimes[j]); }
            while (times.size() <  categoryConfigData[f][0].totalInstances) { times.push_back(timeout); }
            sort(times.begin(), times.end(), sortfunction);
            cout << " & ";
            if (times.size() > 0) { cout << " " << times[ times.size() / 2 ]; }
            else { cout << " -"; }
        }
        cout << "\\\\" << endl;

        // fourth line
        cout << " PAR10 " ;
        // print information
        for (int i = 1; i < category.size(); ++ i) {
            float pten = 0;
            for (int j = 0 ; j < category[i].satTimes.size(); ++ j) { pten += category[i].satTimes[j]; }
            for (int j = 0 ; j < category[i].unsatTimes.size(); ++ j) { pten += category[i].unsatTimes[j]; }
            pten += 10 * timeout * (categoryConfigData[f][0].totalInstances - category[i].satTimes.size() - category[i].unsatTimes.size());   // add penalty for timed out instances!
            cout << " & ";
            cout << " "
                 << pten
                 ;
        }
        cout << "\\\\" << endl;

        cout << "\\hline" << endl;
    }

    cout << "\\end{tabular}" << endl;



    cerr << "c print cumulated FAMILY information ... " << endl;
    cout << endl << endl << "\\textbf{FAMILIES}\\\\" << endl;
    cout << "\\begin{tabular}{c ";
    for (int i = 0; i < satConfigs.size(); ++ i) { cout << " @{\\hspace{1ex}} c " ; }
    cout << "}" << endl;
    cout << "\\hline" << endl;
    cout << "Family (instances) ";

    for (int i = 0; i < satConfigs.size(); ++ i) { cout << " & " << satConfigs[i]; }
    cout << "  \\\\" << endl;

    cout << "\\hline" << endl;

    int printed = 0;

    for (int f = 0 ; f < familyConfigData.size(); ++ f) {
        // print the info "nicely" ...
        vector< FamConfig >& fam = familyConfigData[f];
        if (fam[0].totalInstances < 5 || fam[0].satTimes.size() + fam[0].unsatTimes.size() < 5) { continue; }  // jump over this family

        printed ++;

        if (printed % 15 == 0) {   // have another table!
            cout << "\\end{tabular}" << endl << endl;
            cout << "\\begin{tabular}{c ";
            for (int i = 0; i < satConfigs.size(); ++ i) { cout << " @{\\hspace{1ex}} c " ; }
            cout << "}" << endl;
            cout << "\\hline" << endl;
            cout << "Family (instances) ";
            for (int i = 0; i < satConfigs.size(); ++ i) { cout << " & " << satConfigs[i]; }
            cout << "  \\\\" << endl;
            cout << "\\hline" << endl;
        }
        // first line
        cout << " " << familyNames[f][0] << " (" << fam[0].totalInstances << ")" ;

        // select best configurations
        int bestUnique = 1, bestSATUnique = 1, bestUNSATUnique = 1, bestSat = 1, bestUnsat = 1, bestTotal = 1;
        for (int i = 2; i < fam.size(); ++ i) {
            if (fam[i].satTimes.size() > fam[ bestSat].satTimes.size()) { bestSat = i; }
            if (fam[i].unsatTimes.size() > fam[ bestUnsat].unsatTimes.size()) { bestUnsat = i; }
            if (fam[i].satTimes.size() + fam[i].unsatTimes.size()  > fam[ bestTotal].satTimes.size() + fam[ bestTotal].unsatTimes.size()) { bestTotal = i; }
            if (fam[i].uniqueContributions > fam[ bestUnique].uniqueContributions) { bestUnique = i; }
            if (fam[i].satContributions > fam[ bestSATUnique].satContributions) { bestSATUnique = i; }
            if (fam[i].unsatContributions > fam[ bestUNSATUnique].unsatContributions) { bestUNSATUnique = i; }
        }
        // print information
        for (int i = 1; i < fam.size(); ++ i) {
            cout << " & ";
            bool boldTotal = (fam[i].satTimes.size() + fam[i].unsatTimes.size() == fam[ bestTotal].satTimes.size() + fam[ bestTotal].unsatTimes.size());
            bool boldUnique = fam[i].uniqueContributions == fam[ bestUnique].uniqueContributions;
            bool boldSATUnique   = fam[i].satContributions != 0 && (fam[i].satContributions == fam[ bestSATUnique].satContributions);
            bool boldUNSATUnique = fam[i].unsatContributions != 0 && (fam[i].unsatContributions == fam[ bestUNSATUnique].unsatContributions);
            cout << " "
                 << (boldTotal ? "\\textbf{" : "")
                 << fam[i].satTimes.size() + fam[i].unsatTimes.size()
                 << (boldTotal ? "}" : "")
                 << " / "
                 << (boldUnique ? "\\textbf{" : "")
                 << fam[i].uniqueContributions
                 << (boldUnique ? "}" : "")
                 << " /s "
                 << (boldSATUnique ? "\\textbf{" : "")
                 << fam[i].satContributions
                 << (boldSATUnique ? "}" : "")
                 << " /u "
                 << (boldUNSATUnique ? "\\textbf{" : "")
                 << fam[i].unsatContributions
                 << (boldUNSATUnique ? "}" : "")
                 << " ";
        }
        cout << "\\\\" << endl;


        // second line
        cout << " s:" << fam[0].satTimes.size() << " u:" << fam[0].unsatTimes.size() << " " ;
        // print information
        for (int i = 1; i < fam.size(); ++ i) {
            cout << " & ";
            bool boldSat   = fam[i].satTimes.size()   == fam[ bestSat].satTimes.size();
            bool boldUnsat = fam[i].unsatTimes.size() == fam[ bestUnsat].unsatTimes.size();
            cout << " "
                 << (boldSat ? "\\textbf{" : "")
                 << fam[i].satTimes.size()
                 << (boldSat ? "}" : "")
                 << " / "
                 << (boldUnsat ? "\\textbf{" : "")
                 << fam[i].unsatTimes.size() << " "
                 << (boldUnsat ? "}" : "")
                 ;
        }
        cout << "\\\\" << endl;

        // third line
        cout << " MEDIAN " ;
        // print information
        for (int i = 1; i < fam.size(); ++ i) {
            float pten = 0;
            times.clear();
            for (int j = 0 ; j < fam[i].satTimes.size(); ++ j) { times.push_back(fam[i].satTimes[j]); }
            for (int j = 0 ; j < fam[i].unsatTimes.size(); ++ j) { times.push_back(fam[i].unsatTimes[j]); }
            while (times.size() <  familyConfigData[f][0].totalInstances) { times.push_back(timeout); }
            sort(times.begin(), times.end(), sortfunction);
            cout << " & ";
            if (times.size() > 0) { cout << " " << times[ times.size() / 2 ]; }
            else { cout << " -"; }
        }
        cout << "\\\\" << endl;

        // fourth line
        cout << " PAR10 " ;
        // print information
        for (int i = 1; i < fam.size(); ++ i) {
            float pten = 0;
            for (int j = 0 ; j < fam[i].satTimes.size(); ++ j) { pten += fam[i].satTimes[j]; }
            for (int j = 0 ; j < fam[i].unsatTimes.size(); ++ j) { pten += fam[i].unsatTimes[j]; }
            pten += 10 * timeout * (familyConfigData[f][0].totalInstances - fam[i].satTimes.size() - fam[i].unsatTimes.size());   // add penalty for timed out instances!
            cout << " & ";
            cout << " "
                 << pten
                 ;
        }
        cout << "\\\\" << endl;

        cout << "\\hline" << endl;
    }

    cout << "\\end{tabular}" << endl;

    return 0;

}
