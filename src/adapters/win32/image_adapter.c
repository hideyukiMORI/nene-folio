#include "image_adapter.h"
#include "diagram_render.h"
#include "image_cache.h"
#include "image_decode.h"
#include "image_download.h"
#include "image_file.h"
#include "storage_native_path.h"
#include <objbase.h>
#include <stdlib.h>
#include <windows.h>
struct image_adapter
{
    SRWLOCK lock;
    HANDLE _Nullable wake;
    HANDLE _Nullable worker;
    wchar_t root[storage_native_capacity];
    struct image_cache *_Nullable cache;
    struct image_job queued;
    uint64_t ticket;
    bool stopping;
    enum image_phase phase;
    enum image_outcome outcome;
    struct image_bitmap *_Nullable result;
};
static bool cancelled(const struct image_work *_Nonnull work)
{
    struct image_adapter *adapter = work->adapter;
    AcquireSRWLockShared(&adapter->lock);
    bool stale = adapter->stopping || adapter->ticket != work->ticket;
    ReleaseSRWLockShared(&adapter->lock);
    return stale;
}
static void progress(const struct image_work *_Nonnull work, enum image_phase phase)
{
    struct image_adapter *adapter = work->adapter;
    AcquireSRWLockExclusive(&adapter->lock);
    if (adapter->ticket == work->ticket)
        adapter->phase = phase;
    ReleaseSRWLockExclusive(&adapter->lock);
}
static enum image_outcome local_image(struct image_adapter *_Nonnull adapter,
                                      const struct image_job *_Nonnull job,
                                      const struct image_work *_Nonnull work,
                                      struct image_bitmap *_Nullable *_Nonnull out)
{
    struct image_file *_Nullable file = nullptr;
    struct image_request request = {.category = job->category, .source = job->source};
    enum image_outcome outcome =
        image_file_read(adapter->root, &request, IMAGE_EXPORT_ALL_LOCAL, &file);
    if (outcome == IMAGE_READY)
    {
        struct image_encoded encoded = {.bytes = image_file_bytes(file),
                                        .length = image_file_length(file)};
        outcome = image_cache_get(adapter->cache, job, &encoded, out);
        if (outcome == IMAGE_PENDING)
        {
            outcome = image_decode_read(encoded, work, out);
            if (outcome == IMAGE_READY && !cancelled(work))
                image_cache_store(adapter->cache, job, &encoded, *out);
        }
    }
    image_file_destroy(file);
    return outcome;
}
static enum image_outcome https_image(const struct image_job *_Nonnull job,
                                      const struct image_work *_Nonnull work,
                                      struct image_bitmap *_Nullable *_Nonnull out)
{
    struct image_download *_Nullable download = nullptr;
    enum image_outcome outcome =
        image_download_read(image_source_target(job->source), work, &download);
    if (outcome == IMAGE_READY)
        outcome = image_decode_read(image_download_bytes(download), work, out);
    image_download_destroy(download);
    return outcome;
}
static enum image_outcome load_file(struct image_adapter *_Nonnull adapter,
                                    const struct image_job *_Nonnull job,
                                    const struct image_work *_Nonnull work,
                                    struct image_bitmap *_Nullable *_Nonnull out)
{
    enum image_outcome result = IMAGE_UNSUPPORTED;
    switch (image_source_kind(job->source))
    {
    case IMAGE_SOURCE_LOCAL:
        result = local_image(adapter, job, work, out);
        break;
    case IMAGE_SOURCE_HTTPS:
        result = image_cache_get(adapter->cache, job, nullptr, out);
        if (result == IMAGE_PENDING)
        {
            result = https_image(job, work, out);
            if (result == IMAGE_READY && !cancelled(work))
                image_cache_store(adapter->cache, job, nullptr, *out);
        }
        break;
    case IMAGE_SOURCE_UNSUPPORTED:
        break;
    }
    return result;
}
static enum image_outcome load_diagram(struct image_adapter *_Nonnull adapter,
                                       const struct image_job *_Nonnull job,
                                       const struct image_work *_Nonnull work,
                                       struct image_bitmap *_Nullable *_Nonnull out)
{
    enum image_outcome result = image_cache_get(adapter->cache, job, nullptr, out);
    if (result == IMAGE_PENDING)
    {
        struct diagram_request request = {job->diagram, job->palette, job->face};
        result = diagram_render_read(&request, work, out);
        if (result == IMAGE_READY && !cancelled(work))
            image_cache_store(adapter->cache, job, nullptr, *out);
    }
    return result;
}
static enum image_outcome load(struct image_adapter *_Nonnull adapter,
                               const struct image_job *_Nonnull job,
                               struct image_bitmap *_Nullable *_Nonnull out)
{
    struct image_work work = {
        .adapter = adapter, .ticket = job->ticket, .cancelled = cancelled, .progress = progress};
    switch (job->kind)
    {
    case IMAGE_JOB_EMPTY:
        return IMAGE_CANCELLED;
    case IMAGE_JOB_FILE:
        return load_file(adapter, job, &work, out);
    case IMAGE_JOB_DIAGRAM:
        return load_diagram(adapter, job, &work, out);
    }
    return IMAGE_FAILED;
}
static bool take_job(struct image_adapter *_Nonnull adapter, struct image_job *_Nonnull job)
{
    AcquireSRWLockExclusive(&adapter->lock);
    bool running = !adapter->stopping;
    *job = adapter->queued;
    adapter->queued = (struct image_job){0};
    ReleaseSRWLockExclusive(&adapter->lock);
    return running;
}
static void publish(struct image_adapter *_Nonnull adapter, const struct image_job *_Nonnull job,
                    enum image_outcome outcome, struct image_bitmap *_Nullable result)
{
    AcquireSRWLockExclusive(&adapter->lock);
    if (!adapter->stopping && adapter->ticket == job->ticket)
    {
        adapter->outcome = outcome;
        adapter->result = result;
        result = nullptr;
        adapter->phase = IMAGE_PHASE_IDLE;
    }
    ReleaseSRWLockExclusive(&adapter->lock);
    image_bitmap_destroy(result);
}
static void complete_job(struct image_adapter *_Nonnull adapter, struct image_job *_Nonnull job,
                         HRESULT initialized)
{
    struct image_bitmap *_Nullable bitmap = nullptr;
    enum image_outcome outcome = FAILED(initialized) ? IMAGE_FAILED : load(adapter, job, &bitmap);
    publish(adapter, job, outcome, bitmap);
}
static DWORD WINAPI worker(void *_Nullable context)
{
    struct image_adapter *adapter = context;
    HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    for (;;)
    {
        if (WaitForSingleObject(adapter->wake, INFINITE) != WAIT_OBJECT_0)
            break;
        struct image_job job = {0};
        bool running = take_job(adapter, &job);
        if (running && job.kind != IMAGE_JOB_EMPTY)
            complete_job(adapter, &job, initialized);
        image_job_destroy(&job);
        if (!running)
            break;
    }
    if (SUCCEEDED(initialized))
        CoUninitialize();
    return 0;
}
static void cancel(struct image_adapter *_Nonnull adapter)
{
    AcquireSRWLockExclusive(&adapter->lock);
    ++adapter->ticket;
    image_job_destroy(&adapter->queued);
    image_bitmap_destroy(adapter->result);
    adapter->result = nullptr;
    adapter->outcome = IMAGE_CANCELLED;
    adapter->phase = IMAGE_PHASE_IDLE;
    ReleaseSRWLockExclusive(&adapter->lock);
}
static enum image_outcome submit(struct image_adapter *_Nonnull adapter, struct image_job job,
                                 uint64_t *_Nonnull ticket)
{
    cancel(adapter);
    AcquireSRWLockExclusive(&adapter->lock);
    job.ticket = adapter->ticket;
    adapter->queued = job;
    adapter->outcome = IMAGE_PENDING;
    adapter->phase =
        job.kind == IMAGE_JOB_DIAGRAM ? IMAGE_PHASE_DIAGRAM_PARSING : IMAGE_PHASE_READING;
    *ticket = job.ticket;
    ReleaseSRWLockExclusive(&adapter->lock);
    SetEvent(adapter->wake);
    return IMAGE_PENDING;
}
static enum image_outcome request(struct image_adapter *_Nonnull adapter,
                                  const struct image_request *_Nonnull input,
                                  uint64_t *_Nonnull ticket)
{
    struct image_job job = {0};
    enum image_outcome copied = image_job_copy(&job, input);
    return copied == IMAGE_READY ? submit(adapter, job, ticket) : copied;
}
static enum image_outcome diagram(struct image_adapter *_Nonnull adapter,
                                  const struct diagram_request *_Nonnull input,
                                  uint64_t *_Nonnull ticket)
{
    struct image_job job = {0};
    enum image_outcome copied = image_job_diagram(&job, input);
    return copied == IMAGE_READY ? submit(adapter, job, ticket) : copied;
}
static enum image_outcome poll(struct image_adapter *_Nonnull adapter, uint64_t ticket,
                               struct image_bitmap *_Nullable *_Nonnull out)
{
    AcquireSRWLockExclusive(&adapter->lock);
    enum image_outcome outcome = ticket == adapter->ticket ? adapter->outcome : IMAGE_CANCELLED;
    if (outcome == IMAGE_READY)
    {
        *out = adapter->result;
        adapter->result = nullptr;
        adapter->outcome = IMAGE_CANCELLED;
    }
    ReleaseSRWLockExclusive(&adapter->lock);
    return outcome;
}
static enum image_phase phase(struct image_adapter *_Nonnull adapter)
{
    AcquireSRWLockShared(&adapter->lock);
    enum image_phase result = adapter->phase;
    ReleaseSRWLockShared(&adapter->lock);
    return result;
}
enum image_outcome image_adapter_create(const struct storage_path *_Nonnull root,
                                        struct image_adapter *_Nullable *_Nonnull out)
{
    struct image_adapter *_Nullable adapter = calloc(1, sizeof *adapter);
    if (adapter == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    InitializeSRWLock(&adapter->lock);
    adapter->outcome = IMAGE_CANCELLED;
    size_t length = 0;
    if (!storage_native_path_fill(root, adapter->root, &length) ||
        image_cache_create(&adapter->cache) != IMAGE_READY)
    {
        image_adapter_destroy(adapter);
        return IMAGE_OUT_OF_MEMORY;
    }
    adapter->wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (adapter->wake != nullptr)
        adapter->worker = CreateThread(nullptr, 0, worker, adapter, 0, nullptr);
    if (adapter->worker == nullptr)
    {
        image_adapter_destroy(adapter);
        return IMAGE_FAILED;
    }
    *out = adapter;
    return IMAGE_READY;
}
static enum image_outcome asset(struct image_adapter *_Nonnull adapter,
                                const struct image_request *_Nonnull request,
                                enum image_export_policy policy,
                                struct image_asset *_Nullable *_Nonnull out)
{
    struct image_file *_Nullable file = nullptr;
    enum image_outcome result = image_file_read(adapter->root, request, policy, &file);
    if (result == IMAGE_READY)
    {
        struct image_asset_input input = {.bytes = image_file_bytes(file),
                                          .length = image_file_length(file),
                                          .identity = image_file_identity(file),
                                          .relative = image_file_relative(file)};
        result = image_asset_create(&input, out);
    }
    image_file_destroy(file);
    return result;
}
struct image_port image_adapter_port(struct image_adapter *_Nonnull adapter)
{
    return (struct image_port){.adapter = adapter,
                               .request = request,
                               .diagram = diagram,
                               .poll = poll,
                               .phase = phase,
                               .cancel = cancel,
                               .asset = asset};
}
void image_adapter_destroy(struct image_adapter *_Nullable adapter)
{
    if (adapter == nullptr)
        return;
    cancel(adapter);
    AcquireSRWLockExclusive(&adapter->lock);
    adapter->stopping = true;
    ReleaseSRWLockExclusive(&adapter->lock);
    if (adapter->worker != nullptr)
    {
        CancelSynchronousIo(adapter->worker);
        SetEvent(adapter->wake);
        WaitForSingleObject(adapter->worker, INFINITE);
        CloseHandle(adapter->worker);
    }
    if (adapter->wake != nullptr)
        CloseHandle(adapter->wake);
    image_cache_destroy(adapter->cache);
    free(adapter);
}
