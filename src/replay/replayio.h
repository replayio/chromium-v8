#ifndef V8_REPLAY_REPLAYIO_H_
#define V8_REPLAY_REPLAYIO_H_

#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"

namespace v8 {
namespace internal {
class Isolate;
class JSPromise;
class PromiseReactionJobTask;
class String;

// Whether promise hooks, thenable adoptions and reaction jobs report to the
// dependency graph (RecordReplayOnPromiseHook, AddPromiseDependencyGraphAdoption,
// RecordReplayOnPromiseReactionJob), on this thread. Defined in api.cc.
bool RecordReplayReportsPromises();

// Reports the reaction job enqueued for a reaction of the settled promise.
// Defined in debug.cc.
void RecordReplayOnPromiseReactionJob(Isolate* isolate, Handle<JSPromise> settled,
                                      Handle<PromiseReactionJobTask> task);
}  // namespace internal

namespace replayio {

// Whether the current point replays, so that values can be recorded/replayed
// and ids handed out consistently.
bool AreEventsAvailable();

v8::internal::Handle<v8::internal::String> RecordReplayStringHandle(
    const char* why, v8::internal::Isolate* isolate,
    v8::internal::Handle<v8::internal::String> input);

v8::internal::MaybeHandle<v8::internal::String> RecordReplayStringHandle(
    const char* why, v8::internal::Isolate* isolate,
    v8::internal::MaybeHandle<v8::internal::String> input);

}  // namespace replayio
}  // namespace v8

#endif  // V8_REPLAY_REPLAYIO_H_
