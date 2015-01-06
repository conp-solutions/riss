
#include <fstream>
#include <sstream>
#include <iostream>

#include "useful.h"


int main(int argc, char* argv[])
{
	if( argc == 1 ) { // no arguments, then print the headline of a data file for the extracted data
	  cout << "Instance Status ExitCode RealTime CpuTime Memory ParseTime Variables Clauses "
	      << "Decisions Conflicts PropagatedLits PLitsPerSec Restarts ConPerRestart Removals RemovedClauses ConclictLiterals "
	      << "avgLsize avgLBD maxLsize " 
	      << "CP3-pp-time CP3-ip-time CP3-rem-cls "
	      << "subTime subCls subSteps strSteps "
	      << "eeTime eeCls eeLits eeSteps "
	      << "hteTime hteCls hteSteps "
	      << "bveTime bveSteps "
	      << "aBvaTime aBvaMatchs aBvaCls iBvaTime iBvaMatchs iBvaCls xBvaTime xBvaMatchs xBvaCls "
	      << "probeTime probeSteps "
	      << "vivTime vivCls vivSteps "
	      << "unhideTime unhideCls unhideLits "
	      << "resTime resCls resSteps "
	      << "xorTime xorEEs xorSteps "
	      << "fmTime fmAmos fmSteps "
	      << "cceTime cceCls cceSteps "
	      << "denseVars "
	      << "rate-time rate-steps "
	      << endl;
	  exit(0);
	}

  if( argc < 3 ) { cerr << "wrong number of parameters" << endl; exit(1); }
  
  
  const int extractNumber = 34; // number of string-lines below, plus 1 (starts counting with 0...)
  bool handled[ extractNumber ];
  for( int i = 0 ; i < extractNumber; ++ i ) handled[i] = false;
  
  
//  Instance Status ExitCode RealTime CpuTime Memory Decisions Conflicts CP3PPtime CP3IPtime CP3remcls
  
  float time = 0;
  int exitCode = 0;
  
  string Status;  // 0
  string ExitCode;  // 1
  string RealTime;  // 2
  string CpuTime;  // 3
  string Memory;  // 4
  string Decisions;  // 5
  string Conflicts;  // 6
  // learning
  string avgLsize,avgLBD,maxCsize; // 11
  string propagations, ppPerSecond; // 26
  string restarts, consPerRestart; // 27
  string learnReduces; // 28
  string removedClauses; // 29
  string conflictLiterals; // 30
  
  // CP3 Stats
  string CP3PPtime,CP3IPtime,CP3remcls;  // 7
  string abvaTime,abvaMatchs,abvaCls;  // 8
  string ibvaTime,ibvaMatchs,ibvaCls;  // 9
  string xbvaTime,xbvaMatchs,xbvaCls;  // 10
  string ParseTime;  // 12
  string susTime, subCls; // 13
  string subSteps, strSteps; // 14 
  string eeTime, eeCls, eeLits, eeSteps; // 15
  string hteTime, hteCls, hteSteps; // 16
  string bveTime, bveSteps; // 17
  string probeTime, probeSteps; // 18
  string vivTime, vivCls, vivSteps; // 19
  string unhideTime, unhideCls, unhideLits; // 20
  string resTime, resCls, resSteps; // 21
  string xorTime, xorNewEE, xorSteps; // 22
  string fmTime, fmAMOs, fmSteps; // 23
  string cceTime, cceCls, cceSteps; // 24
  string denseVars; //25
  string rateTime, rateSteps; //33
  
  // instance
  string variables; // 31
  string clauses; // 32


  // parse all files that are given as parameters
  bool verbose = false;
  int fileNameIndex = 1;
  for( int i = 2; i < argc; ++ i ) {

     if( i == 2 && string(argv[i]) == "--debug" ){
       verbose = true;
       // fileNameIndex ++;
       continue;
     }

     // if( i == fileNameIndex ) continue; // do not parse the CNF file!
     
     
      if(verbose) cerr << "use file [ " << i << " ] " << argv[i] << endl;
      
      ifstream fileptr ( argv[i], ios_base::in);
      if(!fileptr) {
	cerr << "c ERROR cannot open file " << argv[i] << " for reading" << endl;
	continue;
      }
  
	string line;		// current line
	// parse every single line
	while( getline (fileptr, line) )
	{
			if(verbose) cerr << "c current line: " << line << endl;
			if( !handled[0] && line.find("[runlim] status:") == 0 ) {
				if(verbose) cerr << "hit status" << endl;
				handled[0] = true;
				Status =  Get(line,2).c_str();
				if( Status == "out" ) {
				 Status = Get(line,4).c_str(); 
				 if( Status == "time" ) Status = "timeout";
				 else if (Status == "memory" ) Status = "memoryout";
				}
			}
	
			else if( !handled[1] && line.find("[runlim] result:") == 0 ) {
			  if(verbose) cerr << "hit exit code" << endl;
				handled[1] = true;
				ExitCode = Get(line,2);
			} 

			else if( !handled[2] && line.find("[runlim] real:") == 0 ) {
			  if(verbose) cerr << "hit real time" << endl;
				handled[2] = true;
				RealTime =  Get(line,2).c_str();
			}

			else if( !handled[3] && line.find("[runlim] time:") == 0 ) {
			  if(verbose) cerr << "hit time" << endl;
				handled[3] = true;
				CpuTime =  Get(line,2).c_str();
			}

			else if( !handled[4] && line.find("[runlim] space:") == 0 ) {
				handled[4] = true;
				Memory =  Get(line,2).c_str();
			}
			else if( !handled[5] && line.find("c decisions             :") == 0 ) {
				handled[5] = true;
				Decisions =  Get(line,3).c_str();
			}
			else if( !handled[6] && line.find("c conflicts             :") == 0 ) {
				handled[6] = true;
				Conflicts =  Get(line,3).c_str();
			}
			else if( !handled[7] && line.find("c [STAT] CP3 ") == 0 ) {
			  if(verbose) cerr << "hit cp3 stats" << endl;
				handled[7] = true;
				CP3PPtime = Get(line,3);
				CP3IPtime = Get(line,5);
				if( !getline (fileptr, line) ) break; // more lines for this technique!
				if( line.find("c [STAT] CP3(2) ") == 0 ) {
				    CP3remcls=Get(line,7);
				}
			}
			else if( !handled[8] && line.find("c [STAT] AND-BVA ") == 0 ) {
			  if(verbose) cerr << "hit and-bva stats" << endl;
				handled[8] = true;
				abvaTime = Get(line,3);
				abvaMatchs = Get(line,5);
				abvaCls = Get(line,9);
			}
			else if( !handled[9] && line.find("c [STAT] ITE-BVA ") == 0 ) {
			  if(verbose) cerr << "hit ite-bva stats" << endl;
				handled[9] = true;
				ibvaTime = Get(line,3);
				ibvaMatchs = Get(line,5);
				ibvaCls = Get(line,9);
			}
			else if( !handled[10] && line.find("c [STAT] XOR-BVA ") == 0 ) {
			  if(verbose) cerr << "hit xor-bva stats" << endl;
				handled[10] = true;
				xbvaTime = Get(line,3);
				xbvaMatchs = Get(line,5);
				xbvaCls = Get(line,9);
			}
			
			else if( !handled[11] && line.find("c learning:") == 0 ) {
			  if(verbose) cerr << "hit learning stats" << endl;
				handled[11] = true;
				avgLsize=Get(line,4);
				avgLBD=Get(line,7);
				maxCsize=Get(line,10);
			} 
			
			else if( !handled[12] && line.find("c |  Parse time:") == 0 ) {
			  if(verbose) cerr << "hit parse time" << endl;
				handled[12] = true;
				ParseTime=Get(line,4);
			} 
			
			else if( !handled[13] && line.find("c [STAT] SuSi(1)") == 0 ) {
			  if(verbose) cerr << "hit subsimp time stats" << endl;
				handled[13] = true;
				susTime=Get(line,3);
				subCls=Get(line,5);
			}
			else if( !handled[14] && line.find("c [STAT] SuSi(2)") == 0 ) {
			  if(verbose) cerr << "hit subsimp step stats" << endl;
				handled[14] = true;
				subSteps=Get(line,3);
				strSteps=Get(line,5);
			}
			else if( !handled[15] && line.find("c [STAT] EE ") == 0 ) {
			  if(verbose) cerr << "hit ee stats" << endl;
				handled[15] = true;
				eeTime=Get(line,3);
				eeCls=Get(line,9);
				eeLits=Get(line,7);
				eeSteps=Get(line,5);
			}
			else if( !handled[16] && line.find("c [STAT] HTE ") == 0 ) {
				if(verbose) cerr << "hit hte stats" << endl;
				handled[16] = true;
				hteTime=Get(line,3);
				hteCls=Get(line,5);
				hteSteps=Get(line,9);
			}
			else if( !handled[17] && line.find("c [STAT] BVE(1) ") == 0 ) {
				if(verbose) cerr << "hit bve stats" << endl;
				handled[17] = true;
				bveTime=Get(line,3);
				bveSteps=Get(line,18);
			}
			else if( !handled[18] && line.find("c [STAT] PROBING(1) ") == 0 ) {
				if(verbose) cerr << "hit probe stats" << endl;
				handled[18] = true;
				probeTime=Get(line,3);
				if( !getline (fileptr, line) ) break; // more lines for this technique!
				if( line.find("c [STAT] PROBING(2) ") == 0 ) {
				    probeSteps=Get(line,11);
				}
			}
			else if( !handled[19] && line.find("c [STAT] VIVI ") == 0 ) {
				if(verbose) cerr << "hit viv stats" << endl;
				handled[19] = true;
				vivTime=Get(line,3);
				vivCls=Get(line,5);
				vivSteps=Get(line,9);
			}
			else if( !handled[20] && line.find("c [STAT] UNHIDE ") == 0 ) {
				if(verbose) cerr << "hit unhide stats" << endl;
				handled[20] = true;
				unhideTime=Get(line,3);
				unhideCls=Get(line,5);
				unhideLits=Get(line,9);
			}
			else if( !handled[21] && line.find("c [STAT] RES ") == 0 ) {
				if(verbose) cerr << "hit resolve stats" << endl;
				handled[21] = true;
				resTime=Get(line,3);
				resCls=Get(line,5);
				resSteps=Get(line,13);
			}
			else if( !handled[22] && line.find("c [STAT] XOR ") == 0 ) {
				if(verbose) cerr << "hit xor stats" << endl;
				handled[22] = true;
				xorTime=Get(line,3);
				if( xorTime.size() > 2 ) xorTime = xorTime.substr( 0, xorTime.size() - 2); // remove trailing "s,"
				if( !getline (fileptr, line) ) break; // more lines for this technique!
				if( !getline (fileptr, line) ) break; // more lines for this technique!
				if( line.find("c [STAT] XOR(3) ") == 0 ) {
				  xorNewEE=Get(line,3);
				  xorSteps=Get(line,13);
				}
			}
			else if( !handled[23] && line.find("c [STAT] FM ") == 0 ) {
				if(verbose) cerr << "hit fm stats" << endl;
				handled[23] = true;
				fmTime =  Get(line, 3);
				fmSteps = Get(line, 21);
				fmAMOs =  Get(line, 9);
			}
			else if( !handled[24] && line.find("c [STAT] CCE ") == 0 ) {
				if(verbose) cerr << "hit cce stats" << endl;
				handled[24] = true;
				cceTime  = Get(line, 3);
				cceCls   = Get(line, 5);
				cceSteps = Get(line, 11);
			}
			else if( !handled[25] && line.find("c [STAT] DENSE ") == 0 ) {
				if(verbose) cerr << "hit dense stats" << endl;
				handled[25] = true;
				denseVars =  Get(line, 3);
			}
			
			else if( !handled[26] && line.find("c propagations          : ") == 0 ) {
				handled[26] = true;
				propagations =  Get(line, 3);
				ppPerSecond =  Get(line, 4);
				if( ppPerSecond.size() > 1 ) ppPerSecond = ppPerSecond.substr(1, ppPerSecond.size() - 1 ); // cut of the leading "("
			}
			else if( !handled[27] && line.find("c restarts              : ") == 0 ) {
				handled[27] = true;
				restarts =  Get(line, 3);
				consPerRestart = Get(line, 4);
				if( consPerRestart.size() > 1 ) consPerRestart = consPerRestart.substr(1, consPerRestart.size() - 1 ); // cut of the leading "("
			}
			else if( !handled[28] && line.find("c nb ReduceDB           : ") == 0 ) {
				handled[28] = true;
				learnReduces =  Get(line, 4);
			}
			else if( !handled[29] && line.find("c nb reduced Clauses    : ") == 0 ) {
				handled[29] = true;
				removedClauses =  Get(line, 5);
			}
			else if( !handled[30] && line.find("c conflict literals     : ") == 0 ) {
				handled[30] = true;
				conflictLiterals =  Get(line, 4);
			}
			
			else if( !handled[31] && line.find("c |  Number of variables:") == 0 ) {
				handled[31] = true;
				variables =  Get(line, 5);
			}
			else if( !handled[32] && line.find("c |  Number of clauses:") == 0 ) {
				handled[32] = true;
				clauses =  Get(line, 5);
			}
			else if( !handled[33] && line.find("c [STAT] RATE ") == 0 ) {
				handled[33] = true;
				rateTime  = Get(line, 3);
				rateSteps = Get(line, 5);
			}
	}
  
  
  }
  
  // output collected information
	cout << argv[ fileNameIndex ] << " ";
	if( handled[0] || Status != "" ) cout << Status ; else cout << "fail";
	cout << " ";
	if( handled[1] || ExitCode != "" ) cout << ExitCode ; else cout << "-";
	cout << " ";
	if( handled[2] || RealTime != "" ) cout << RealTime ; else cout << "-";
	cout << " ";
	if( handled[3] || CpuTime != "" ) cout << CpuTime ; else cout << "-";
	cout << " ";
	if( handled[4] || Memory != "" ) cout << Memory ; else cout << "-";
	cout << " ";
	if( handled[12] || ParseTime != "" ) cout << ParseTime ; else cout << "-";
	cout << " ";
	if( handled[31] || variables != "" ) cout << variables ; else cout << "-";
	cout << " ";
	if( handled[32] || clauses != "" ) cout << clauses ; else cout << "-";
	cout << " ";
	if( handled[5] || Decisions != "" ) cout << Decisions ; else cout << "-";
	cout << " ";
	if( handled[6] || Conflicts != "" ) cout << Conflicts ; else cout << "-";
	cout << " ";
	if( handled[26] || propagations != "" || ppPerSecond != "" ) cout << propagations << " " << ppPerSecond ; else cout << "- -";
	cout << " ";
	if( handled[27] || restarts != "" || consPerRestart != "" ) cout << restarts << " " << consPerRestart ; else cout << "- -";
	cout << " ";
	if( handled[28] || learnReduces != "" ) cout << learnReduces  ; else cout << "-";
	cout << " ";
	if( handled[29] || removedClauses != "" ) cout << removedClauses  ; else cout << "-";
	cout << " ";
	if( handled[30] || conflictLiterals != "" ) cout << conflictLiterals  ; else cout << "-";
	cout << " ";
	if( handled[11] || avgLsize != "" || avgLBD != "" || maxCsize != "" ) cout << avgLsize << " " << avgLBD << " " << maxCsize  ; else cout << "- - -";
	cout << " ";
	if( handled[7] || CP3PPtime != "" || CP3IPtime != "" || CP3remcls != "" ) cout << CP3PPtime << " " << CP3IPtime << " " << CP3remcls  ; else cout << "- - -";
	cout << " ";
	if( handled[13] || susTime != "" || subCls != "" ) cout << susTime << " " << subCls ; else cout << "- -";
	cout << " ";
	if( handled[14] || subSteps != "" || strSteps != "" ) cout << subSteps << " " << strSteps ; else cout << "- -";
	cout << " ";
	if( handled[15] || eeTime != "" || eeCls != ""  || eeLits != "" || eeSteps != "" ) cout << eeTime << " " << eeCls << " " << eeLits << " " << eeSteps; else cout << "- - - -";
	cout << " ";
	if( handled[16] || hteTime != "" || hteCls != "" || hteSteps != "" ) cout << hteTime << " " << hteCls << " " << hteSteps  ; else cout << "- - -";
	cout << " ";
	if( handled[17] || bveTime != "" || bveSteps != "" ) cout << bveTime << " " << bveSteps ; else cout << "- -";
	cout << " ";	
	if( handled[8] || abvaTime != "" || abvaMatchs != "" || abvaCls != "" ) cout << abvaTime << " " << abvaMatchs << " " << abvaCls  ; else cout << "- - -";
	cout << " ";
	if( handled[9] || ibvaTime != "" || ibvaMatchs != "" || ibvaCls != "" ) cout << ibvaTime << " " << ibvaMatchs << " " << ibvaCls  ; else cout << "- - -";
	cout << " ";
	if( handled[10] || xbvaTime != "" || xbvaMatchs != "" || xbvaCls != "" ) cout << xbvaTime << " " << xbvaMatchs << " " << xbvaCls  ; else cout << "- - -";
	cout << " ";	
	if( handled[18] || probeTime != "" || probeSteps != "" ) cout << probeTime << " " << probeSteps ; else cout << "- -";
	cout << " ";	
	if( handled[19] || vivTime != "" || vivCls != "" || vivSteps != "" ) cout << vivTime << " " << vivCls << " " << vivSteps  ; else cout << "- - -";
	cout << " ";	
	if( handled[20] || unhideTime != "" || unhideCls != "" || unhideLits != "" ) cout << unhideTime << " " << unhideCls << " " << unhideLits  ; else cout << "- - -";
	cout << " ";	
	if( handled[21] || resTime != "" || resCls != "" || resSteps != "" ) cout << resTime << " " << resCls << " " << resSteps  ; else cout << "- - -";
	cout << " ";	
	if( handled[22] || xorTime != "" || xorNewEE != "" || xorSteps != "" ) cout << xorTime << " " << xorNewEE << " " << xorSteps  ; else cout << "- - -";
	cout << " ";
	if( handled[23] || fmTime != "" || fmAMOs != "" || fmSteps != "" ) cout << fmTime << " " << fmAMOs << " " << fmSteps  ; else cout << "- - -";
	cout << " ";	
	if( handled[24] || cceTime != "" || cceCls != "" || cceSteps != "" ) cout << cceTime << " " << cceCls << " " << cceSteps  ; else cout << "- - -";
	cout << " ";
	if( handled[25] || denseVars != "" ) cout << denseVars  ; else cout << "-";
	cout << " ";
	if( handled[33] || rateTime != "" || rateSteps != "" ) cout << rateTime << " " << rateSteps  ; else cout << "- -";
	cout << " ";
      cout << endl;
	
  return 0;
  
}
