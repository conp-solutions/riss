
#include <fstream>
#include <sstream>
#include <iostream>

#include "useful.h"


int main(int argc, char* argv[])
{
  if( argc == 1 ) {
    cout << "usage: ./select col file1 ... filen" << endl;
    cout << "  combine all the lines of the n files, and selects the line where the value in column col is the smallest" << endl;
    cout << endl;
    cout << "  columns are space separated" << endl;
    cout << "  no number is worse than a number" << endl;
    exit(0);
  }

  if( argc < 3 ) { cerr << "wrong number of parameters" << endl; exit(1); }
  
  int col = atoi( argv[1] );
  cerr << "c compare col " << col << " (first column has index 1!)" << endl;
  col --;
  
  ifstream** files = new ifstream*[argc];
  
  int nrFiles = 0 ;
  for( int i = 2; i < argc; ++ i ) {
     files[nrFiles] = new ifstream (argv[i], ios_base::in);
     
     if( files[nrFiles]->bad() ) {
       cerr << "c could not open all files. openend " << nrFiles << ". failed at " << string(argv[i]) << endl;
       break; // stop here!
     } else {
       cerr << "c opened file " << argv[i] << endl; 
     }
     nrFiles++;
  }
  
  cerr << "c opened " << nrFiles << " files." << endl;
  cerr << "c here comes the file ..." << endl << "======================" << endl;
  
  if( nrFiles == 0 ) {
    cerr << "c abort, because no files have been opened" << endl;
    exit(1);
  }
  
  string strings [nrFiles];
  string firstCol;
  
  int currentLine = 0;
  while( true ) 
  {
    // get next line for each file
    bool error = false;
    for( int i = 0; i < nrFiles; ++ i ) {
      if( !getline ( (*files[i]), strings[i]) ) {
	error = true;
      }
      // cerr << "c get file " << i << " ,line " << currentLine << " : " << strings[i] << endl;
    }
    
    if( error ) {
      bool badError = false;
      for( int i = 0; i < nrFiles; ++ i ) {
	badError = badError || files[i]->eof();
      }  
      if( badError ) {
	cerr << "c reached end of file" << endl;
	break;
      } else {
	cerr << "c cannot read line " << currentLine << " properly, skip it" << endl;
	continue; //
      }
    }
    
    firstCol = Get( strings[0], 0);
    
    // select the smallest line
    double smallestValue = 0;
    int smallestCol = 0;
    bool initialized = false;
    if( Get( strings[0], col ) != "-" ) {
      stringstream s;
      s << Get( strings[0], col );
      s >> smallestValue;
      initialized = true;
    }
    error = false;
    for( int i = 1; i < nrFiles; ++ i ) {
      if( firstCol != Get( strings[i], 0) ) {
	error = true;
      } else {
	// select only if column did not fail
	if( Get( strings[i], col ) != "-" ) {
	  stringstream s2;
	  s2 << Get( strings[i], col );
	  double thisValue = 0;
	  s2 >> thisValue;
	  if(!initialized || thisValue < smallestValue) {
	    smallestValue = thisValue;
	    smallestCol = i;
	    initialized = true;
	  }
	}
      }
    }
    if( error ) {
      cerr << "c first column of lines " << currentLine << " do not match" << endl;
    } else {
      cout << strings[smallestCol] << endl; // print selected string
    }
    currentLine ++;
  }

  cerr << "======================" << endl;
  cerr << "c read " << currentLine << " lines" << endl;
  
  // close files and clean up!
  for( int i = 0; i < nrFiles; ++i ) {
    files[i]->close();
  }
  delete [] files;

  return 0;
}
