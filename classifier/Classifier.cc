/*
 * Classifier.cpp
 *
 *  Created on: Dec 23, 2013
 *      Author: gardero
 */

#include "classifier/Classifier.h"
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include "riss/utils/System.h"
#include <iostream>
#include <ctime>

using namespace std;

Classifier::Classifier(Configurations& config, const char* prefix): configurations(config), correctCount(0) , nonZero(false)
{
    mode = RNDFOREST;
    gainThreshold = 0;
    verbose = 0;
    classifyTime = 5;
    timeout = 900;
    useTempFiles = false;
    wekaLocation = "/usr/share/java/weka.jar";
    this->prefix = prefix;
    for (int config = 0; config < configurations.getSize(); config++) {
        stringstream scmd;
        scmd << prefix << configurations.getNames()[config];
        scmd << ".model";
        classifiersNames.push_back(scmd.str());
    }
}

Classifier::~Classifier()
{
    // TODO Auto-generated destructor stub
}

/** print elements of a vector */
template <typename T>
inline std::ostream& operator<<(std::ostream& other, const std::vector<T>& data)
{
    for (int i = 0 ; i < data.size(); ++ i) {
        other << " " << data[i];
    }
    return other;
}

double getProb(string line)
{
    istringstream split(line);
    string word;
    bool positive = true; // positive
    int index = 0;
    do {
        split >> word;
        if (index == 2 && word.compare("2:bad") == 0) {
            positive = false;
        }
        index++;
    } while (split);
    double p = atof(word.c_str());
    return positive ? p : 1 - p;
}

bool isCorrect(string line)
{
    return line.find("+") == string::npos;
}

string runandoutput(string cmd)
{

    cerr << "c run and output, current used memory: " << Riss::memUsed() << " MB, peak: " << Riss::memUsedPeak() << " MB" << endl;
    string data;
    FILE * stream;
    const int max_buffer = 256;
    char buffer[max_buffer];
    cmd.append(" 2>&1");

    stream = popen(cmd.c_str(), "r");
    cerr << "c error after popen: " << endl;
    perror("popen");

    if (stream) {
        cerr << "c while reading from command call, current used memory: " << Riss::memUsed() << " MB, peak: " << Riss::memUsedPeak() << " MB" << endl;
        while (!feof(stream))
            if (fgets(buffer, max_buffer, stream) != nullptr) {
                data.append(buffer);
            }
        pclose(stream);
    }
    cerr << "c after command call, current used memory: " << Riss::memUsed() << " MB, peak: " << Riss::memUsedPeak() << " MB" << endl;

    return data;
}

vector<int>& Classifier::classifyJava(const char* wekaFile)
{
    cerr << "c call java, current used memory: " << Riss::memUsed() << " MB, peak: " << Riss::memUsedPeak() << " MB" << endl;
    stringstream scmd;
    // -cp "./predictor.jar":"/usr/share/java/weka.jar" predicor.Predictor
    //scmd << "java -Xmx1600M -cp \""<< predictorLocation.c_str() << "\":\"" << wekaLocation.c_str() << "\" predictor.Predictor ";
    scmd << "java -Xms100M -Xmx1G -cp \"" << predictorLocation.c_str() << "\":\"" << wekaLocation.c_str() << "\" predictor.Predictor ";
    //scmd << "java -cp \""<< predictorLocation.c_str() << "\":\"" << wekaLocation.c_str() << "\" predictor.Predictor ";
    scmd << "\"" << wekaFile << "\" ";
    scmd << "\"" << prefix << "\" ";
    scmd << "\"" << configurations.getDefinitionsFilename() << "\" ";
    scmd << "\"" << (nonZero ? "true" : "false") << "\" ";
    scmd << " | grep \"@classIndex\"";

    if (verbose > 0) {
        cerr << "c call to weka: " << scmd.str().c_str() << endl;
    }

    string s = runandoutput(scmd.str().c_str());

    if (verbose > 0) {
        cerr << "c output: " << s.c_str() << endl;
    }

    istringstream fin(s);
    //fin.open(name.str().c_str(),istream::in);
    classification.clear();
    string line;
    if (getline(fin, line)) {
        istringstream split(line);
        string word;
        split >> word; // @classIndex
        int index;
        split >> index; // index
        classification.push_back(index);
    }
    return classification;
}

vector<int>& Classifier::classify(const char* wekaFile)
{
    classification.clear();
    correct.clear();
    probabilities.clear();
    correctCount = 0;
    vector<double> tprobs;
    vector<bool> tcorrect;
    configurations.loadConfigurationInfo();
    for (int config = 0; config < configurations.getSize(); config++) {
        stringstream scmd;
        int index = configurations.getClassIndexes()[config];
        //scmd << "java -Xmx1600M -cp \"" << wekaLocation << "\" weka.classifiers.meta.FilteredClassifier -l " << classifiersNames[config];
        scmd << "java -Xms100M -Xmx1G -cp \"" << wekaLocation << "\" weka.classifiers.meta.FilteredClassifier -l " << classifiersNames[config];
        //scmd << "java -cp \"" << wekaLocation << "\" weka.classifiers.meta.FilteredClassifier -l " << classifiersNames[config];
        scmd << " -T " << wekaFile;
        scmd << " -c " << index;
        stringstream name;
        if (useTempFiles) {
            name << "/tmp/blackbox_" << (int) getpid() << ".results";
            scmd << " -p 0 | grep \":\" > " << name.str();
        } else {
            scmd << " -p 0 | grep \":\"";
        }

        if (verbose > 0) { cerr << "c call to weka: " << scmd.str().c_str() << endl; }
        if (useTempFiles) {
            int returnValue = system(scmd.str().c_str());
            if (verbose > 0)
                cerr << "c weka called resulted in exit code " << returnValue
                     << endl;
            if (returnValue != 0) { // return empty set of chosen classes!
                if (verbose > 0) {
                    cerr << "c WARNING: weka call failed" << endl;
                    cerr << "c weka called resulted in exit code "
                         << returnValue << endl;
                    cerr << "c faulty call: " << scmd.str().c_str() << endl;
                }
                remove(name.str().c_str());
                classification.clear();
                return classification;
            }
        }
        string s;
        if (useTempFiles) {
            std::ifstream t(name.str().c_str());
            std::stringstream buffer;
            buffer << t.rdbuf();
            s = buffer.str();
            t.close();
            remove(name.str().c_str());
        } else {
            s = runandoutput(scmd.str().c_str());
        }
        istringstream fin(s);
        //fin.open(name.str().c_str(),istream::in);
        string line;
        tprobs.clear();
        tcorrect.clear();
        for (int row = 0; getline(fin, line); row++) {
//        cerr << "c next line " << line << endl;
            tprobs.push_back(getProb(line));
            tcorrect.push_back(isCorrect(line));
        }
        if (config == 0) {
            for (int i = 0; i < tprobs.size(); i++) {
                probabilities.push_back(tprobs[i]);
                classification.push_back(0);
                correct.push_back(tcorrect[i]);
            }
        } else {
            for (int i = 0; i < probabilities.size(); ++i) {
                if (probabilities[i] < tprobs[i]) {
                    probabilities[i] = tprobs[i];
                    classification[i] = config;
                    correct[i] = tcorrect[i];
                }
            }
        }

        if (verbose > 0) {
            if (tprobs.size() == 0) { cerr << "c probability for " << configurations.getNames()[config] << " is " << 0 << endl; }
            else { cerr << "c probability for " << configurations.getNames()[config] << " is " << tprobs[0] << " all probs: " << probabilities << endl; }
        }

    }
    for (int i = 0; i < correct.size(); ++i) {
        if (correct[i]) {
            correctCount++;
        }
    }
    // check whether there is a class that has a higher propability than 0!
    if (nonZero) {
        bool foundNonZero = false;
        for (int i = 0 ; i < probabilities.size(); ++i) if (probabilities[i] != 0) { foundNonZero = true; break; }
        if (!foundNonZero) { classification.clear(); }   // if the propability for all classes is '0', then do not select a single class!
    }
    return classification;
}

int getTime()
{
    return time(0) * 1000;
}

// java weka.classifiers.meta.FilteredClassifier -l $model$config".model" -T $testingdataset  -c ${classattr[$config]}
// '

double Classifier::trainModel(const char* wekaFile, int config, const char* tempclassifier,
                              int localmode, int   ltrees,
                              int lfeatures)
{
    const char* accuracyfile = "accuracy.txt";

    stringstream scmd;
    string line = configurations.getClassifierInfo()[config];
    scmd
            << "java -cp \"" << wekaLocation << "\" weka.classifiers.meta.FilteredClassifier ";
    scmd << line;

    scmd << " -d " << tempclassifier;


    scmd << " -t " << wekaFile;


    if (gainThreshold > 0) {
        scmd << " -W weka.classifiers.meta.AttributeSelectedClassifier -- ";
        scmd << "-E \"weka.attributeSelection.GainRatioAttributeEval \" ";
        scmd << "-S \"weka.attributeSelection.Ranker -T " << gainThreshold
             << " -N -1\"";

    }

    switch (localmode) {
    case RNDFOREST:
        scmd << " -W weka.classifiers.trees.RandomForest -- -I " << ltrees
             << " -K " << lfeatures;
        break;
    default:
        scmd << " -W weka.classifiers.meta.AdaBoostM1 -- -P 100 -S 1 -I "
             << ltrees;
        scmd << " -W weka.classifiers.trees.RandomTree -- -K " << lfeatures
             << " -M 1.0 ";
        break;
    }
    scmd << " -S " << getTime();
    scmd << " | grep \"^Correctly Classified Instances\" | awk '{print $5}' > "
         << accuracyfile;
//  cout << scmd.str().c_str() << endl;
    if (verbose > 0) {
        cout << "trying " << ltrees << " trees and " << lfeatures << " features"
             << endl;
        cout.flush();
    }
    system(scmd.str().c_str());
    ifstream acc;
    acc.open(accuracyfile, istream::in);
    double accuracy = 0;
    acc >> accuracy;
    acc.close();
    remove(accuracyfile);
    return accuracy;
}

int Classifier::trainBest(const char* wekaFile, int config)
{
    WekaDataset dataset(wekaFile);
    const int n = 8;
    int dtrees[n]    = { 0, 0, 0, 0, -4, -2, 2, 4 };
    int dfeatures[n] = { 10, 5, -5, 5, 0, 0, 0, 0 };
    ClassifierMode localmode = mode;
    string tempclassifier =  classifiersNames[config] + ".tmp";
    int times = 2;
    int iterations = 4;
    double maccuracy = -1;
    int bmove;
    bool firsttime = true;
    for (localmode = RNDFOREST; localmode < MIXED; ++localmode) {
        if ((localmode & mode) > 0) {
            int ltrees = configurations.getSize();
            int lfeatures = 0; // For unlimited trees
            double accuracy = trainModel(wekaFile, config,
                                         tempclassifier.c_str(), localmode, ltrees,
                                         lfeatures);
            if (verbose > 0) {
                cout << "Accuracy " << accuracy << "%" << endl;
            }
            if (accuracy > maccuracy) {
                stringstream scmd2;
                scmd2 << "cat " << tempclassifier;
                scmd2 << " > " << classifiersNames[config];
                system(scmd2.str().c_str());
                maccuracy = accuracy;
            }
//          if (maccuracy < 100) {
//              lfeatures = dataset.getAttributesNumber() / 2; // for trees with half of the # of attributes height
//              accuracy = trainModel(wekaFile, config, tempclassifier.c_str(),
//                      localmode, ltrees, lfeatures);
//              cout << "Accuracy " << accuracy << "%" << endl;
//              if (accuracy > maccuracy) {
//                  stringstream scmd2;
//                  scmd2 << "cat " << tempclassifier;
//                  scmd2 << " > " << classifiersNames[config];
//                  system(scmd2.str().c_str());
//                  maccuracy = accuracy;
//              }
//          }
            for (int iteration = 0; iteration < iterations && maccuracy < 100; ++iteration) {
                bmove = -1;
                if (verbose > 0) {
                    cout << "Iteration " << iteration << endl;
                }
                for (int move = 0; move < n && maccuracy < 100; ++move) {
                    if (ltrees + dtrees[move] > 0
                            && lfeatures + dfeatures[move] >= 0) {
                        if (verbose > 0) {
                            cout << "Move " << move << endl;
                        }
                        for (int it = 0; it < times; ++it) {
                            double accuracy = trainModel(wekaFile, config,
                                                         tempclassifier.c_str(), localmode,
                                                         ltrees + dtrees[move],
                                                         lfeatures + dfeatures[move]);
                            if (verbose > 0) {
                                cout << "Accuracy " << accuracy << "%" << endl;
                            }
                            if (accuracy > maccuracy) {
                                stringstream scmd2;
                                scmd2 << "cat " << tempclassifier;
                                scmd2 << " > " << classifiersNames[config];
                                system(scmd2.str().c_str());
                                maccuracy = accuracy;
                                bmove = move;
                            }
                        }
                    }

                }
                if (bmove == -1 || maccuracy == 100) {
                    // local best
                    if (verbose > 0) {
                        cout << "Local best" << endl;
                    }
                    break;
                } else {
                    ltrees = ltrees + dtrees[bmove];
                    lfeatures = lfeatures + dfeatures[bmove];
                }
            }

        }
    }
    remove(tempclassifier.c_str());
    if (verbose > 0) {
        cout << "Best for " << configurations.getNames()[config] << " "  << maccuracy << "%" << endl;
    }
    return 0;
}

int Classifier::train(const char* wekaFile)
{
    configurations.loadConfigurationInfo();
    for (int config = 0; config < configurations.getSize(); config++) {
        if (verbose > 0) {
            cout << "Computing " << config << "/" << configurations.getSize() << endl;
        }
        trainBest(wekaFile, config);
    }
    return test(wekaFile);
}

void Classifier::writeTestDetails(const char* wekaFile)
{
    configurations.loadConfigurationInfo();
    WekaDataset dataset(wekaFile);
    vector<string> row;
    int sindex = configurations.getClassIndexes()[0] - 2;
    int offset = configurations.getSize();
    ofstream out;
    string file = wekaFile;
    file = file + ".results.test";
    if (verbose > 0) {
        cout << "Writing results to " << file.c_str() << endl;
    }
    out.open(file.c_str(), ostream::out);
    out.setf(ios::fixed);
    out.precision(10);
    const char* delim = " ";

    out << "instance" << delim;
    out << "features_computation_time" << delim;
    for (int k = 0; k < offset; ++k) {
        out << configurations.getNames()[k] << "_runtime" << delim;
        out << configurations.getNames()[k] << "_class" << delim;
    }
    out << "prediction" << delim;
    out << "probability" << delim;
    out << "correct" << delim;
    out << "prediction_runtime" << endl;

    double average = 0;
    int n = 0;
    int intime = 0;
    for (int i = 0; dataset.getDataRow(row); i++) {
        out << row[0] << delim;
        for (int k = -1; k < offset * 2; ++k) {
            out << row[sindex + k] << delim;
        }
        string prediction = configurations.getNames()[classification[i]];
        out << prediction  << delim;
        out << probabilities[i] << delim;
        average += (correct[i] ? 1 : 0);
        out << correct[i] << delim;
        double t = atof(row[sindex + classification[i] * 2].c_str());
        if (t >= 0) {
            t = t + atof(row[sindex - 1].c_str()) + classifyTime;
        }
        out << t;
        out << endl;
        row.clear();
        if (t > -1 && t < timeout) {
            intime++;
        }
        n++;
    }
    average = average / n;
    ofstream out2;
    string file2 = "classification.totals";
    out2.open(file2.c_str(), ostream::app);
    out2.setf(ios::fixed);
    out2.precision(10);
    out2 << file.c_str() << " " << average << " " << intime << endl;
    out2.close();
    out.close();
}

int Classifier::test(const char* wekaFile)
{
    vector<int> configs;
    configs = classify(wekaFile);
    writeTestDetails(wekaFile);
    return correctCount;
}
