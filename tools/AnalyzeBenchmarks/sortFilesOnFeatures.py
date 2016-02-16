# Norbert Manthey, 2015
# create alias file from feature file
#
# python sortFilesOnFeatures.py feature-file.csv uniq-files.txt file-mapping.txt file-reverse-mapping.txt
#
# Input:
#  feature-file.csv: 1st line is headline (ignored). 1st column is name of the formula, all other columns store features of that formula
#
# Output:
#  uniq-files.txt:           list of formulas that represents all formulas of the benchmark
#  file-mapping.txt:         contains all equivalence classes (useful to blow results from experiment on uniq formulas to full set of formulas)
#  file-reverse-mapping.txt: contains the alias for each formulas (the formula that is used in the uniq list instead of the current formula)
#


# TODO: functions and abort criterion, "with open" for blocks that handle opened files

import sys
import string
import os;
import re

def atoi(text):
    return int(text) if text.isdigit() else text

def natural_keys(text):
    '''
    alist.sort(key=natural_keys) sorts in human order
    http://nedbatchelder.com/blog/200712/human_sorting.html
    (See Toothy's implementation in the comments)
    '''
    return [ atoi(c) for c in re.split('(\d+)', text) ]
    
#
# start of the main function
#

if len( sys.argv ) < 1:
  print "error: no feature file given - abort"
  sys.exit(1)

featurefile = open(sys.argv[1], "r")
filelines = featurefile.readlines()

# store features, move instance from first position to last position
features = []

# split instance names and features
for line in filelines[1:]:
	tokenline = line.split( )
	movetokenline = tokenline[1:]
	movetokenline.append( tokenline[0] )
	features.append( movetokenline )

# sort newly created list of features (finally sorts by name of the instance, if all features are equal)
features.sort()

# match instances
lastinstance = ""
lastmatch = -1
currentID = -1
matchingformulas=[]
for item in features:
  # print ""
  currentID = currentID + 1
  name = item[-1]
  # are we working with the first element?
  matches = True
  # compare features
  if lastmatch != -1: 
    # print "compare " + name + " with " + lastinstance
    itemlength=len( item )
    comparelength=len( features[lastmatch] )
    matches=itemlength==comparelength and itemlength >= 0
    if matches:
      # compare all items of the features element by element 
      for i in range(0,itemlength-1):
        if item[i] != features[lastmatch][i]:
          matches=False
          # print "features " + str(i) + " do not match: " + item[i] + " vs " + features[lastmatch][i]
          break
        # else:
        #   print "compared " + item[i] + " with " + features[lastmatch][i]
    # else:
    #   print "matching length failed: " + str(itemlength) + " vs " + str(comparelength)
  else:
    # print "not matching, as we are touching the first element"
    matches = False

  # print "Formulas match: " + str(matches)
  # evaluate outcome
  if lastmatch == -1 or not matches:
    # store most recently seen new formula
    lastmatch = currentID
    lastinstance = name
    # print "novel formula " + name + " with " + str(item[0:len(item)-1])
    # print "currentID: " + str(currentID) + " last name: " + lastinstance
    # create new class
    thisclass = []
    thisclass.append( name )
    matchingformulas.append( thisclass )
  else:
	  # print "match: " + name + " with " + lastinstance
	  # add to most recent class
	  matchingformulas[-1].append( name )
	  
# print final information (there is also the headline in the feature files)
print "different instances: " + str(len(matchingformulas)) + " out of " + str( len(filelines) )

# sort clases
for eclass in matchingformulas:
  sorted(eclass, key=natural_keys)
  # print eclass

#
# take care of printing unique formulas
#
if len( sys.argv ) < 3:
  print "error: no file name for the unique formula file given - stop"
  sys.exit(1)
  
# print all uniq instances
uniqfile = open(sys.argv[2], "w")
for eclass in matchingformulas:
  uniqfile.write(str(eclass[0]) + '\n') # write first element of the class (should be the oldest formula we found based on file names that are based on years
uniqfile.close()

#
# take care of printing the forward mapping (alias -> list of other files, not including the alist itself!)
#
if len( sys.argv ) < 4:
  print "error: no file name for the file mapping given - stop"
  sys.exit(1)
  
# print forward mapping
mappingfile = open(sys.argv[3], "w")
for eclass in matchingformulas:
  for formula in eclass:
    mappingfile.write(str(formula) + " ") # write all formulas of this class into a single line
  mappingfile.write('\n') 
mappingfile.close()

#
# take care of printing the reverse mapping (for each file prints the file that is used in the uniq list)
#
if len( sys.argv ) < 5:
  print "error: no file name for the file mapping given - stop"
  sys.exit(1)
  
# print reverse mapping
reversemappingfile = open(sys.argv[4], "w")
for eclass in matchingformulas:
  for formula in eclass:
    reversemappingfile.write(str(formula) + " " + eclass[0] + "\n") # write all formulas of this class into a single line
reversemappingfile.close()
