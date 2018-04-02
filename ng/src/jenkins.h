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

#define JENKINS_MAX_CH 20

//! possible job states
typedef enum JENKINS_STATE_e
{
    JENKINS_STATE_UNKNOWN = 0,
    JENKINS_STATE_OFF,
    JENKINS_STATE_RUNNING,
    JENKINS_STATE_IDLE,
} JENKINS_STATE_t;

JENKINS_STATE_t jenkinsStrToState(const char *str);
const char *jenkinsStateToStr(const JENKINS_STATE_t state);

//! possible job results
typedef enum JENKINS_RESULT_e
{
    JENKINS_RESULT_UNKNOWN = 0,
    JENKINS_RESULT_SUCCESS,
    JENKINS_RESULT_UNSTABLE,
    JENKINS_RESULT_FAILURE,
} JENKINS_RESULT_t;

JENKINS_RESULT_t jenkinsStrToResult(const char *str);
const char *jenkinsResultToStr(const JENKINS_RESULT_t result);

#define JENKINS_JOBNAME_LEN 48
#define JENKINS_SERVER_LEN  32

typedef struct JENKINS_INFO_s
{
    short int        chIx;
    bool             active;
    JENKINS_RESULT_t result;
    JENKINS_STATE_t  state;
    char             job[JENKINS_JOBNAME_LEN];
    char             server[JENKINS_SERVER_LEN];
    int32_t          time;
} JENKINS_INFO_t;

void jenkinsSetInfo(const JENKINS_INFO_t *pkInfo);

void jenkinsClearInfo(void);

#endif // __JENKINS_H__
