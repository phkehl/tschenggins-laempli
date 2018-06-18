#!/bin/bash
#Updates a Tschenggins Job on the backend
#
#Multiple Jenkins jobs can be given for one backend job.
#The multiple jobs will be ANDed together.
#E.g. JenkinsJob1 AND JenkinsJob2 => BackendJob
#
#If run without an argument it will update a hardcoded list of
#backend jobs
#
#Can be run with arguments:
#First argument is Backend Job, all following arguments are Jenkins Jobs
#E.g ./pushJenkinsStateToLaempli.sh BackendJob JenkinsJob1 JenkinsJob2

USER="USER"
PASS="PASS"
BACKENDUSER="BACKENDUSER"
BACKENDPASS="BACKENDPASS"
BACKENDSERVER="BACKENDSERVER"
BACKENDPORT=BACKENDPORT
TARGETSERVER="TARGETSERVER"
JENKINSSERVER="JENKINSSERVER"
DEFAULTTARGETJOB="DEFAULTTARGETJOB"
DEFAULTJENKINSJOBS="JOB1 JOB2 JOB3"

#push update to backend $1=result, $2=state $3=backendjob
function update {
	echo "Updating '$3' with state '$2' and result '$1'"
	curl --header "Content-Type: application/json" -s \
		--request POST \
		--data '{"cmd":"update","debug":0,"states":[{"name":"'$3'","result":"'$1'","server":"'$TARGETSERVER'","state":"'$2'"}]}' \
		http://$BACKENDUSER:$BACKENDPASS@$BACKENDSERVER:$BACKENDPORT/tschenggins-laempli/ng/ > /dev/null
    if [[ $? -ne 0 ]]; then
        echo "There was an error updating the backend"
    fi
}

#get state of project $1=buildNumber $2=jobname
function getstate {
	local state=`curl -s -get --user $USER:$PASS https://$JENKINSSERVER/jenkins-jobs/$2/builds/$1/build.xml | grep -m1 -Po "(?<=<duration>).+(?=</duration>)"`
	if [[ "$state" != "" ]]; then
		if [[ "$state" != "0" ]]; then
			echo "idle"
		else
			echo "running"
		fi
	else
		echo "unknown"
	fi
}

#get result of project $1=buildNumber $2=jobname
function getresult {
	local state=`curl -s -get --user $USER:$PASS https://$JENKINSSERVER/jenkins-jobs/$2/builds/$1/build.xml | grep -a1 startTime | grep -m1 -Po "(?<=<result>).+(?=</result>)"`
	echo "$state" | awk '{print tolower($1)}'
}

#get latest buildNr for project  $1=project
function getlatestbuild {
	local nextBuild=`curl -s -get --user $USER:$PASS https://$JENKINSSERVER/jenkins-jobs/$1/nextBuildNumber`
	echo $(($nextBuild-1))
	exit $(($nextBuild-1))
}

#get latest job result for project $1=project
function getjobresult {
	#echo "Getting job status for job $1"
	local buildNr=`getlatestbuild $1`
	#echo "Got buildnr $buildNr"
	local result=`getresult $buildNr $1`
	result=`echo $result | sed 's/aborted/unknown/'`
	if [[ "$result" == "" ]]; then
		result="unknown"
	fi
	echo "$result"
}

#get latest job state for project $1=project
function getjobstate {
	#echo "Getting job status for job $1"
	local buildNr=`getlatestbuild $1`
	#echo "Got buildnr $buildNr"
	local state=`getstate $buildNr $1`
	echo "$state"
}

#update a job with latest result and state $1=backendjob $2...end=jenkins jobsarray
function updatetargetjob {
	local finalResult="initialized"
	local finalState="idle"
	local targetjob="$1"
	shift
	local jobs=("${@}")
	for job in "${jobs[@]}"
	do
		local result=`getjobresult $job`
		local state=`getjobstate $job`
		if [[ "$state" != "idle" ]]; then
			finalState="running"
		fi
        if [[ "$result" == "failure" ]]; then
            finalResult="failure";
        fi
        if [[ "$result" == "unstable" && "$finalResult" != "failure" ]]; then
            finalResult="unstable";
        fi
        if [[ "$result" == "unknown" &&  "$finalResult" != @("failure"|"unstable") ]]; then
            finalResult="unknown";
        fi
        if [[ "$result" == "success" &&  "$finalResult" != @("failure"|"unstable"|"unknown") ]]; then
            finalResult="success";
        fi
		echo "$job: $result, $state"
	done
	update "$finalResult" "$finalState" $targetjob
}

function main {
	#if there are no parameters
	if [[ $# -eq 0 ]]; then
		updatetargetjob "$DEFAULTTARGETJOB" $DEFAULTJENKINSJOBS
	else
		if [[ $# -eq 1 ]]; then
			echo "Illegal number of arguments - at least 2 needed"
			echo "Example: TARGETJOB JENKINSJOB [JENKINSJOB2, ...]"
		else
			updatetargetjob "$1" "${@:2}"
		fi
	fi
}

#execute main function
main $@
