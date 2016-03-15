head data2.csv 
  awk '{print $1,$8,$9,$10}' data2.csv > size.csv
  awk '{print $1,$4,$6}' data1.csv > resources.csv 
  gedit features1.csv 
  cp features1.csv features.csv
  sed -i s/\'//g features.csv 
  head -n 1  features.csv 


  sed -i s/\,//g features.csv 
  head -n 1  features.csv 
  cp features1.csv features.csv
  sed -i "s/\,/ /g" features.csv 
  head -n 1  features.csv 
  ls
  mkdir clusterData
  mv data1.csv data2.csv features1.csv clusterData/
  ls
  awk '{print $1}' features.csv > succesfulExtraction.csv 
  head succesfulExtraction.csv 
  for f in `cat succesfulExtraction.csv`; do echo "$f ok" > featureState.csv; done 
  cat featureState.csv | wc -l
  rm featureState.csv; for f in `cat succesfulExtraction.csv`; do echo "$f ok" >> featureState.csv; done 
  cat featureState.csv | wc -l
  awk '{print $1}' size.csv > files.txt 
  nano files.txt 
  nano succesfulExtraction.csv 
  rm featureState.csv 
  nano succesfulExtraction.csv 
  for f in `cat succesfulExtraction.csv`; do echo "$f ok" >> featureState.csv; done 
  nano featureState.csv 
  for f in `cat files.txt`; do grep $f featureState.csv ; ec=$?; if [ "$ec" -ne "0" ]; then echo "$f fail" >> featureState.csv ; fi ; done 
  cat featureState.csv | wc -l
  tail featureState.csv 
  gedit join.sh 
  ls
  head succesfulExtraction.csv 
  cat succesfulExtraction.csv | wc -l
  head join.sh
  ./join.sh fullFeatureData.csv  featureState.csv size.csv resources.csv 
  cat fullFeatureData.csv | wc -l
  head -n 1 fullFeatureData.csv 

