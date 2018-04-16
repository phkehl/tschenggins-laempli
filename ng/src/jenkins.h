/*!
    \file
    \brief flipflip's Tschenggins Lämpli: Jenkins status (see \ref FF_JENKINS)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup FF_JENKINS JENKINS
    \ingroup FF

    @{
*/
#ifndef __JENKINS_H__
#define __JENKINS_H__

#include "stdinc.h"

//! initialise
void jenkinsInit(void);

//! start Jenkins task
void jenkinsStart(void);

//! print Jenkins task/status  monitor string
void jenkinsMonStatus(void);

//! maximum number of channels (jobs) we can track
#define JENKINS_MAX_CH 20

//! possible job states
typedef enum JENKINS_STATE_e
{
    JENKINS_STATE_UNKNOWN = 0,  //!< unknown state
    JENKINS_STATE_OFF,          //!< Jenkins job is off (disabled)
    JENKINS_STATE_RUNNING,      //!< Jenkins job is running (building)
    JENKINS_STATE_IDLE,         //!< Jenkins job is idle
} JENKINS_STATE_t;

//! convert Jenkins state string to state enum
/*!
    \param[in] str  the state string ("unkown", "off", "running", "idle")
    \returns the corresponding state, or #JENKINS_STATE_UNKNOWN for illegal strings
*/
JENKINS_STATE_t jenkinsStrToState(const char *str);

//! possible job results, ordered from best to worst
typedef enum JENKINS_RESULT_e
{
    JENKINS_RESULT_UNKNOWN = 0,  //!< unknown result
    JENKINS_RESULT_SUCCESS,      //!< build is success (stable)
    JENKINS_RESULT_UNSTABLE,     //!< build is unstable (but has completed)
    JENKINS_RESULT_FAILURE,      //!< build is failure (has not completed)
} JENKINS_RESULT_t;

//! convert Jenkins result string to result enum
/*!
    \param[in] str  the result string ("unkown", "off", "running", "idle")
    \returns the corresponding result, or #JENKINS_RESULT_UNKNOWN for illegal strings
*/
JENKINS_RESULT_t jenkinsStrToResult(const char *str);

//! maximum length of a job name
#define JENKINS_JOBNAME_LEN 48

//! maximum length of a server name
#define JENKINS_SERVER_LEN  32

//! Jenkins job information
typedef struct JENKINS_INFO_s
{
    uint16_t         chIx;                         //!< channel (< #JENKINS_MAX_CH)
    bool             active;                       //!< active, i.e. state/result/job/server/time fields valid
    JENKINS_RESULT_t result;                       //!< job result
    JENKINS_STATE_t  state;                        //!< job state
    char             job[JENKINS_JOBNAME_LEN];     //!< job name
    char             server[JENKINS_SERVER_LEN];   //!< server name
    int32_t          time;                         //!< timestamp
} JENKINS_INFO_t;

//! update Jenkins job info
/*!
    \param[in] pkInfo  pointer to Jenkins job info struct to copy data from
*/
void jenkinsSetInfo(const JENKINS_INFO_t *pkInfo);

//! set all states to JENKINS_STATE_UNKNOWN
void jenkinsUnknownAll(void);

//! clear all info
void jenkinsClearAll(void);

#endif // __JENKINS_H__
