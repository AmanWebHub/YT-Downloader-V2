#include "DownloadJob.h"

namespace DownloadJob
{
    HANDLE CreateDownloadJob()
    {
        HANDLE job = CreateJobObjectW(nullptr, nullptr);

        if (job == nullptr)
        {
            return nullptr;
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo{};

        jobInfo.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        if (!SetInformationJobObject(
            job,
            JobObjectExtendedLimitInformation,
            &jobInfo,
            sizeof(jobInfo)))
        {
            CloseHandle(job);
            return nullptr;
        }

        return job;
    }
}
