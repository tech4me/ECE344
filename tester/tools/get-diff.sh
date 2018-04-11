ECE_SVN=svn+ssh://ug250.eecg.utoronto.ca/svn
#ECE_SVN=svn+ssh://ug131.eecg.utoronto.ca/svn
START_GRP=1
END_GRP=39

for i in `seq -f "%03g" ${START_GRP} ${END_GRP}`
do
	SVN_REP=${ECE_SVN}/os-${i}/svn
	echo "Downloading patch for os-${i}"
	svn diff ${SVN_REP}/tags/asst2-start ${SVN_REP}/tags/asst2-end > os161-asst-${i}.patch
done
